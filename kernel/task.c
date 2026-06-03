#include "task.h"
#include "heap.h"

#define MAX_TASKS 4
task_t tasks[MAX_TASKS];
int current_task = 0;
int task_count = 0;

// Panggil fungsi Assembly yang kita buat tadi
extern void switch_task(uint32_t *old_esp, uint32_t new_esp);

void tasking_init() {
    tasks[0].active = 1; // Program ke-0 adalah Kernel Utama (Shell)
    current_task = 0;
    task_count = 1;
}

// Bikin program baru (Thread)
void create_task(void (*func)()) {
    if (task_count >= MAX_TASKS) return;

    // Sewa RAM 4KB khusus untuk stack memori program ini
    uint32_t* stack = (uint32_t*)kmalloc(4096);
    uint32_t* top_of_stack = (uint32_t*)((uint32_t)stack + 4096);

    // Manipulasi stack agar menyerupai perilaku CPU setelah interupsi
    *(--top_of_stack) = (uint32_t)func; // EIP (Titik mulai fungsi)

    // Simulasikan instruksi 'pusha' (8 register general purpose diset 0)
    for (int i = 0; i < 8; i++) {
        *(--top_of_stack) = 0;
    }

    // Daftarkan programnya
    tasks[task_count].esp = (uint32_t)top_of_stack;
    tasks[task_count].active = 1;
    task_count++;
}

// Tukar kendali CPU ke program selanjutnya
void yield() {
    if (task_count <= 1) return; // Kalau cuma 1 program, tidak perlu tukar
    
    int old_task = current_task;
    current_task = (current_task + 1) % task_count; // Berputar (0 -> 1 -> 0 -> 1)
    
    // Panggil fungsi Assembly untuk menukar otak CPU
    switch_task(&tasks[old_task].esp, tasks[current_task].esp);
}