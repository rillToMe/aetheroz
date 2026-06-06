#include "task.h"
#include "heap.h"

#define MAX_TASKS 4
task_t tasks[MAX_TASKS];
int current_task = 0;
int task_count = 0;

// Panggil fungsi Assembly switch_task (sekarang 64-bit)
extern void switch_task(uint64_t *old_rsp, uint64_t new_rsp);

void tasking_init() {
    tasks[0].active = 1; // Program ke-0 adalah Kernel Utama (Shell)
    current_task = 0;
    task_count = 1;
}

// Buat program baru (Thread)
void create_task(void (*func)()) {
    if (task_count >= MAX_TASKS) return;

    uint64_t* stack = (uint64_t*)kmalloc(4096);
    if (stack == NULL) return;

    // Puncak stack (tumbuh ke bawah)
    uint64_t* top = (uint64_t*)((uint64_t)stack + 4096);

    // Tiru urutan push yang dilakukan switch_task saat dipanggil:
    //   push rbp, push rbx, push r12, push r13, push r14, push r15
    // kemudian ret akan kembali ke func.
    *(--top) = (uint64_t)func; // return address → func
    *(--top) = 0;              // rbp
    *(--top) = 0;              // rbx
    *(--top) = 0;              // r12
    *(--top) = 0;              // r13
    *(--top) = 0;              // r14
    *(--top) = 0;              // r15

    tasks[task_count].rsp    = (uint64_t)top;
    tasks[task_count].active = 1;
    task_count++;
}

// Tukar kendali CPU ke program selanjutnya
void yield() {
    if (task_count <= 1) return; // Single-task: nothing to switch to

    int old_task = current_task;
    current_task = (current_task + 1) % task_count;

    switch_task(&tasks[old_task].rsp, tasks[current_task].rsp);
}