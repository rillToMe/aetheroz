// ============================================================
// include/timer.h — Unified Timer API, Kyuzen OS
//
// STANDAR: Semua logika waktu di OS menggunakan MILLISECOND (ms).
//          Frekuensi hardware (TIMER_HZ) hanya diubah di satu tempat ini.
//
// Cara mengubah frekuensi PIT:
//   Cukup ganti #define TIMER_HZ di bawah.
//   Seluruh OS (uptime, sleep, render, blink) otomatis menyesuaikan.
// ============================================================

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// ============================================================
// KONFIGURASI HARDWARE — ubah di sini saja
// ============================================================
#define TIMER_HZ          50            // Frekuensi PIT (tick/detik). Bisa: 50, 100, 144...
#define TIMER_MS_PER_TICK (1000 / TIMER_HZ) // ms per tick (20ms pada 50Hz, 10ms pada 100Hz)

// ============================================================
// API WAKTU BERBASIS MILLISECOND (standar baru)
// Gunakan ini untuk semua logika waktu di OS.
// ============================================================

// Baca waktu sistem dalam milidetik sejak boot.
// uint64_t: overflow ≈ 584 juta tahun — tidak perlu khawatir wrap.
uint64_t timer_get_ms(void);

// Sleep selama N milidetik. CPU di-halt selama menunggu (hemat daya).
// Aman dipanggil dari kernel context (tidak di dalam ISR).
void     timer_sleep_ms(uint32_t ms);

// ============================================================
// API WAKTU BERBASIS TICKS (untuk internal / low-level)
// ============================================================

// Baca tick counter mentah sejak boot (monotonic).
// uint64_t: pada 50Hz overflow ≈ 11.7 miliar tahun.
uint64_t timer_get_ticks(void);

// Baca uptime dalam detik.
uint64_t timer_get_seconds(void);

// Baca CPU usage persentase (0-100%). Tetap uint32_t — nilainya kecil.
uint32_t timer_get_cpu_usage(void);

// Sleep selama N ticks (low-level, lebih presisi dari timer_sleep_ms).
void     timer_sleep_ticks(uint32_t ticks);

// ============================================================
// MAKRO HELPER (backward compatible)
// ============================================================

// Konversi ms → ticks (untuk driver level)
#define TICKS(ms)         ((ms) / TIMER_MS_PER_TICK)

// ============================================================
// INISIALISASI
// ============================================================
void init_timer(uint32_t freq);
void timer_callbacks_init(void);

// ============================================================
// CALLBACK/SUBSCRIBER SYSTEM
// ============================================================
typedef void (*timer_callback_t)(uint32_t tick);
#define TIMER_MAX_CALLBACKS 8

int  timer_register(timer_callback_t cb);
void timer_unregister(timer_callback_t cb);

// ============================================================
// LEGACY (tetap ada agar tidak breaking)
// ============================================================
uint32_t get_cpu_usage(void);

#endif // TIMER_H