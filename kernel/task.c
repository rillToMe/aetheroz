// ============================================================
// kernel/task.c — Preemptive Task Scheduler, Kyuzen OS
//
// ARSITEKTUR: Semua context switch via Full ISR Frame.
//   - Context switch dipicu oleh PIT IRQ0 di BSP dan LAPIC timer di AP.
//   - Setiap task punya stack dengan "Fake ISR Frame" sehingga
//     POPA64 + IRETQ dari timer_isr bisa melanjutkan task tersebut.
//   - switch_task() (cooperative) DIHAPUS.
//
// FLOW:
//   Timer ISR → timer_handler/lapic_timer_handler(rsp) → schedule*(r)
//        → mov rsp, rax → POPA64 → IRETQ → task baru berjalan
// ============================================================

#include "task.h"
#include "heap.h"
#include "spinlock.h"
#include "smp.h"
#include "lapic.h"
#include "paging.h"
#include "timer.h"
#include "vfs.h"
#include <stddef.h>

// ============================================================
// GLOBAL STATE
// ============================================================
task_t tasks[MAX_TASKS];
int    current_task = 0;
int    task_count   = 0;

// Guards TCB slot allocation, task_count, and per-task metadata teardown.
// NOT taken on the scheduling hot path — see cpu_current_task note below.
static spinlock_t scheduler_lock = SPINLOCK_INIT;

// Per-CPU "currently running task" id (-1 = idle). Each entry is written only
// by its own CPU (schedule_on_cpu / task_exit / idle loop) plus one-time init,
// so it needs no cross-CPU lock. Remote reads (dump, panic) are advisory.
static int cpu_current_task[SMP_MAX_CPUS];

// ============================================================
// PER-CPU RUN QUEUES (SMP load balancing)
//
// Each CPU owns a FIFO of READY task ids. A READY task is present in EXACTLY
// ONE run queue; a RUNNING task is in none (tracked by cpu_current_task).
// This single-membership invariant is what prevents a task from being run on
// two CPUs at once: a task can only be scheduled by first dequeuing it under
// the owning queue's lock, which removes it from every queue atomically.
//
// Balancing has two halves:
//   - Placement: create_task enqueues onto the least-loaded CPU (pick_target_cpu).
//   - Work-stealing: an idle CPU pulls a task from the busiest remote queue.
//
// Locking: every queue has its own lock and we never hold two queue locks at
// once (stealing dequeues the victim, then enqueues to self as separate critical
// sections), so the run queues cannot deadlock against each other.
// ============================================================
#define RUNQ_CAPACITY MAX_TASKS   // Never more than MAX_TASKS tasks system-wide.

typedef struct {
    spinlock_t       lock;
    volatile uint32_t count;      // Advisory load metric; aligned 32-bit read is atomic on x86.
    int              entries[RUNQ_CAPACITY];
} run_queue_t;

static run_queue_t cpu_runqueues[SMP_MAX_CPUS];

// Aging: every AGE_STEP_MS a READY task waits adds +1 to its effective priority,
// capped at AGE_MAX_BONUS, so low-priority tasks cannot starve.
#define AGE_STEP_MS    100
#define AGE_MAX_BONUS  8

extern uint64_t timer_get_ms(void);

// Effective priority = base priority + aging bonus from time spent waiting.
static uint32_t effective_prio(int task_id, uint64_t now_ms) {
    uint32_t base = tasks[task_id].priority;
    uint64_t enq  = tasks[task_id].enqueue_ms;
    uint64_t waited = (now_ms > enq) ? (now_ms - enq) : 0;
    uint32_t bonus = (uint32_t)(waited / AGE_STEP_MS);
    if (bonus > AGE_MAX_BONUS) bonus = AGE_MAX_BONUS;
    return base + bonus;
}

static int runq_push(uint32_t cpu_id, int task_id) {
    run_queue_t *rq = &cpu_runqueues[cpu_id];
    uint64_t flags = spinlock_lock_irqsave(&rq->lock);
    if (rq->count >= RUNQ_CAPACITY) {   // Should never happen; fail safe rather than corrupt.
        spinlock_unlock_irqrestore(&rq->lock, flags);
        return -1;
    }
    tasks[task_id].enqueue_ms = timer_get_ms();   // stamp for aging
    rq->entries[rq->count++] = task_id;
    spinlock_unlock_irqrestore(&rq->lock, flags);
    return 0;
}

