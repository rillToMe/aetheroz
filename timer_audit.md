# Timer Audit Report — Kyuzen OS

## Hasil Scan

### 🔴 VIOLATION — Harus Difix

| File | Line | Masalah | Fix |
|------|------|---------|-----|
| `apps/shell.c` | 212 | `for(volatile int w=0; w<50000000; w++)` — busy-spin delay, tidak konsisten dengan timer manapun | → `timer_sleep_ticks(TICKS(2000))` |
| `apps/zen.c` | 74 | `for(volatile int w=0; w<90000000; w++)` — busy-spin saat error RAM habis | → `timer_sleep_ticks(TICKS(3000))` |

### 🟡 INFO — Perlu Diperhatikan (bukan violation)

| File | Line | Catatan |
|------|------|---------|
| `apps/kernel_userlib.c` | 68, 78 | `sti; hlt` di `read_keyboard` dan `sys_yield` — ini BENAR, ini implementasi dari `timer_sleep_ticks` |
| `drivers/timer.c` | 97–99 | `outb(0x40/0x43)` — BENAR, ini di dalam `init_timer()` sendiri |
| `drivers/rtc.c` | semua | RTC adalah sumber waktu terpisah (real-world time, bukan uptime) — ini normal, bukan violation |

### ✅ BERSIH — Tidak Ada Masalah

- Semua `TIMER_HZ` usage → dari `include/timer.h` (single source of truth ✅)
- `timer_ticks` → hanya internal `drivers/timer.c` (static, tidak bocor ✅)
- `syscall.c` → sudah pakai `timer_get_ticks()` ✅
- `kernel_userlib.c` → sudah pakai `timer_get_ticks()` ✅
- `login.c` → sudah pakai `timer_sleep_ticks(TICKS(ms))` ✅

## Yang Perlu Difix

1. `apps/shell.c:212` — busy-spin delay di perintah `sleep`
2. `apps/zen.c:74` — busy-spin delay di error handler

## Catatan Arsitektur

`sti; hlt` yang tersisa di `kernel_userlib.c` adalah **implementasi yang benar** dari idle CPU.
Ini adalah equivalent dari `timer_sleep_ticks` di kernel side. Tidak perlu diubah.
