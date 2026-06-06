// ============================================================
// kernel/task.c — Preemptive Task Scheduler, Kyuzen OS
//
// ARSITEKTUR: Semua context switch via Full ISR Frame.
//   - Timer IRQ0 adalah SATU-SATUNYA trigger context switch.
//   - Setiap task punya stack dengan "Fake ISR Frame" sehingga
//     POPA64 + IRETQ dari timer_isr bisa melanjutkan task tersebut.
//   - switch_task() (cooperative) DIHAPUS.
//
// FLOW:
//   IRQ0 → timer_isr_stub → timer_handler(rsp) → schedule(r)
//        → mov rsp, rax → POPA64 → IRETQ → task baru berjalan
// ============================================================

#include "task.h"
#include "heap.h"
#include <stddef.h>

// ============================================================
// GLOBAL STATE
// ============================================================
task_t tasks[MAX_TASKS];
int    current_task = 0;
int    task_count   = 0;

// String copy helper (tidak bisa include string.h di kernel)
static void task_strncpy(char* dst, const char* src, int n) {
    int i = 0;
    while (i < n - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

// ============================================================
// tasking_init — Daftarkan task 0 (kernel main) sebagai current task
// ============================================================
void tasking_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_DEAD;
        tasks[i].rsp   = 0;
    }

    // Task 0 = kernel main thread yang sedang berjalan
    // RSP-nya akan diisi oleh schedule() pada preemption pertama
    tasks[0].id         = 0;
    tasks[0].state      = TASK_RUNNING;
    tasks[0].stack_base = 0;   // Kernel stack, jangan di-free
    task_strncpy(tasks[0].name, "kmain", 16);

    current_task = 0;
    task_count   = 1;
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
    if (task_count >= MAX_TASKS) return;

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
    *(--p) = (uint64_t)func;    // RIP = entry point fungsi task

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
    *(--p) = 0ULL;   // rdi
    *(--p) = 0ULL;   // r8
    *(--p) = 0ULL;   // r9
    *(--p) = 0ULL;   // r10
    *(--p) = 0ULL;   // r11
    *(--p) = 0ULL;   // r12
    *(--p) = 0ULL;   // r13
    *(--p) = 0ULL;   // r14
    *(--p) = 0ULL;   // r15  ← p sekarang = RSP yang akan disimpan di TCB

    // p sekarang menunjuk ke r15, yang adalah RSP "benar" dari ISR frame ini
    tasks[task_count].id         = (uint32_t)task_count;
    tasks[task_count].rsp        = (uint64_t)p;      // RSP = pointer ke r15 di fake frame
    tasks[task_count].stack_base = (uint64_t)stack;  // Untuk cleanup nanti
    tasks[task_count].state      = TASK_READY;
    task_strncpy(tasks[task_count].name, name ? name : "task", 16);

    task_count++;
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
registers_t* schedule(registers_t* current_regs) {
    // Jika hanya 1 task, tidak perlu switch
    if (task_count <= 1) return current_regs;

    // 1. Simpan RSP task saat ini
    tasks[current_task].rsp   = (uint64_t)current_regs;
    tasks[current_task].state = TASK_READY;

    // 2. Round-robin: cari task READY berikutnya
    int next     = current_task;
    int attempts = 0;
    do {
        next = (next + 1) % task_count;
        attempts++;
        if (attempts > task_count) {
            // Tidak ada task lain yang READY — lanjutkan task saat ini
            tasks[current_task].state = TASK_RUNNING;
            return current_regs;
        }
    } while (tasks[next].state != TASK_READY || tasks[next].rsp == 0);

    // 3. Switch ke task berikutnya
    current_task              = next;
    tasks[current_task].state = TASK_RUNNING;

    return (registers_t*)tasks[current_task].rsp;
}

// ============================================================
// yield — Hint bahwa task sedang idle
//
// Sejak beralih ke Preemptive, yield() tidak lagi melakukan switch
// secara langsung. Context switch HANYA terjadi via timer IRQ0.
// Fungsi ini sekarang hanya mem-block CPU sampai interrupt berikutnya,
// membantu cpu_idle_tracker mendeteksi bahwa task sedang menunggu.
// ============================================================
void yield(void) {
    // hlt: tidurkan CPU sampai interrupt berikutnya (timer/keyboard/mouse)
    // sti: pastikan interrupt enabled dulu sebelum hlt
    __asm__ volatile("sti; hlt");
}