// Pop the highest effective-priority task; ties break by longest wait (oldest
// enqueue). O(RUNQ_CAPACITY) scan — trivial at MAX_TASKS=8.
static int runq_pop(uint32_t cpu_id) {
    run_queue_t *rq = &cpu_runqueues[cpu_id];
    uint64_t flags = spinlock_lock_irqsave(&rq->lock);
    if (rq->count == 0) {
        spinlock_unlock_irqrestore(&rq->lock, flags);
        return -1;
    }

    uint64_t now = timer_get_ms();
    uint32_t best_i = 0;
    uint32_t best_prio = effective_prio(rq->entries[0], now);
    for (uint32_t i = 1; i < rq->count; i++) {
        uint32_t p = effective_prio(rq->entries[i], now);
        if (p > best_prio ||
            (p == best_prio &&
             tasks[rq->entries[i]].enqueue_ms < tasks[rq->entries[best_i]].enqueue_ms)) {
            best_prio = p;
            best_i = i;
        }
    }

    int task_id = rq->entries[best_i];
    rq->entries[best_i] = rq->entries[--rq->count];   // compact: fill gap with last
    spinlock_unlock_irqrestore(&rq->lock, flags);
    return task_id;
}

// Pick the least-loaded online CPU for a newly-ready task. Load = queued tasks
// plus one if the CPU is currently running something. A fully idle CPU wins
// immediately so fresh work lands where it can start without waiting a quantum.
static uint32_t pick_target_cpu(void) {
    uint32_t online = smp_online_cpu_count();
    if (online > SMP_MAX_CPUS) online = SMP_MAX_CPUS;
    if (online == 0) return 0;

    uint32_t best_cpu  = 0;
    uint32_t best_load = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < online; i++) {
        if (cpu_current_task[i] < 0 && cpu_runqueues[i].count == 0) {
            return i;   // Idle core: schedule here right away.
        }
        uint32_t load = cpu_runqueues[i].count + (cpu_current_task[i] >= 0 ? 1u : 0u);
        if (load < best_load) {
            best_load = load;
            best_cpu  = i;
        }
    }
    return best_cpu;
}

// Work-stealing: pull one task from the busiest remote run queue. Only steals a
// genuinely waiting task (queue length >= 1); the victim's running task is never
// touched. Returns a task id, or -1 if no remote CPU has waiting work.
static int steal_task(uint32_t self_cpu) {
    uint32_t online = smp_online_cpu_count();
    if (online > SMP_MAX_CPUS) online = SMP_MAX_CPUS;

    int      victim = -1;
    uint32_t best   = 0;
    for (uint32_t i = 0; i < online; i++) {
        if (i == self_cpu) continue;
        uint32_t c = cpu_runqueues[i].count;   // Advisory: victim may change before we lock.
        if (c > best) {
            best   = c;
            victim = (int)i;
        }
    }
    if (victim < 0) return -1;

    return runq_pop((uint32_t)victim);   // -1 if the queue emptied out before we locked it.
}

extern void kprint(const char* str);
extern void kprint_num(uint64_t num);
extern uint64_t hhdm_offset;

// String copy helper (tidak bisa include string.h di kernel)
static void task_strncpy(char* dst, const char* src, int n) {
    int i = 0;
    while (i < n - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static const char* task_state_name(uint8_t state) {
    switch (state) {
        case TASK_READY: return "READY";
        case TASK_RUNNING: return "RUNNING";
        case TASK_SLEEPING: return "SLEEPING";
        case TASK_DEAD: return "DEAD";
        case TASK_BLOCKED: return "BLOCKED";
        default: return "UNKNOWN";
    }
}

static void task_entry_trampoline(void (*func)(void)) {
    if (func != NULL) {
        func();
    }
    task_exit();
}

// ============================================================
// tasking_init — Daftarkan task 0 (kernel main) sebagai current task
// ============================================================
void tasking_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_DEAD;
        tasks[i].rsp   = 0;
        tasks[i].pml4_phys = PHYS_NULL;
        tasks[i].cookie = 0;
        tasks[i].wake_at_ms = 0;
        tasks[i].priority = PRIO_NORMAL;
        tasks[i].enqueue_ms = 0;
    }

    for (int i = 0; i < SMP_MAX_CPUS; i++) {
        cpu_current_task[i] = -1;

        run_queue_t *rq = &cpu_runqueues[i];
        rq->lock.locked = 0;
        rq->count = 0;
    }

    // Task 0 = kernel main thread yang sedang berjalan
    // RSP-nya akan diisi oleh schedule() pada preemption pertama
    tasks[0].id         = 0;
    tasks[0].state      = TASK_RUNNING;
    tasks[0].stack_base = 0;   // Kernel stack, jangan di-free
    tasks[0].pml4_phys  = PHYS_NULL;  // Uses boot PML4
    tasks[0].cookie     = 0;          // Kernel task: no cookie
    tasks[0].wake_at_ms = 0;
    tasks[0].priority   = PRIO_NORMAL;
    tasks[0].enqueue_ms = 0;
    task_strncpy(tasks[0].name, "kmain", 16);

    current_task = 0;
    task_count   = 1;
    cpu_current_task[0] = 0;

    // Record boot CR3 for percpu tracking
    phys_addr_t boot_cr3 = vmm_read_cr3();
    percpu_t *bsp = smp_get_cpu(0);
    if (bsp != NULL) {
        bsp->current_cr3 = (uint64_t)boot_cr3;
    }
}

