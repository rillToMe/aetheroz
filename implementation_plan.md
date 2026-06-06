# Unified Timer Architecture — Kyuzen OS

## Latar Belakang Masalah

Timer pernah broken dari awal karena **tidak ada satu sumber kebenaran (single source of truth)** untuk waktu.
Setiap komponen punya cara sendiri menghitung waktu dan mengelola interrupt:

---

## Audit Sistem Saat Ini

### Sumber "Waktu" yang Tersebar (Current State)

```
IRQ0 → timer_isr_stub → timer_handler()   [drivers/timer.c]
IRQ1 → keyboard_isr_stub → keyboard_handler()  [drivers/keyboard.c]  ← punya EOI sendiri
IRQ12 → mouse_isr_stub → mouse_handler()       [drivers/mouse.c]      ← punya EOI sendiri
```

### Problem yang Ditemukan

| # | Masalah | File | Detail |
|---|---------|------|--------|
| 1 | **Magic numbers tersebar** | `timer.c`, `login.c`, `syscall.c`, `userlib.c` | `#define TIMER_HZ 50` ada di dalam fungsi (!)  bukan di header global |
| 2 | **`extern uint32_t timer_ticks`** disebar ke mana-mana | `kernel_userlib.c:32`, `syscall.c:54`, `timer.c:6` | Raw variable diakses langsung, tidak ada API |
| 3 | **EOI tersebar di 3 tempat berbeda** | `keyboard.c:79`, `mouse.c:122`, `timer.c:103` | Format tidak konsisten, mudah terlupakan di driver baru |
| 4 | **`compositor_flush()` dipanggil sembarangan** | 8+ tempat | `print()`, `fs_list()`, `timer_handler()`, dll — tidak ada kontrol frame rate |
| 5 | **`sti; hlt` di 5 tempat berbeda** | `kernel_userlib.c:66,76`, `userlib.c:12`, `syscall.c:260` | Setiap developer bisa salah pasang (terbukti menyebabkan freeze) |
| 6 | **Tick-based delay pakai magic counts** | `login.c:102,150,155` | `for(i<100)` dengan komentar `// ~2 detik` — fragile jika TIMER_HZ berubah |
| 7 | **CPU idle tracking broken** | `timer.c:45`, `syscall.c:84` | `yield_counter` heuristik tidak akurat karena `hlt` masih scattered |
| 8 | **`tty_blink_cursor` dipanggil dari timer** | `timer.c:92` | Tightly coupled, tidak bisa di-disable |

---

## Arsitektur yang Diusulkan: Unified Timer Subsystem

### Konsep: One Clock, Many Subscribers

```
PIT IRQ0 (50 Hz)
       │
       ▼
  timer_isr_stub (ASM)
       │
       ▼
  timer_handler()   ← SATU-SATUNYA tempat yang mengelola tick
       │
       ├─── tick++  (global monotonic counter)
       ├─── send EOI
       ├─── loop: registered_callbacks[i](tick)
       │         │
       │         ├── callback: uptime_display()   [visual]
       │         ├── callback: cursor_blink()     [tty]
       │         ├── callback: compositor_flush() [render]
       │         └── callback: cpu_tracker()      [stats]
       └─── done
```

### File yang Akan Dibuat/Dimodifikasi

---

### [NEW] `include/timer.h` — Public Timer API

Header ini jadi satu-satunya titik include untuk semua kebutuhan timing.

```c
#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// ============================================================
// KONSTANTA GLOBAL — single source of truth
// ============================================================
#define TIMER_HZ         50           // Frekuensi PIT
#define TIMER_MS_PER_TICK (1000 / TIMER_HZ)  // 20ms per tick

// Makro delay berbasis tick — TIDAK perlu hitung manual
#define TICKS_PER_SEC    TIMER_HZ
#define TICKS(ms)        ((ms) / TIMER_MS_PER_TICK)  // ms → ticks

// ============================================================
// API PUBLIK
// ============================================================

// Baca uptime dalam ticks (monotonic, tidak pernah wrap dalam sesi normal)
uint32_t timer_get_ticks(void);

// Baca uptime dalam detik
uint32_t timer_get_seconds(void);

// Baca CPU usage (0-100%)
uint32_t timer_get_cpu_usage(void);

// Sleep berbasis tick — MENGGANTIKAN for-loop manual
void timer_sleep_ticks(uint32_t ticks);

// Inisialisasi PIT
void init_timer(uint32_t freq);

// ============================================================
// CALLBACK SYSTEM — subscribe ke setiap tick
// ============================================================
typedef void (*timer_callback_t)(uint32_t tick);

#define TIMER_MAX_CALLBACKS 8

// Daftarkan fungsi yang akan dipanggil setiap tick
int  timer_register(timer_callback_t cb);

// Hapus callback
void timer_unregister(timer_callback_t cb);

#endif
```

