// ============================================================
// drivers/timer.c — Unified Timer Driver + Preemptive Scheduler Hook
//
// ARSITEKTUR:
//   - timer_ticks (uint64_t, static) = satu-satunya sumber waktu raw
//   - timer_handler() = dipanggil dari timer_isr_stub, MENGEMBALIKAN RSP
//   - schedule() di task.c melakukan context switch via RSP swap
//   - Semua timing eksternal pakai timer_get_ms() — hardware-agnostic
//
// URUTAN timer_handler (tidak boleh diubah):
//   1. Increment ticks + CPU tracking
//   2. Jalankan semua subscriber callbacks
//   3. Kirim EOI ke PIC (sebelum switch agar task baru bisa terima interrupt)
//   4. Cek preemption quantum → panggil schedule() → return RSP
// ============================================================

#include "io.h"
#include <stdint.h>
#include "timer.h"
#include "task.h"    // registers_t, schedule()

// ============================================================
// STATE INTERNAL
// ============================================================

static volatile uint64_t timer_ticks = 0;

// CPU Usage Tracker (tetap tick-based untuk akurasi rasio)
static uint32_t cpu_idle_ticks    = 0;
static uint32_t cpu_total_ticks   = 0;
static uint32_t current_cpu_usage = 0;

// yield_counter: di-increment oleh syscall sys_yield / yield()
// Timer membacanya setiap tick untuk deteksi CPU idle
extern volatile uint32_t yield_counter;
static uint32_t prev_yield_snapshot = 0;

// Callback registry
static timer_callback_t timer_callbacks[TIMER_MAX_CALLBACKS] = {0};

// Preemption quantum (20ms default)
static uint64_t next_schedule_ms = 0;

// ============================================================
// PUBLIC API — MILLISECOND STANDARD
// ============================================================

uint64_t timer_get_ms(void) {
    return (timer_ticks * 1000ULL) / TIMER_HZ;
}

void timer_sleep_ms(uint32_t ms) {
    uint64_t target = timer_get_ms() + ms;
    while (timer_get_ms() < target) {
        __asm__ volatile("sti; hlt");
    }
}

// ============================================================
// PUBLIC API — TICK LEVEL
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

uint32_t get_cpu_usage(void) {  // Legacy alias
    return current_cpu_usage;
}

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
    return -1;
}

void timer_unregister(timer_callback_t cb) {
    for (int i = 0; i < TIMER_MAX_CALLBACKS; i++) {
        if (timer_callbacks[i] == cb) timer_callbacks[i] = 0;
    }
}

// ============================================================
// PIT INITIALIZATION
// ============================================================

void init_timer(uint32_t freq) {
    uint32_t divisor = 1193180 / freq;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

// ============================================================
// IRQ0 HANDLER — dipanggil oleh timer_isr_stub
//
// PENTING: Fungsi ini sekarang MENGEMBALIKAN registers_t*
//   - Return = RSP yang sama   → tidak ada context switch
//   - Return = RSP task lain   → preemptive context switch
//
// Assembly di timer_isr_stub melakukan: mov rsp, rax (pakai return value ini)
// ============================================================
registers_t* timer_handler(registers_t* r) {
    // 1. Tick counter (monotonic, tidak pernah overflow untuk OS normal)
    timer_ticks++;

    // 2. CPU Usage tracking (tick-based, reset per detik)
    cpu_total_ticks++;
    uint32_t cur = yield_counter;
    if (cur != prev_yield_snapshot) {
        cpu_idle_ticks++;  // Task sedang yield/idle saat tick ini
    }
    prev_yield_snapshot = cur;

    if (cpu_total_ticks >= TIMER_HZ) {
        current_cpu_usage = 100 - ((cpu_idle_ticks * 100) / cpu_total_ticks);
        cpu_total_ticks   = 0;
        cpu_idle_ticks    = 0;
    }

    // 3. Jalankan subscriber callbacks (render, blink, flush)
    //    Callbacks berjalan di konteks task saat ini, sebelum switch
    for (int i = 0; i < TIMER_MAX_CALLBACKS; i++) {
        if (timer_callbacks[i]) {
            timer_callbacks[i](timer_ticks);
        }
    }

    // 4. EOI — kirim ke Master PIC sebelum context switch
    //    Sehingga task baru bisa langsung menerima interrupt berikutnya
    outb(0x20, 0x20);

    // 5. Preemptive scheduling — cek apakah quantum sudah habis
    uint64_t now = timer_get_ms();
    if (now >= next_schedule_ms) {
        next_schedule_ms = now + 20; // Quantum = 20ms
        return schedule(r);          // Minta scheduler untuk memilih task berikutnya
    }

    // Quantum belum habis — kembalikan RSP task saat ini (tidak ada switch)
    return r;
}