// ============================================================
// create_task — Buat task baru dengan Fake ISR Frame
//
// Fake ISR Frame adalah representasi dari "seolah-olah task ini
// baru saja di-interrupt oleh IRQ0". Saat scheduler memilih task ini,
// POPA64 + IRETQ akan me-restore frame ini dan melompat ke func.
//
// Layout Fake ISR Frame (dari TINGGI ke RENDAH, dibangun dari stack_top turun):
//   stack_top → [SS][RSP][RFLAGS][CS][RIP][error_code][int_num]
//               [rax][rbx][rcx][rdx][rbp][rsi][rdi][r8..r15]
//                                                         ↑ RSP task (frame pointer)
// ============================================================
void create_task(void (*func)(void), const char* name) {
    create_task_prio(func, name, PRIO_NORMAL);
}

void create_task_prio(void (*func)(void), const char* name, uint8_t priority) {
    uint32_t kick_cpus[SMP_MAX_CPUS];
    uint32_t kick_count = 0;

    // Alokasi stack baru
    uint8_t* stack = (uint8_t*)kmalloc(TASK_STACK_SIZE);
    if (!stack) return;

    uint64_t stack_top = (uint64_t)stack + TASK_STACK_SIZE;

    // Zero out stack untuk keamanan
    for (int i = 0; i < TASK_STACK_SIZE; i++) stack[i] = 0;

    // Bangun Fake ISR Frame dari atas stack ke bawah
    // Setiap `*(--p)` = PUSH (decrement pointer, lalu isi nilai)
    uint64_t* p = (uint64_t*)stack_top;

    // ── CPU auto-push (5 slot, urutan tinggi→rendah) ──
    *(--p) = 0x10ULL;            // SS  = kernel data segment
    *(--p) = stack_top;          // RSP = top of this task's stack (setelah iretq)
    *(--p) = 0x202ULL;           // RFLAGS: bit 1 (reserved=1) + IF=1 (interrupt enabled)
    *(--p) = 0x08ULL;            // CS  = kernel code segment
    *(--p) = (uint64_t)task_entry_trampoline; // RIP = wrapper aman untuk task

    // ── ISR stub push (2 slot) ──
    *(--p) = 0ULL;               // error_code (stub push 0 PERTAMA = higher addr)
    *(--p) = 0ULL;               // int_num    (stub push int KEDUA = lower addr)

    // ── PUSHA64 order (15 slot): rax PERTAMA = highest, r15 TERAKHIR = RSP ──
    // Urutan harus cocok dengan PUSHA64 macro: push rax, rbx, rcx, rdx, rbp, rsi, rdi, r8..r15
    // (rax pushed first = ends up at RSP+112 = highest, r15 pushed last = RSP+0 = lowest)
    *(--p) = 0ULL;   // rax  (akan ada di [RSP+112] — lokasi return value syscall)
    *(--p) = 0ULL;   // rbx
    *(--p) = 0ULL;   // rcx
    *(--p) = 0ULL;   // rdx
    *(--p) = 0ULL;   // rbp
    *(--p) = 0ULL;   // rsi
    *(--p) = (uint64_t)func; // rdi = argumen pertama task_entry_trampoline
    *(--p) = 0ULL;   // r8
    *(--p) = 0ULL;   // r9
    *(--p) = 0ULL;   // r10
    *(--p) = 0ULL;   // r11
    *(--p) = 0ULL;   // r12
    *(--p) = 0ULL;   // r13
    *(--p) = 0ULL;   // r14
    *(--p) = 0ULL;   // r15  ← p sekarang = RSP yang akan disimpan di TCB

    uint64_t flags = spinlock_lock_irqsave(&scheduler_lock);

    int slot = -1;
    for (int i = 1; i < task_count; i++) {
        if (tasks[i].state == TASK_DEAD) {
            slot = i;
            break;
        }
    }

    if (slot < 0 && task_count < MAX_TASKS) {
        slot = task_count++;
    }

    if (slot < 0) {
        spinlock_unlock_irqrestore(&scheduler_lock, flags);
        kfree(stack);
        return;
    }

    if (tasks[slot].stack_base != 0) {
        kfree((void*)tasks[slot].stack_base);
    }

    // p sekarang menunjuk ke r15, yang adalah RSP "benar" dari ISR frame ini
    tasks[slot].id         = (uint32_t)slot;
    tasks[slot].rsp        = (uint64_t)p;      // RSP = pointer ke r15 di fake frame
    tasks[slot].stack_base = (uint64_t)stack;  // Untuk cleanup nanti
    tasks[slot].state      = TASK_READY;
    tasks[slot].pml4_phys  = PHYS_NULL;        // Kernel task: shared PML4
    tasks[slot].cookie     = 0;
    tasks[slot].wake_at_ms = 0;
    tasks[slot].priority   = priority;
    task_strncpy(tasks[slot].name, name ? name : "task", 16);

    spinlock_unlock_irqrestore(&scheduler_lock, flags);

    // Place the new task on the least-loaded CPU and wake only that one CPU.
    // This replaces the old broadcast-to-all-idle-cores wakeup, which made
    // every idle AP spin on the scheduler lock only for one to win the task.
    uint32_t target = pick_target_cpu();
    if (runq_push(target, slot) != 0) {
        // Queue full (only possible under MAX_TASKS overflow). Reclaim the slot.
        uint64_t reclaim = spinlock_lock_irqsave(&scheduler_lock);
        tasks[slot].state = TASK_DEAD;
        tasks[slot].rsp   = 0;
        spinlock_unlock_irqrestore(&scheduler_lock, reclaim);
        kfree(stack);
        return;
    }

    // Only a remote CPU needs an IPI to preempt promptly; CPU 0 (BSP) picks the
    // task up on its own next PIT tick, and the local AP does so on its LAPIC tick.
    if (target != smp_current_cpu_index()) {
        kick_cpus[kick_count++] = target;
    }

    for (uint32_t i = 0; i < kick_count; i++) {
        smp_mark_reschedule(kick_cpus[i]);
        lapic_send_reschedule(kick_cpus[i]);
    }
}

