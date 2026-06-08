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
static volatile uint64_t timer_ms = 0;
static volatile uint32_t timer_current_hz = TIMER_DEFAULT_HZ;
static uint32_t timer_ms_remainder = 0;

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
    return timer_ms;
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

uint32_t timer_get_refresh_rate(void) {
    return timer_current_hz;
}

int timer_is_supported_refresh_rate(uint32_t hz) {
    return (hz == 60 || hz == 100 || hz == 144) ? 1 : 0;
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

static void timer_program_pit(uint32_t freq) {
    uint32_t divisor = 1193180 / freq;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

static uint64_t timer_irq_save(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

static void timer_irq_restore(uint64_t flags) {
    if (flags & (1ULL << 9)) {
        __asm__ volatile("sti" ::: "memory");
    }
}

int timer_set_refresh_rate(uint32_t hz) {
    if (!timer_is_supported_refresh_rate(hz)) return -1;

    uint64_t flags = timer_irq_save();
    timer_current_hz = hz;
    timer_ms_remainder = 0;
    cpu_total_ticks = 0;
    cpu_idle_ticks = 0;
    prev_yield_snapshot = yield_counter;
    timer_program_pit(hz);
    timer_irq_restore(flags);
    return 0;
}

void init_timer(uint32_t freq) {
    if (!timer_is_supported_refresh_rate(freq)) {
        freq = TIMER_DEFAULT_HZ;
    }

    timer_current_hz = freq;
    timer_ms_remainder = 0;
    timer_program_pit(freq);
}

static void timer_accumulate_ms(void) {
    uint32_t hz = timer_current_hz;
    timer_ms += 1000 / hz;
    timer_ms_remainder += 1000 % hz;

    if (timer_ms_remainder >= hz) {
        timer_ms++;
        timer_ms_remainder -= hz;
    }
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
    timer_accumulate_ms();

    // 2. CPU Usage tracking (tick-based, reset per detik)
    cpu_total_ticks++;
    uint32_t cur = yield_counter;
    if (cur != prev_yield_snapshot) {
        cpu_idle_ticks++;  // Task sedang yield/idle saat tick ini
    }
    prev_yield_snapshot = cur;

    if (cpu_total_ticks >= timer_current_hz) {
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
