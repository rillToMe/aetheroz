# Kyuzen OS - Task & Multitasking Documentation

> **Arsitektur**: Preemptive Multitasking via IRQ0 (Timer)  
> **Header**: `include/task.h`  
> **Implementasi**: `kernel/task.c`, `drivers/timer.c`, `arch/x86/timer_isr.asm`

---

## Cara Kerja Singkat

Kyuzen OS menggunakan **Preemptive Multitasking**. Timer PIT (IRQ0) fires setiap **20ms**, menyimpan seluruh register CPU dari task yang sedang berjalan, lalu melanjutkan task berikutnya. Tidak ada `switch_task()` manual — semua context switch terjadi otomatis via interrupt.

```
Timer IRQ0 (tiap 20ms)
    └─→ timer_isr_stub (ASM)
            ├─ PUSHA64 (simpan semua register ke stack)
            ├─→ timer_handler(rsp)           ← C handler
            │       └─→ schedule(r)           ← pilih task berikutnya
            │               └── return RSP_baru
            ├─ mov rsp, rax                   ← GANTI STACK ke task baru
            ├─ POPA64                          ← restore register task baru
            └─ IRETQ                           ← lompat ke RIP task baru
```

---

## Quick Start — Membuat Task Baru

### 1. Tulis fungsi task kamu

```c
// Di file manapun (kernel/myfeature.c, dll)
#include "task.h"

void my_background_task(void) {
    while (1) {
        // Lakukan sesuatu...
        do_work();

        // Beri tahu timer bahwa kita sedang "idle" (untuk CPU usage tracker)
        // Timer akan preempt kita secara otomatis — yield() hanya opsional hint
        yield();
    }
    // CATATAN: Jangan return! Tidak ada cleanup handler untuk saat ini.
    // Gunakan loop infinite atau tandai state = TASK_DEAD untuk exit.
}
```

### 2. Daftarkan task di `kernel/kernel.c` (atau setelah `tasking_init`)

```c
#include "task.h"

void kmain(void) {
    // ... inisialisasi hardware ...

    tasking_init();  // WAJIB dipanggil sebelum create_task

    // Buat task baru — langsung aktif!
    create_task(my_background_task, "background");
    create_task(another_task,        "another");

    // Lanjutkan kernel utama seperti biasa
    // Timer akan preempt dan switch otomatis
    sti();
    user_shell();  // Task 0 (kmain) terus berjalan
}
```

---

## API Reference

### `tasking_init(void)`
Inisialisasi subsistem tasking. Daftarkan kernel main (kmain) sebagai **Task 0**.  
**Harus dipanggil SEBELUM** `create_task()` dan **SETELAH** `init_timer()`.

```c
tasking_init();
```

---

### `create_task(func, name)`
Buat task baru dengan stack sendiri dan fake ISR frame.

| Parameter | Tipe | Keterangan |
|-----------|------|-----------|
| `func` | `void (*)(void)` | Fungsi entry point task |
| `name` | `const char*` | Nama task untuk debugging (max 15 karakter) |

```c
create_task(my_task_func, "my-task");
```

**Batas:** Maksimum `MAX_TASKS = 8` task secara bersamaan (termasuk kmain).  
**Stack size:** Tiap task mendapat `TASK_STACK_SIZE = 8192` bytes (8KB).

---

### `yield(void)`
Hint bahwa task sedang idle — CPU di-halt sampai interrupt berikutnya.

```c
yield();  // Hemat CPU, tunggu IRQ berikutnya (timer, keyboard, dll)
```

> **Catatan:** Dalam preemptive mode, `yield()` **tidak wajib** dipanggil.  
> Timer akan switch task secara paksa setiap 20ms.  
> Gunakan `yield()` di dalam loop menunggu untuk hemat daya CPU.

---

### `schedule(registers_t* current_regs)` ← Internal
Dipanggil secara otomatis oleh `timer_handler`. **Jangan panggil langsung.**

---

## Task States

```c
#define TASK_READY    0  // Siap dijadwalkan, menunggu giliran
#define TASK_RUNNING  1  // Sedang berjalan di CPU saat ini
#define TASK_SLEEPING 2  // Menunggu event (belum diimplementasi)
#define TASK_DEAD     3  // Selesai, slot bisa di-reuse
```