void scheduler_dump(void) {
    task_t task_snapshot[MAX_TASKS];
    int cpu_snapshot[SMP_MAX_CPUS];
    percpu_t per_cpu_snapshot[SMP_MAX_CPUS];
    int snapshot_count;
    int active_count = 0;

    uint64_t flags = spinlock_lock_irqsave(&scheduler_lock);

    snapshot_count = task_count;
    for (int i = 0; i < MAX_TASKS; i++) {
        task_snapshot[i] = tasks[i];
    }
    for (int i = 0; i < SMP_MAX_CPUS; i++) {
        cpu_snapshot[i] = cpu_current_task[i];
        percpu_t *cpu = smp_get_cpu((uint32_t)i);
        if (cpu != NULL) {
            per_cpu_snapshot[i] = *cpu;
        }
    }

    spinlock_unlock_irqrestore(&scheduler_lock, flags);

    for (int i = 0; i < snapshot_count && i < MAX_TASKS; i++) {
        if (task_snapshot[i].state != TASK_DEAD) {
            active_count++;
        }
    }

    kprint("[sched] tasks=");
    kprint_num((uint64_t)active_count);
    kprint("/");
    kprint_num((uint64_t)snapshot_count);
    kprint(", online_cpus=");
    kprint_num((uint64_t)smp_online_cpu_count());
    kprint("\n");

    for (int i = 0; i < snapshot_count && i < MAX_TASKS; i++) {
        kprint("  task ");
        kprint_num((uint64_t)task_snapshot[i].id);
        kprint("  ");
        kprint(task_state_name(task_snapshot[i].state));
        kprint("  prio=");
        kprint_num((uint64_t)task_snapshot[i].priority);
        kprint("  ");
        kprint(task_snapshot[i].name);
        kprint("\n");
    }

    kprint("  cpu map:");
    uint32_t online = smp_online_cpu_count();
    if (online > SMP_MAX_CPUS) online = SMP_MAX_CPUS;
    for (uint32_t i = 0; i < online; i++) {
        kprint(" cpu");
        kprint_num((uint64_t)i);
        kprint("=");
        if (cpu_snapshot[i] >= 0) {
            kprint_num((uint64_t)cpu_snapshot[i]);
        } else {
            kprint("idle");
        }
        kprint("(q=");
        kprint_num((uint64_t)cpu_runqueues[i].count);
        kprint(")");
    }
    kprint("\n");

    for (uint32_t i = 0; i < online; i++) {
        kprint("  cpu");
        kprint_num((uint64_t)i);
        kprint(" lapic=");
        kprint_num((uint64_t)per_cpu_snapshot[i].lapic_id);
        kprint(" sched_ticks=");
        kprint_num((uint64_t)per_cpu_snapshot[i].scheduler_ticks);
        kprint(" idle_ticks=");
        kprint_num((uint64_t)per_cpu_snapshot[i].idle_ticks);
        if (per_cpu_snapshot[i].reschedule_pending) {
            kprint(" resched=pending");
        }
        kprint("\n");
    }
}

