# Kyuzen OS - Task & Multitasking Documentation

> **Arsitektur**: Preemptive Multitasking via IRQ0 (Timer)  
> **Header**: `include/task.h`  
> **Implementasi**: `kernel/task.c`, `drivers/timer.c`, `arch/x86/timer_isr.asm`

---

## Recent Updates: Network Ping & Refresh Rate

### e1000 / Ping Networking

Kyuzen OS sekarang punya jalur networking awal berbasis **Intel e1000 + lwIP**:

```
shell ping
    └─→ sys_ping / kernel_ping()
            └─→ lwIP raw ICMP
                    └─→ kyuzen_netif linkoutput
                            └─→ e1000_send()
```

Perbaikan penting di driver e1000:

- TX/RX descriptor ring dan packet buffer sekarang dialokasikan dari **PMM physical pages**, lalu diakses CPU lewat **HHDM** (`phys + hhdm_offset`).
- Driver tidak lagi memakai `kmalloc()` untuk buffer DMA e1000, karena heap kernel berada di virtual mapping VMM (`0xFFFF9000...`) dan tidak bisa dikonversi benar dengan `virt - hhdm_offset`.
- Bug sebelumnya: NIC menerima alamat DMA palsu, descriptor TX tidak pernah selesai (`DD` tidak balik), lalu log penuh dengan `[e1000] WARN: TX ring full`.
- Setelah fix, test pertama yang disarankan adalah `ping 10.0.2.2` di QEMU user networking. Jika gateway reply, TX/RX e1000 + ARP + ICMP dasar sudah bekerja.

File terkait:

| File | Fungsi |
|------|--------|
| [`drivers/net/e1000/e1000.c`](drivers/net/e1000/e1000.c) | Driver Intel e1000, DMA descriptor ring, TX/RX poll |
| [`drivers/net/lwip/port/kyuzen_netif.c`](drivers/net/lwip/port/kyuzen_netif.c) | Glue layer e1000 ↔ lwIP |
| [`kernel/net_init.c`](kernel/net_init.c) | Init lwIP, netif, DHCP/static fallback, DNS |
| [`kernel/net_ping.c`](kernel/net_ping.c) | ICMP Echo Request/Reply implementation |
| [`apps/shell.c`](apps/shell.c) | Command shell `ping [host]` |

### Runtime Refresh Rate

Default timer/PIT refresh rate diubah dari **50Hz** menjadi **60Hz**. Refresh rate juga bisa diganti dari shell dengan preset awal:

```text
refresh
refresh 60
refresh 100
refresh 144
```

Behavior:

- `refresh` tanpa argumen menampilkan refresh rate aktif.
- `refresh 60`, `refresh 100`, dan `refresh 144` memprogram ulang PIT runtime.
- Nilai lain ditolak agar path awal tetap stabil.
- `timer_get_ms()` sekarang berbasis accumulator runtime, bukan konstanta compile-time, jadi uptime, sleep, lwIP timeout, dan scheduler tetap konsisten saat refresh rate diganti.
- Scheduler quantum tetap berbasis waktu **20ms**, sementara IRQ timer berjalan sesuai refresh rate aktif.

File terkait:

| File | Fungsi |
|------|--------|
| [`include/timer.h`](include/timer.h) | Default 60Hz, API `timer_set_refresh_rate()` dan `timer_get_refresh_rate()` |
| [`drivers/timer.c`](drivers/timer.c) | Program PIT runtime, accumulator waktu ms, CPU usage tracker |
| [`kernel/timer_callbacks.c`](kernel/timer_callbacks.c) | Callback visual flush, cursor, network poll |
| [`apps/shell.c`](apps/shell.c) | Command shell `refresh [60|100|144]` |

---

## Cara Kerja Singkat

Kyuzen OS menggunakan **Preemptive Multitasking**. Timer PIT (IRQ0) default berjalan di **60Hz** (~16.67ms per tick), menyimpan seluruh register CPU dari task yang sedang berjalan, lalu memberi scheduler kesempatan untuk melanjutkan task berikutnya. Tidak ada `switch_task()` manual — semua context switch terjadi otomatis via interrupt.

```
Timer IRQ0 (default 60Hz, bisa 60/100/144)
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
> Timer akan mengecek quantum scheduler berbasis waktu setiap tick.  
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

Scheduler memakai quantum **20ms** (bisa diubah di `drivers/timer.c`). Timer IRQ sendiri berjalan sesuai refresh rate aktif (`60`, `100`, atau `144` Hz):

```c
// drivers/timer.c
#define SCHEDULER_QUANTUM_MS 20  // Ubah di sini untuk fine-tuning
```

### Batas & Keterbatasan Saat Ini

| Item | Nilai | Catatan |
|------|-------|---------|
| Max tasks | 8 | `MAX_TASKS` di `include/task.h` |
| Stack per task | 8 KB | `TASK_STACK_SIZE` di `include/task.h` |
| Timer refresh | 60 / 100 / 144 Hz | Default 60Hz, bisa diubah via shell `refresh` |
| Quantum | 20 ms | Scheduler quantum di `drivers/timer.c` |
| Ring 3 (user space) | ⚠️ Belum | Semua task saat ini Ring 0 (kernel) |
| Task cleanup/join | ⚠️ Belum | Tandai `TASK_DEAD` secara manual |
| Stack overflow guard | ⚠️ Belum | Jangan allokasi array besar di task |

---

## Files yang Relevan

| File | Fungsi |
|------|--------|
| [`include/task.h`](include/task.h) | Public API, `registers_t`, `task_t`, konstanta |
| [`include/timer.h`](include/timer.h) | Timer API, default refresh rate, preset runtime |
| [`kernel/task.c`](kernel/task.c) | Implementasi `create_task`, `schedule`, `yield` |
| [`drivers/timer.c`](drivers/timer.c) | `timer_handler`, PIT runtime refresh, scheduler quantum |
| [`kernel/timer_callbacks.c`](kernel/timer_callbacks.c) | Timer subscribers: visual, cursor, screen flush, network poll |
| [`arch/x86/timer_isr.asm`](arch/x86/timer_isr.asm) | ISR stub — inti dari context switch |
| [`arch/x86/isr_macro.inc`](arch/x86/isr_macro.inc) | `PUSHA64`/`POPA64` — layout stack frame |
