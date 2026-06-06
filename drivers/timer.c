// ============================================================
// drivers/timer.c — Unified Timer Driver, Kyuzen OS
//
// ARSITEKTUR: Satu sumber IRQ0 → callback array.
// Tidak ada logic visual di sini. Semua subscriber didaftarkan
// dari kernel/timer_callbacks.c via timer_register().
//
// ATURAN:
//   - Semua kode yang butuh waktu → #include "timer.h"
//   - Semua kode yang butuh delay → timer_sleep_ticks(TICKS(ms))
//   - JANGAN akses timer_ticks langsung dari luar file ini
// ============================================================

#include "io.h"
#include <stdint.h>
#include "timer.h"
#include "task.h"

// ============================================================
// STATE INTERNAL — hanya bisa diakses via API (timer_get_*)
// ============================================================

static volatile uint32_t timer_ticks = 0;

// --- CPU Usage Tracker ---
static uint32_t cpu_idle_ticks    = 0;
static uint32_t cpu_total_ticks   = 0;
static uint32_t current_cpu_usage = 0;

// yield_counter: di-increment di syscall.c setiap kali sys_yield dipanggil.
// Timer membacanya tiap tick untuk menentukan apakah CPU idle.
extern volatile uint32_t yield_counter;
static uint32_t prev_yield_snapshot = 0;

// --- Callback Registry ---
static timer_callback_t timer_callbacks[TIMER_MAX_CALLBACKS] = {0};

// ============================================================
// PUBLIC API IMPLEMENTATION
// ============================================================

uint32_t timer_get_ticks(void) {
    return timer_ticks;
}

uint32_t timer_get_seconds(void) {
    return timer_ticks / TIMER_HZ;
}

uint32_t timer_get_cpu_usage(void) {
    return current_cpu_usage;
}

// Legacy alias — agar kode lama (syscall.c syscall 37) tidak rusak
uint32_t get_cpu_usage(void) {
    return current_cpu_usage;
}

// Sleep N ticks — busy-wait yang aman (tidak disable interrupt)
void timer_sleep_ticks(uint32_t ticks) {
    uint32_t target = timer_ticks + ticks;
    while (timer_ticks < target) {
        // hlt: hemat daya, bangun saat interrupt berikutnya (termasuk timer)
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
    return -1; // Slot penuh
}

void timer_unregister(timer_callback_t cb) {
    for (int i = 0; i < TIMER_MAX_CALLBACKS; i++) {
        if (timer_callbacks[i] == cb) {
            timer_callbacks[i] = 0;
            return;
        }
    }
}

// ============================================================
// PIT INITIALIZATION
// ============================================================

void init_timer(uint32_t freq) {
    uint32_t divisor = 1193180 / freq;
    outb(0x43, 0x36);                              // Channel 0, lobyte/hibyte, Mode 3
    outb(0x40, (uint8_t)(divisor & 0xFF));         // Low byte divisor
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF)); // High byte divisor
}

// ============================================================
// IRQ0 HANDLER — dipanggil oleh timer_isr_stub setiap tick
//
// URUTAN EKSEKUSI (tidak boleh diubah):
//   1. Increment counter (selalu pertama)
//   2. Update CPU stats
//   3. Jalankan semua callback subscriber
//   4. Kirim EOI ke PIC (selalu terakhir sebelum iretq)
// ============================================================

void timer_handler(void) {
    // 1. Monotonic tick counter
    timer_ticks++;

    // 2. CPU usage tracking (heuristik berbasis yield_counter)
    cpu_total_ticks++;
    uint32_t cur = yield_counter;
    if (cur != prev_yield_snapshot) {
        cpu_idle_ticks++;
    }
    prev_yield_snapshot = cur;

    // Kalkulasi ulang setiap TIMER_HZ ticks (1 detik)
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

    // 4. EOI: beritahu PIC bahwa IRQ0 sudah diproses.
    //    HARUS di sini — satu tempat, tidak tersebar ke driver lain.
    outb(0x20, 0x20);
}