void task_exit(void) {
    uint32_t cpu_id = smp_current_cpu_index();

    // Flush and release any open fds before tearing the task down. Done outside
    // scheduler_lock: vfs has its own lock and must not nest under it.
    vfs_close_all(smp_current_task_id());

    uint64_t flags = spinlock_lock_irqsave(&scheduler_lock);

    if (cpu_id < SMP_MAX_CPUS) {
        int cur = cpu_current_task[cpu_id];
        if (cur > 0 && cur < task_count) {
            // If task has a private address space, destroy it
            if (tasks[cur].pml4_phys != PHYS_NULL) {
                // Switch to kernel PML4 (safe — uses saved boot PML4)
                vmm_switch_to_kernel_as();

                // Must unlock before destroy (it acquires paging_lock)
                tasks[cur].state = TASK_DEAD;
                tasks[cur].rsp = 0;
                phys_addr_t dead_pml4 = tasks[cur].pml4_phys;
                tasks[cur].pml4_phys = PHYS_NULL;
                cpu_current_task[cpu_id] = -1;

                spinlock_unlock_irqrestore(&scheduler_lock, flags);
                vmm_destroy_address_space(dead_pml4, 1);
                scheduler_idle_loop();
            }

            tasks[cur].state = TASK_DEAD;
            tasks[cur].rsp = 0;
        }
        cpu_current_task[cpu_id] = -1;
    }

    spinlock_unlock_irqrestore(&scheduler_lock, flags);

    scheduler_idle_loop();
}

void scheduler_idle_loop(void) {
    for (;;) {
        uint32_t cpu_id = smp_current_cpu_index();

        uint64_t flags = spinlock_lock_irqsave(&scheduler_lock);
        if (cpu_id < SMP_MAX_CPUS) {
            cpu_current_task[cpu_id] = -1;
        }
        spinlock_unlock_irqrestore(&scheduler_lock, flags);

        smp_note_idle_tick(cpu_id);
        __asm__ volatile("sti; hlt");
    }
}

