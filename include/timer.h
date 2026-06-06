#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// ============================================================
// KONSTANTA GLOBAL — single source of truth untuk semua timing
// Ubah TIMER_HZ di sini, semua kalkulasi otomatis menyesuaikan.
// ============================================================
#define TIMER_HZ          50                    // Frekuensi PIT (ticks per detik)
#define TIMER_MS_PER_TICK (1000 / TIMER_HZ)    // Berapa ms per satu tick (20ms pada 50Hz)

// Konversi waktu yang mudah dibaca — GANTI magic numbers dengan ini!
//   TICKS(1000) = 50 ticks = 1 detik
//   TICKS(500)  = 25 ticks = 0.5 detik
#define TICKS(ms)         ((ms) / TIMER_MS_PER_TICK)

// ============================================================
// API PUBLIK — semua kode lain pakai fungsi ini, BUKAN extern
// ============================================================

// Inisialisasi PIT pada frekuensi tertentu (dipanggil sekali dari kmain)
void     init_timer(uint32_t freq);

// Baca uptime dalam ticks (monotonic counter sejak boot)
uint32_t timer_get_ticks(void);

// Baca uptime dalam detik
uint32_t timer_get_seconds(void);

// Baca CPU usage persentase (0–100%)
uint32_t timer_get_cpu_usage(void);

// Sleep selama N ticks — AMAN dipanggil dari kernel context
// (busy-wait berbasis timer_ticks, tidak memblokir interrupt)
void     timer_sleep_ticks(uint32_t ticks);

// ============================================================
// CALLBACK/SUBSCRIBER SYSTEM
// Driver atau subsistem bisa mendaftar untuk dipanggil tiap tick.
// Semua callback harus CEPAT (< 1ms) — jangan lakukan I/O berat di sini.
// ============================================================
typedef void (*timer_callback_t)(uint32_t tick);

#define TIMER_MAX_CALLBACKS 8

// Daftarkan fungsi yang dipanggil setiap IRQ0 tick.
// Mengembalikan index (>= 0) jika sukses, -1 jika slot penuh.
int  timer_register(timer_callback_t cb);

// Hapus callback berdasarkan pointer fungsi.
void timer_unregister(timer_callback_t cb);

// Inisialisasi default subscriber kernel (visual, cursor, flush, cpu)
// Dipanggil dari kmain setelah init_timer().
void timer_callbacks_init(void);

// ============================================================
// LEGACY (tetap ada agar kode lama tidak rusak)
// Akan di-deprecate di masa depan — gunakan timer_get_cpu_usage()
// ============================================================
uint32_t get_cpu_usage(void);

#endif // TIMER_H