---

### [MODIFY] `drivers/timer.c` — Implementasi Unified Timer

Perubahan utama:
- Hapus semua `extern` yang menyebar
- Tambah `timer_callbacks[]` array
- `timer_handler()` hanya: `tick++`, jalankan callbacks, EOI
- Visual rendering (spinner, uptime) pindah ke callback function terpisah
- `compositor_flush()` dikontrol dari sini dengan frame limiter

```c
// Contoh struktur baru timer_handler():
void timer_handler() {
    timer_ticks++;

    // Jalankan semua subscriber
    for (int i = 0; i < TIMER_MAX_CALLBACKS; i++) {
        if (timer_callbacks[i]) timer_callbacks[i](timer_ticks);
    }

    // EOI — satu tempat, tidak tersebar
    outb(0x20, 0x20);
}
```

---

### [NEW] `kernel/timer_callbacks.c` — Subscriber Default

Pisahkan logika visual dari driver timer:

```c
// Subscriber 1: visual (spinner + uptime)
static void cb_visual(uint32_t tick) {
    if (fb_width == 0) return;
    // spinner setiap 10 ticks
    // uptime setiap TIMER_HZ ticks
}

// Subscriber 2: cursor blink
static void cb_cursor(uint32_t tick) {
    if (tick % 25 == 0) tty_blink_cursor();
}

// Subscriber 3: screen flush — frame rate limited ke 50fps
static void cb_flush(uint32_t tick) {
    (void)tick;
    compositor_flush();
}

// Subscriber 4: CPU usage tracking
static void cb_cpu(uint32_t tick) {
    cpu_total_ticks++;
    // ... yield_counter logic
}

// Dipanggil dari kernel_main setelah init_timer()
void timer_callbacks_init() {
    timer_register(cb_visual);
    timer_register(cb_cursor);
    timer_register(cb_flush);
    timer_register(cb_cpu);
}
```

---

### [MODIFY] `apps/login.c` — Gunakan TICKS() macro

```c
// SEBELUM (fragile):
for(int i = 0; i < 100; i++) sys_yield(); // ~2 detik pada 50Hz

// SESUDAH (self-documenting):
timer_sleep_ticks(TICKS(2000)); // 2000ms = 2 detik, otomatis correct
```

---

### [MODIFY] `apps/kernel_userlib.c` — Hapus `extern timer_ticks` langsung

```c
// SEBELUM:
extern uint32_t timer_ticks;
uint32_t sys_uptime(void) { return timer_ticks; }

// SESUDAH:
uint32_t sys_uptime(void) { return timer_get_ticks(); }  // via API
```

---

## Open Questions

> [!IMPORTANT]
> **Q1**: Mau implementasi `timer_sleep_ticks()` sebagai busy-wait atau interrupt-driven?
> - **Busy-wait** (mudah): loop `while(timer_get_ticks() < target)` — tapi membuang CPU
> - **Interrupt-driven** (bersih): set `sleep_until` flag, `hlt` sampai timer callback wake up — lebih kompleks

> [!IMPORTANT]
> **Q2**: Berapa banyak callback yang dibutuhkan? `TIMER_MAX_CALLBACKS = 8` cukup?
> Saat ini ada 4 default (visual, cursor, flush, cpu). Sisanya untuk driver baru (misal: network polling, audio tick).

> [!NOTE]
> **Q3**: `compositor_flush()` setiap tick = 50 kali/detik. Ini fine untuk QEMU, tapi bisa berat jika resolusi tinggi. Bisa dijadikan configurable: `#define RENDER_HZ 25` (setiap 2 ticks).

---

## Verification Plan

### Build Check
```bash
make clean && make boot_image.iso
```

### Manual Test
1. Boot → timer spinner berputar mulus tanpa keyboard
2. UPTIME update setiap tepat 1 detik
3. Login delay terasa natural (~1-2 detik)
4. Buka fileman/taskmgr → tidak freeze
5. CPU usage di taskmgr menunjukkan angka realistis

### Regression Test
- Keyboard input masih berfungsi setelah refactor
- Mouse masih bergerak
- Shell command semua masih bekerja