Untuk menghentikan task dari dalam task itu sendiri:

```c
void my_task(void) {
    do_work();

    // Tandai task ini sebagai mati — scheduler akan skip slot ini
    tasks[current_task].state = TASK_DEAD;

    // Yield selamanya — timer tidak akan kembali ke task ini
    while (1) yield();
}
```

---

## Memeriksa Status Task (Debug)

```c
#include "task.h"

// Lihat semua task yang aktif
for (int i = 0; i < task_count; i++) {
    const char* state_str[] = {"READY", "RUNNING", "SLEEPING", "DEAD"};
    kprintf("Task %d [%s]: %s\n",
        tasks[i].id,
        tasks[i].name,
        state_str[tasks[i].state]);
}

// Lihat task yang sedang berjalan
kprintf("Current task: %d (%s)\n",
    current_task,
    tasks[current_task].name);
```

---

## Contoh Lengkap: Background Counter Task

```c
// kernel/counter_task.c
#include "task.h"
#include "timer.h"

static uint64_t counter = 0;

void counter_task(void) {
    uint64_t last_print = 0;

    while (1) {
        counter++;

        // Print setiap 1 detik (tanpa busy-wait)
        uint64_t now = timer_get_ms();
        if (now - last_print >= 1000) {
            last_print = now;
            kprintf("[counter] %llu\n", counter);
        }

        yield();  // Beri giliran ke task lain, hemat CPU
    }
}
```

```c
// kernel/kernel.c
#include "task.h"

extern void counter_task(void);

void kmain(void) {
    // ... init hardware ...
    tasking_init();
    create_task(counter_task, "counter");
    // Selesai! counter_task berjalan paralel dengan kernel
}
```

---

## Arsitektur Teknis

### Stack Layout (Full ISR Frame)

Setiap task memiliki stack dengan "Fake ISR Frame" di bagian atasnya.  
Scheduler bekerja dengan cara **mengganti RSP** ke ISR frame milik task berikutnya.

```
Stack task (tumbuh ke bawah ↓)
┌─────────────────────────────┐ ← stack_top (alokasi kmalloc)
│  SS         = 0x10          │
│  RSP        = stack_top     │
│  RFLAGS     = 0x202 (IF=1)  │ ← CPU akan IRETQ dari sini
│  CS         = 0x08          │
│  RIP        = &func         │ ← Entry point task
│  error_code = 0             │
│  int_num    = 0             │
│  rax..r15   = 0 (15 regs)  │
└─────────────────────────────┘ ← task.rsp (RSP yang disimpan di TCB)
```

### Quantum

Scheduler dipanggil setiap **20ms** (bisa diubah di `drivers/timer.c`):

```c
// drivers/timer.c
#define SCHEDULER_QUANTUM_MS 20  // Ubah di sini untuk fine-tuning
```

### Batas & Keterbatasan Saat Ini

| Item | Nilai | Catatan |
|------|-------|---------|
| Max tasks | 8 | `MAX_TASKS` di `include/task.h` |
| Stack per task | 8 KB | `TASK_STACK_SIZE` di `include/task.h` |
| Quantum | 20 ms | Di `drivers/timer.c` |
| Ring 3 (user space) | ⚠️ Belum | Semua task saat ini Ring 0 (kernel) |
| Task cleanup/join | ⚠️ Belum | Tandai `TASK_DEAD` secara manual |
| Stack overflow guard | ⚠️ Belum | Jangan allokasi array besar di task |

---

## Files yang Relevan

| File | Fungsi |
|------|--------|
| [`include/task.h`](include/task.h) | Public API, `registers_t`, `task_t`, konstanta |
| [`kernel/task.c`](kernel/task.c) | Implementasi `create_task`, `schedule`, `yield` |
| [`drivers/timer.c`](drivers/timer.c) | `timer_handler` — trigger scheduler tiap 20ms |
| [`arch/x86/timer_isr.asm`](arch/x86/timer_isr.asm) | ISR stub — inti dari context switch |
| [`arch/x86/isr_macro.inc`](arch/x86/isr_macro.inc) | `PUSHA64`/`POPA64` — layout stack frame |