// ============================================================
// schedule_on_cpu — Per-CPU Run Queue Scheduler with Work-Stealing
//
// Dipanggil dari PIT (BSP) atau LAPIC timer (AP) setiap ~20ms, dan dari
// reschedule IPI saat CPU lain menaruh pekerjaan baru untuk CPU ini.
//
// Pemilihan task berikutnya (urutan prioritas):
//   1. Run queue lokal CPU ini (FIFO round-robin di dalam satu core).
//   2. Work-stealing: ambil satu task dari run queue remote yang paling sibuk.
//
// Urutan operasi menjaga invarian "satu task hanya di satu queue" dan mencegah
// sebuah task berjalan di dua CPU sekaligus:
//   - `next` di-POP lebih dulu (dihapus dari queue → kepemilikan eksklusif).
//   - Baru setelah `next` aman, task keluar (`cur`) disimpan konteksnya dan
//     di-PUSH kembali ke queue lokal supaya CPU lain boleh mencurinya.
//
// Input : registers_t* = RSP task yang sedang di-interrupt (full ISR frame)
// Output: registers_t* = RSP task berikutnya (akan di-load ke RSP di ASM)
// ============================================================
registers_t* schedule_on_cpu(uint32_t cpu_id, registers_t* current_regs) {
    if (cpu_id >= SMP_MAX_CPUS || current_regs == NULL) return current_regs;

    smp_note_scheduler_tick(cpu_id);
    smp_clear_reschedule(cpu_id);

    spinlock_lock(&scheduler_lock);

    if (task_count <= 0) {
        spinlock_unlock(&scheduler_lock);
        return current_regs;
    }

    int cur = cpu_current_task[cpu_id];
    int cur_live    = (cur >= 0 && cur < task_count);
    uint8_t cur_st  = cur_live ? tasks[cur].state : (uint8_t)TASK_DEAD;
    int cur_running = cur_live && (cur_st == TASK_RUNNING);
    // Voluntarily blocked (task_sleep_ms / block_current_task): its frame must be
    // preserved so unblock can resume it, but it must NOT go back on a run queue.
    int cur_blocked = cur_live && (cur_st == TASK_SLEEPING || cur_st == TASK_BLOCKED);
    if (!cur_running && !cur_blocked) {
        // Dead or invalid: relinquish the slot entirely.
        cur = -1;
        cpu_current_task[cpu_id] = -1;
    }

    // Secure the next task BEFORE releasing the current one. Popping removes the
    // task from its queue, giving this CPU exclusive ownership — no other CPU
    // can select it, so it can never run on two cores at once.
    int next = runq_pop(cpu_id);
    if (next < 0) {
        next = steal_task(cpu_id);
    }

    if (next < 0) {
        // Nothing else is runnable anywhere.
        //  - If current is a blocked task, save its resume frame and keep it as
        //    this CPU's current: it will hlt-loop in block_current_task until
        //    unblock flips it back to RUNNING (no double-run — it's in no queue).
        //  - If current is still RUNNING, leave it on-CPU untouched.
        if (cur_blocked) {
            tasks[cur].rsp = (uint64_t)current_regs;
        }
        spinlock_unlock(&scheduler_lock);
        return current_regs;
    }

    // A replacement is secured. Save the outgoing task's context. A RUNNING task
    // goes back on a run queue (stealable); a blocked task keeps its frame but is
    // left off every queue (only unblock_task may make it runnable again).
    if (cur_blocked) {
        tasks[cur].rsp = (uint64_t)current_regs;
    }
    if (cur_running) {
        tasks[cur].rsp   = (uint64_t)current_regs;
        tasks[cur].state = TASK_READY;
        if (runq_push(cpu_id, cur) != 0) {
            // The local queue always has room for the outgoing task here (total
            // live tasks <= MAX_TASKS and `next` is currently out of every
            // queue), so this is unreachable. Fail safe: keep the current task
            // running and defer the switch rather than drop a runnable task.
            tasks[cur].state = TASK_RUNNING;
            (void)runq_push(cpu_id, next);
            spinlock_unlock(&scheduler_lock);
            return current_regs;
        }
    }

    if (next >= task_count || tasks[next].state != TASK_READY || tasks[next].rsp == 0) {
        // Stale queue entry (task died between enqueue and now). Skip the switch.
        spinlock_unlock(&scheduler_lock);
        return current_regs;
    }

    tasks[next].state = TASK_RUNNING;
    cpu_current_task[cpu_id] = next;
    if (cpu_id == 0) {
        current_task = next;
    }

    // ── CR3 SWITCH: Load next task's address space ──
    // Only CR3 changes. current_pml4 ALWAYS stays as kernel PML4.
    // User-range isolation is handled by the per-process PML4 loaded into CR3.
    {
        percpu_t *cpu = smp_get_cpu(cpu_id);

        if (tasks[next].pml4_phys != PHYS_NULL) {
            // User task: load its private PML4 into CR3
            uint64_t new_cr3 = (uint64_t)tasks[next].pml4_phys;
            if (cpu == NULL || cpu->current_cr3 != new_cr3) {
                __asm__ volatile("mov %0, %%cr3" :: "r"(new_cr3) : "memory");
                if (cpu) cpu->current_cr3 = new_cr3;
            }
        } else {
            // Kernel task: ensure kernel PML4 is in CR3
            phys_addr_t kern_phys = vmm_get_kernel_pml4_phys();
            if (kern_phys != PHYS_NULL && (cpu == NULL || cpu->current_cr3 != (uint64_t)kern_phys)) {
                __asm__ volatile("mov %0, %%cr3" :: "r"((uint64_t)kern_phys) : "memory");
                if (cpu) cpu->current_cr3 = (uint64_t)kern_phys;
            }
        }
    }

    registers_t *next_regs = (registers_t*)tasks[next].rsp;
    spinlock_unlock(&scheduler_lock);
    return next_regs;
}

registers_t* schedule(registers_t* current_regs) {
    return schedule_on_cpu(0, current_regs);
}

