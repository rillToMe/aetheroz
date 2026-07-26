#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include "pmm.h"

// ============================================================
// CPU CONTEXT — Full ISR Frame
//
// Layout ini cocok 1-to-1 dengan stack frame yang dibuat
// oleh isr_macro.inc (PUSHA64) saat IRQ0 fires:
//
//   [RSP+  0]  r15   ← RSP (pointer ke registers_t*)
//   [RSP+  8]  r14
//   [RSP+ 16]  r13
//   [RSP+ 24]  r12
//   [RSP+ 32]  r11
//   [RSP+ 40]  r10
//   [RSP+ 48]  r9
//   [RSP+ 56]  r8
//   [RSP+ 64]  rdi
//   [RSP+ 72]  rsi
//   [RSP+ 80]  rbp
//   [RSP+ 88]  rdx
//   [RSP+ 96]  rcx
//   [RSP+104]  rbx
//   [RSP+112]  rax
//   [RSP+120]  int_num
//   [RSP+128]  error_code
//   [RSP+136]  rip    (CPU auto-push)
//   [RSP+144]  cs
//   [RSP+152]  rflags
//   [RSP+160]  rsp
//   [RSP+168]  ss
//
// JANGAN ubah urutan field! Preemptive scheduler bergantung padanya.
// ============================================================
typedef struct registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t int_num;
    uint64_t error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) registers_t;


// ============================================================
// TASK STATE
// ============================================================
#define TASK_READY    0
#define TASK_RUNNING  1
#define TASK_SLEEPING 2   // Timed block: waiting for wake_at_ms (sleep queue)
#define TASK_DEAD     3
#define TASK_BLOCKED  4   // Untimed block: waiting on an object (wait queue / sync)

#define MAX_TASKS       8
#define TASK_STACK_SIZE 8192  // 8KB per task

// ============================================================
// TASK CONTROL BLOCK (TCB)
// ============================================================
typedef struct task {
    uint32_t id;
    uint64_t rsp;             // RSP ke full ISR frame saat task di-preempt
    uint64_t stack_base;      // Untuk kfree saat task mati
    uint8_t  state;           // TASK_READY / TASK_RUNNING / TASK_DEAD / TASK_SLEEPING / TASK_BLOCKED
    char     name[16];        // Nama task untuk debugging
    phys_addr_t pml4_phys;    // Physical address of this task's PML4 (0 = kernel shared)
    uint32_t cookie;          // Unique per-address-space ID (assigned dynamically by OS)
    uint64_t wake_at_ms;      // TASK_SLEEPING: absolute timer_get_ms() at which to wake (0 = n/a)
    uint8_t  priority;        // Base priority, higher = more important (PRIO_*)
    uint64_t enqueue_ms;      // When this task last entered a run queue (for aging)
    void*    user_stack_base; // kmalloc'd user stack for current app
    void*    deferred_user_stack_base; // Previous app stack, freed on a later syscall
} task_t;

// Priority levels: higher value = scheduled first. Aging boosts long-waiting
// READY tasks so low-priority work cannot starve indefinitely.
#define PRIO_LOW     0
#define PRIO_NORMAL  1
#define PRIO_HIGH    2
#define PRIO_MAX     3


// ============================================================
// PUBLIC API
// ============================================================

void tasking_init(void);
void create_task(void (*func)(void), const char* name);
void create_task_prio(void (*func)(void), const char* name, uint8_t priority);
void task_exit(void) __attribute__((noreturn));
void scheduler_dump(void);
void scheduler_idle_loop(void) __attribute__((noreturn));

// FIX_001: idle stack permanen per-CPU (dialokasikan di tasking_init, tidak
// pernah di-free). task_exit() pindah ke idle stack SEBELUM stack task DEAD
// di-kfree — menutup race UAF stack antara task_exit dan slot reaper di
// create_task. smp_ap_main juga memakainya untuk meninggalkan stack Limine.
uint64_t task_idle_stack_top(uint32_t cpu_id);
void task_switch_to_idle_stack(uint64_t stack_top) __attribute__((noreturn));
void task_exit_via_idle(uint64_t stack_top, void* old_stack_base) __attribute__((noreturn));
void task_exit_finish_on_idle(void* old_stack_base) __attribute__((noreturn));

// Preemptive scheduler — dipanggil dari timer interrupt
registers_t* schedule(registers_t* current_regs);
registers_t* schedule_on_cpu(uint32_t cpu_id, registers_t* current_regs);

// Yield hint: biarkan CPU idle, timer preempt otomatis via IRQ0
void yield(void);

// ============================================================
// VOLUNTARY BLOCKING (Fase 1 — fondasi sleep/wait queue)
//
// block_current_task() menandai task saat ini non-runnable lalu memicu
// context switch via self-IPI reschedule (reschedule_isr.asm melakukan
// `mov rsp, rax`). Task tidak akan dipilih scheduler sampai unblock_task().
//
//   new_state = TASK_SLEEPING (timed, dibangunkan sleep queue) atau
//               TASK_BLOCKED  (untimed, dibangunkan waker eksplisit).
//
// KONTRAK: dipanggil HANYA dari konteks task (bukan dari dalam ISR), dengan
// interrupt enabled. Tidak boleh dipanggil sambil memegang scheduler_lock.
// ============================================================
void block_current_task(uint8_t new_state);

// Split form of block_current_task for wait queues (Fase 2). Lets the caller
// atomically enqueue-then-mark-blocked under its own object lock, closing the
// lost-wakeup window:
//   flags = spinlock_lock_irqsave(&wq->lock);
//   ...enqueue self on wq...
//   int self = block_prepare(TASK_BLOCKED);   // interrupts stay disabled
//   spinlock_unlock_irqrestore(&wq->lock, flags);
//   if (self >= 0) block_park(self);           // park until woken
// block_prepare returns the task id, or -1 if it could not block (no task
// context, or already woken). The caller MUST keep interrupts disabled between
// block_prepare and releasing its object lock.
int  block_prepare(uint8_t new_state);
void block_park(int self);

// Bangunkan task yang sedang BLOCKED/SLEEPING: set READY + enqueue + IPI.
// Aman dipanggil dari ISR maupun konteks task. No-op jika task tidak blocked.
void unblock_task(int task_id);

// Tidur non-busy selama `ms` milidetik. Task masuk sleep queue dan
// dibangunkan oleh timer_handler saat timer_get_ms() >= wake_at_ms.
void task_sleep_ms(uint32_t ms);

// Dipanggil dari timer tick: bangunkan semua task tidur yang wake_at_ms-nya
// sudah lewat. Ringan (O(MAX_TASKS)), aman dari konteks interrupt.
void sleepq_check_wakeups(uint64_t now_ms);

// Per-CPU current task ID (SMP-safe). Returns -1 if idle.
int smp_current_task_id(void);

extern int    task_count;
extern int    current_task;    // DEPRECATED: only tracks CPU 0. Use smp_current_task_id().
extern task_t tasks[MAX_TASKS];

#endif // TASK_H
