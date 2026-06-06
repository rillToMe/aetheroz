// ============================================================
// drivers/timer.c — Unified Timer Driver, Kyuzen OS
//
// ARSITEKTUR: Single Source of Truth untuk semua timing.
//   - Semua logika waktu OS menggunakan MILLISECOND, bukan raw ticks.
//   - TIMER_HZ hanya ada di timer.h — ubah di sana, tidak ada yang rusak.
//   - Visual/render logic ada di kernel/timer_callbacks.c via callback system.
//
// ATURAN:
//   - Akses waktu → timer_get_ms()
//   - Delay/sleep → timer_sleep_ms(ms)
//   - JANGAN akses timer_ticks langsung dari luar file ini (private/static)
// ============================================================

#include "io.h"
#include <stdint.h>
#include "timer.h"
#include "task.h"

// ============================================================
// STATE INTERNAL — tidak bisa diakses dari luar
// ============================================================

static volatile uint64_t timer_ticks = 0;


// --- CPU Usage Tracker (tetap tick-based untuk akurasi rasio) ---
static uint32_t cpu_idle_ticks    = 0;
static uint32_t cpu_total_ticks   = 0;
static uint32_t current_cpu_usage = 0;

// yield_counter: di-increment di syscall.c setiap sys_yield dipanggil.
// Timer membacanya tiap tick untuk menentukan apakah CPU sedang idle.
extern volatile uint32_t yield_counter;
static uint32_t prev_yield_snapshot = 0;

// --- Callback Registry ---
static timer_callback_t timer_callbacks[TIMER_MAX_CALLBACKS] = {0};

// ============================================================
// PUBLIC API — MILLISECOND STANDARD
// ============================================================

// Waktu sistem dalam ms. uint64_t: overflow ≈ 584 juta tahun.
uint64_t timer_get_ms(void) {
    return (timer_ticks * 1000ULL) / TIMER_HZ;
}

// Sleep N ms — CPU di-halt antar tick.
void timer_sleep_ms(uint32_t ms) {
    uint64_t target = timer_get_ms() + ms;
    while (timer_get_ms() < target) {
        __asm__ volatile("sti; hlt");
    }
}

// ============================================================
// PUBLIC API — TICK LEVEL (low-level / internal)
// ============================================================

uint64_t timer_get_ticks(void) {
    return timer_ticks;
}

uint64_t timer_get_seconds(void) {
    return timer_get_ms() / 1000;
}

uint32_t timer_get_cpu_usage(void) {
    return current_cpu_usage;
}

// Legacy alias
uint32_t get_cpu_usage(void) {
    return current_cpu_usage;
}

// Sleep N ticks (presisi sub-ms)
void timer_sleep_ticks(uint32_t ticks) {
    uint64_t target = timer_ticks + ticks;
    while (timer_ticks < target) {
        __asm__ volatile("sti; hlt");
    }
}

// ============================================================
// CALLBACK REGISTRY
// ============================================================

int timer_register(timer_callback_t cb) {
    for (int i = 0; i < TIMER_MAX_CALLBACKS; i++) {
        if (timer_callbacks[i] == 0) {
            timer_callbacks[i] = cb;
            return i;
        }
    }
    return -1; // Semua slot penuh
}

void timer_unregister(timer_callback_t cb) {
    for (int i = 0; i < TIMER_MAX_CALLBACKS; i++) {
        if (timer_callbacks[i] == cb) {
            timer_callbacks[i] = 0;
        }
    }
}

// ============================================================
// PIT INITIALIZATION
// ============================================================

void init_timer(uint32_t freq) {
    uint32_t divisor = 1193180 / freq;
    outb(0x43, 0x36);                              // Channel 0, lobyte/hibyte, Mode 3 (square wave)
    outb(0x40, (uint8_t)(divisor & 0xFF));         // Low byte
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF)); // High byte
}

// ============================================================
// IRQ0 HANDLER — dipanggil oleh timer_isr_stub setiap tick
//
// URUTAN (tidak boleh diubah):
//   1. Increment tick counter
//   2. Update CPU idle stats (tick-based, untuk presisi rasio)
//   3. Jalankan semua callback subscriber
//   4. Kirim EOI ke Master PIC (selalu terakhir)
// ============================================================

void timer_handler(void) {
    // 1. Tick counter (monotonic)
    timer_ticks++;

    // 2. CPU usage tracking — tetap tick-based (heuristik rasio yield/total)
    cpu_total_ticks++;
    uint32_t cur = yield_counter;
    if (cur != prev_yield_snapshot) {
        cpu_idle_ticks++;
    }
    prev_yield_snapshot = cur;

    // Kalkulasi ulang setiap 1 detik penuh
    if (cpu_total_ticks >= TIMER_HZ) {
        current_cpu_usage = 100 - ((cpu_idle_ticks * 100) / cpu_total_ticks);
        cpu_total_ticks = 0;
        cpu_idle_ticks  = 0;
    }

    // 3. Jalankan semua subscriber yang terdaftar
    for (int i = 0; i < TIMER_MAX_CALLBACKS; i++) {
        if (timer_callbacks[i]) {
            timer_callbacks[i](timer_ticks);
        }
    }

    // 4. EOI — satu tempat, tidak tersebar
    outb(0x20, 0x20);
}