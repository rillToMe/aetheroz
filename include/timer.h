
// include/timer.h — Unified Timer API, Kyuzen OS
//
// STANDAR: Semua logika waktu di OS menggunakan MILLISECOND (ms).
// Refresh/PIT rate default 60Hz dan bisa diganti runtime lewat shell.
// Preset awal yang didukung: 60, 100, 144.

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// KONFIGURASI DEFAULT HARDWARE
#define TIMER_DEFAULT_HZ  60
#define TIMER_HZ          TIMER_DEFAULT_HZ
#define TIMER_MS_PER_TICK (1000 / TIMER_DEFAULT_HZ) // Perkiraan default; runtime Hz bisa berubah.

// API WAKTU BERBASIS MILLISECOND (standar baru)
// Gunakan ini untuk semua logika waktu di OS.

// Baca waktu sistem dalam milidetik sejak boot.
// uint64_t: overflow ≈ 584 juta tahun — tidak perlu khawatir wrap.
uint64_t timer_get_ms(void);

// Sleep selama N milidetik. CPU di-halt selama menunggu (hemat daya).
// Aman dipanggil dari kernel context (tidak di dalam ISR).
void     timer_sleep_ms(uint32_t ms);

// API WAKTU BERBASIS TICKS (untuk internal / low-level)

// Baca tick counter mentah sejak boot (monotonic).
// uint64_t: pada 50Hz overflow ≈ 11.7 miliar tahun.
uint64_t timer_get_ticks(void);

// Baca uptime dalam detik.
uint64_t timer_get_seconds(void);

// Baca CPU usage persentase (0-100%). Tetap uint32_t — nilainya kecil.
uint32_t timer_get_cpu_usage(void);

// Sleep selama N ticks (low-level, lebih presisi dari timer_sleep_ms).
void     timer_sleep_ticks(uint32_t ticks);

// REFRESH / PIT RATE RUNTIME

// Return refresh/PIT rate aktif saat ini.
uint32_t timer_get_refresh_rate(void);

// Set refresh/PIT rate. Saat ini hanya menerima 60, 100, atau 144.
// Return 0 jika sukses, -1 jika nilai tidak didukung.
int      timer_set_refresh_rate(uint32_t hz);

// Helper validasi preset refresh rate.
int      timer_is_supported_refresh_rate(uint32_t hz);

// MAKRO HELPER (backward compatible)

// Konversi ms → ticks (untuk driver level)
#define TICKS(ms)         (((ms) * TIMER_DEFAULT_HZ + 999) / 1000)

// INISIALISASI
void init_timer(uint32_t freq);
void timer_callbacks_init(void);

// CALLBACK/SUBSCRIBER SYSTEM
typedef void (*timer_callback_t)(uint32_t tick);
#define TIMER_MAX_CALLBACKS 8

int  timer_register(timer_callback_t cb);
void timer_unregister(timer_callback_t cb);

// LEGACY (tetap ada agar tidak breaking)
uint32_t get_cpu_usage(void);

#endif // TIMER_H
