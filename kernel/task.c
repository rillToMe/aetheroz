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
#include <stddef.h>

// ============================================================
// GLOBAL STATE
// ============================================================
task_t tasks[MAX_TASKS];
int    current_task = 0;
int    task_count   = 0;

static spinlock_t scheduler_lock = SPINLOCK_INIT;
static int cpu_current_task[SMP_MAX_CPUS];

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
    }

    for (int i = 0; i < SMP_MAX_CPUS; i++) {
        cpu_current_task[i] = -1;
    }

    // Task 0 = kernel main thread yang sedang berjalan
    // RSP-nya akan diisi oleh schedule() pada preemption pertama
    tasks[0].id         = 0;
    tasks[0].state      = TASK_RUNNING;
    tasks[0].stack_base = 0;   // Kernel stack, jangan di-free
    tasks[0].pml4_phys  = PHYS_NULL;  // Uses boot PML4
    tasks[0].cookie     = 0;          // Kernel task: no cookie
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
    task_strncpy(tasks[slot].name, name ? name : "task", 16);

    uint32_t online = smp_online_cpu_count();
    if (online > SMP_MAX_CPUS) online = SMP_MAX_CPUS;
    for (uint32_t i = 1; i < online; i++) {
        if (cpu_current_task[i] < 0) {
            kick_cpus[kick_count++] = i;
        }
    }

    spinlock_unlock_irqrestore(&scheduler_lock, flags);

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
// schedule — Round-Robin Preemptive Scheduler
//
// Dipanggil dari timer_handler setiap ~20ms.
// Menyimpan konteks task saat ini dan memuat konteks task berikutnya.
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

    if (cur >= 0 && cur < task_count && tasks[cur].state == TASK_RUNNING) {
        tasks[cur].rsp = (uint64_t)current_regs;
        tasks[cur].state = TASK_READY;
    } else {
        cur = -1;
        cpu_current_task[cpu_id] = -1;
    }

    int start = (cur >= 0) ? cur : (int)(cpu_id % (uint32_t)task_count);
    int next = -1;

    for (int attempts = 0; attempts < task_count; attempts++) {
        int candidate = (start + 1 + attempts) % task_count;
        if (tasks[candidate].state == TASK_READY && tasks[candidate].rsp != 0) {
            next = candidate;
            break;
        }
    }

    if (next < 0) {
        if (cur >= 0 && cur < task_count) {
            tasks[cur].state = TASK_RUNNING;
        }
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