// ============================================================
// VOLUNTARY BLOCKING (Fase 1) — block_current_task / unblock_task
//
// Model: task menandai dirinya non-runnable (TASK_SLEEPING/TASK_BLOCKED) lalu
// memicu reschedule pass. schedule_on_cpu() menyimpan frame-nya (titik resume)
// tapi TIDAK mengembalikannya ke run queue. Task tetap non-runnable sampai
// unblock_task() membuatnya READY/RUNNING lagi.
//
// KEBENARAN TIDAK BERGANTUNG PADA IPI: timer tick periodik juga menjalankan
// scheduler, jadi IPI yang hilang hanya menambah latensi, bukan menggantung.
//
// State machine (di bawah scheduler_lock):
//   RUNNING --block--> SLEEPING/BLOCKED --unblock--> READY (enqueue) / RUNNING (parked)
// ============================================================

// Volatile read of a task's state — dipakai loop block tanpa memegang lock.
// Byte-aligned read atomic di x86; barrier mencegah compiler meng-cache-nya.
static inline uint8_t task_state_volatile(int id) {
    return *(volatile uint8_t*)&tasks[id].state;
}

// block_prepare — mark the current task non-runnable, WITHOUT parking yet.
// Returns the task id on success, or -1 if there is no schedulable task context
// or the caller is not actually the running task.
//
// Split from block_park so a wait queue can atomically (under ITS lock, with
// interrupts disabled) enqueue the waiter AND mark it blocked before releasing
// the lock — closing the lost-wakeup window. The caller MUST keep interrupts
// disabled between block_prepare and releasing its object lock, otherwise a
// timer tick could deschedule the task while it still holds that lock.
int block_prepare(uint8_t new_state) {
    if (new_state != TASK_SLEEPING && new_state != TASK_BLOCKED) return -1;

    uint32_t cpu_id = smp_current_cpu_index();
    int self = (cpu_id < SMP_MAX_CPUS) ? cpu_current_task[cpu_id] : -1;
    if (self < 0 || self >= task_count) {
        return -1;   // No schedulable task context (idle/boot).
    }

    uint64_t flags = spinlock_lock_irqsave(&scheduler_lock);
    if (tasks[self].state != TASK_RUNNING) {
        // Not the running task (already blocked/woken). Caller should not park.
        spinlock_unlock_irqrestore(&scheduler_lock, flags);
        return -1;
    }
    tasks[self].state = new_state;
    spinlock_unlock_irqrestore(&scheduler_lock, flags);
    return self;
}

// block_park — spin until unblock flips this task back to RUNNING. Each iteration
// forces a reschedule on this CPU: the first pass switches us away (frame saved);
// once descheduled the loop body is frozen and only resumes after we are made
// runnable and re-selected — at which point state is already RUNNING.
// `self` must be the id returned by a preceding block_prepare().
void block_park(int self) {
    if (self < 0 || self >= MAX_TASKS) return;
    uint32_t cpu_id = smp_current_cpu_index();
    while (task_state_volatile(self) != TASK_RUNNING) {
        smp_mark_reschedule(cpu_id);
        lapic_send_reschedule(cpu_id);
        __asm__ volatile("sti; hlt" ::: "memory");
    }
}

void block_current_task(uint8_t new_state) {
    int self = block_prepare(new_state);
    if (self < 0) return;   // couldn't block (no context, or already woken)
    block_park(self);
}

