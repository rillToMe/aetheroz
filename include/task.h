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
typedef struct {
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
#define TASK_SLEEPING 2
#define TASK_DEAD     3

#define MAX_TASKS       8
#define TASK_STACK_SIZE 8192  // 8KB per task

// ============================================================
// TASK CONTROL BLOCK (TCB)
// ============================================================
typedef struct {
    uint32_t id;
    uint64_t rsp;             // RSP ke full ISR frame saat task di-preempt
    uint64_t stack_base;      // Untuk kfree saat task mati
    uint8_t  state;           // TASK_READY / TASK_RUNNING / TASK_DEAD
    char     name[16];        // Nama task untuk debugging
    phys_addr_t pml4_phys;    // Physical address of this task's PML4 (0 = kernel shared)
} task_t;


// ============================================================
// PUBLIC API
// ============================================================

void tasking_init(void);
void create_task(void (*func)(void), const char* name);
void task_exit(void) __attribute__((noreturn));
void scheduler_dump(void);
void scheduler_idle_loop(void) __attribute__((noreturn));

// Preemptive scheduler — dipanggil dari timer interrupt
registers_t* schedule(registers_t* current_regs);
registers_t* schedule_on_cpu(uint32_t cpu_id, registers_t* current_regs);

// Yield hint: biarkan CPU idle, timer preempt otomatis via IRQ0
void yield(void);

extern int    task_count;
extern int    current_task;
extern task_t tasks[MAX_TASKS];

#endif // TASK_H