void unblock_task(int task_id) {
    if (task_id < 0 || task_id >= MAX_TASKS) return;

    uint64_t flags = spinlock_lock_irqsave(&scheduler_lock);
    uint8_t st = tasks[task_id].state;
    if (st != TASK_SLEEPING && st != TASK_BLOCKED) {
        // Not blocked (already running/ready/dead). No-op keeps unblock idempotent
        // and race-safe against a concurrent wakeup.
        spinlock_unlock_irqrestore(&scheduler_lock, flags);
        return;
    }

    // Is the task still parked on a CPU (spinning its own block loop, off every
    // run queue)? If so we must NOT enqueue it — that would let a second CPU run
    // the same stack. Just flip it RUNNING and kick that CPU out of hlt.
    int on_cpu = -1;
    for (uint32_t i = 0; i < SMP_MAX_CPUS; i++) {
        if (cpu_current_task[i] == task_id) { on_cpu = (int)i; break; }
    }

    tasks[task_id].wake_at_ms = 0;

    if (on_cpu >= 0) {
        tasks[task_id].state = TASK_RUNNING;   // parked CPU resumes & exits its loop
        spinlock_unlock_irqrestore(&scheduler_lock, flags);
        smp_mark_reschedule((uint32_t)on_cpu);
        lapic_send_reschedule((uint32_t)on_cpu);
        return;
    }

    // Fully descheduled: mark READY and hand to a run queue. The scheduler sets it
    // RUNNING before resuming its frame, so its block loop sees RUNNING and exits.
    tasks[task_id].state = TASK_READY;
    spinlock_unlock_irqrestore(&scheduler_lock, flags);

    uint32_t target = pick_target_cpu();
    if (runq_push(target, task_id) != 0) {
        // Queue full (MAX_TASKS overflow, unreachable in practice). Re-park as
        // BLOCKED so a later unblock/timer retry can pick it up rather than lose it.
        uint64_t re = spinlock_lock_irqsave(&scheduler_lock);
        if (tasks[task_id].state == TASK_READY) tasks[task_id].state = TASK_BLOCKED;
        spinlock_unlock_irqrestore(&scheduler_lock, re);
        return;
    }
    if (target != smp_current_cpu_index()) {
        smp_mark_reschedule(target);
        lapic_send_reschedule(target);
    }
}

// ============================================================
// SLEEP QUEUE — task_sleep_ms / sleepq_check_wakeups
//
// Implementasi ringan: state per-task (wake_at_ms) + scan O(MAX_TASKS) di timer
// tick. MAX_TASKS kecil (8) sehingga scan lebih murah & sederhana daripada
// linked list terurut, tanpa alokasi.
// ============================================================
void task_sleep_ms(uint32_t ms) {
    if (ms == 0) { yield(); return; }

    uint64_t target = timer_get_ms() + ms;

    uint32_t cpu_id = smp_current_cpu_index();
    int self = (cpu_id < SMP_MAX_CPUS) ? cpu_current_task[cpu_id] : -1;
    if (self < 0 || self >= task_count) {
        // No descheduable task context (early boot / pure idle): fall back to a
        // non-busy halt-wait. Still not a spin — CPU sleeps between interrupts.
        while (timer_get_ms() < target) __asm__ volatile("sti; hlt");
        return;
    }

    uint64_t flags = spinlock_lock_irqsave(&scheduler_lock);
    tasks[self].wake_at_ms = target;
    spinlock_unlock_irqrestore(&scheduler_lock, flags);

    block_current_task(TASK_SLEEPING);
}

void sleepq_check_wakeups(uint64_t now_ms) {
    // Runs in timer-interrupt context. Collect due sleepers under the scheduler
    // lock, then unblock them OUTSIDE the lock (unblock_task takes scheduler_lock
    // and run-queue locks — must not nest here).
    int wake[MAX_TASKS];
    int n = 0;

    uint64_t flags = spinlock_lock_irqsave(&scheduler_lock);
    for (int i = 0; i < MAX_TASKS && i < task_count; i++) {
        if (tasks[i].state == TASK_SLEEPING &&
            tasks[i].wake_at_ms != 0 &&
            now_ms >= tasks[i].wake_at_ms) {
            wake[n++] = i;
        }
    }
    spinlock_unlock_irqrestore(&scheduler_lock, flags);

    for (int i = 0; i < n; i++) {
        unblock_task(wake[i]);   // re-validates state under lock; safe if it raced
    }
}


// ============================================================
// smp_current_task_id — Per-CPU current task ID
//
// Returns the task ID running on the CURRENT CPU.
// Returns -1 if the CPU is idle (no task scheduled).
// SMP-safe: reads per-CPU state, not the global current_task.
// ============================================================
int smp_current_task_id(void) {
    uint32_t cpu_id = smp_current_cpu_index();
    if (cpu_id >= SMP_MAX_CPUS) return -1;
    return cpu_current_task[cpu_id];
}

// ============================================================
// yield — Hint bahwa task sedang idle
//
// Sejak beralih ke Preemptive, yield() tidak lagi melakukan switch
// secara langsung. Context switch terjadi via timer interrupt.
// Fungsi ini sekarang hanya mem-block CPU sampai interrupt berikutnya,
// membantu cpu_idle_tracker mendeteksi bahwa task sedang menunggu.
// ============================================================
void yield(void) {
    // hlt: tidurkan CPU sampai interrupt berikutnya (timer/keyboard/mouse)
    // sti: pastikan interrupt enabled dulu sebelum hlt
    __asm__ volatile("sti; hlt");
}



