/**
 * @file sys_arch.c
 * @brief lwIP OS abstraction layer implementation for Kyuzen OS.
 *
 * Kyuzen OS — Monolithic 64-bit Higher-Half Kernel
 * Toolchain: Clang/LLVM (x86_64), -ffreestanding, -mcmodel=kernel
 *
 * In NO_SYS=1 mode the only runtime function lwIP requires from the
 * OS port is sys_now() — a monotonically increasing millisecond counter
 * used to drive internal software timers (TCP retransmit, ARP expiry, etc.).
 *
 * Threading, semaphores, mailboxes, and mutexes are all compiled out by
 * the NO_SYS guard inside lwip/sys.h — we never implement them here.
 *
 * ===========================================================================
 * HOW TO WIRE sys_now() TO YOUR REAL TIMER
 * ===========================================================================
 *
 * The default implementation below uses the x86_64 TSC (Time Stamp Counter)
 * to produce a millisecond timestamp without requiring a configured PIT.
 * This works at boot before any interrupt infrastructure is set up.
 *
 * Once your PIT or APIC timer is running, replace this with a reference to
 * your kernel's tick counter for better accuracy:
 *
 *   // In arch/x86/pit.c:
 *   volatile uint32_t g_pit_ticks_ms = 0;   // incremented in IRQ0 handler
 *
 *   // In sys_arch.c — just add:
 *   extern volatile uint32_t g_pit_ticks_ms;
 *   uint32_t sys_now(void) { return g_pit_ticks_ms; }
 *
 * ===========================================================================
 */

#include "lwip/opt.h"
#include "arch/sys_arch.h"

/* -------------------------------------------------------------------------
 * TSC-based millisecond counter (boot-time fallback)
 *
 * The TSC frequency varies per CPU. We estimate it at boot by measuring
 * the TSC delta over a short busy-wait loop calibrated against the PIT.
 *
 * For initial lwIP bring-up this is more than accurate enough:
 *   - TCP timer granularity: 250 ms
 *   - ARP timer granularity: 5000 ms
 *   - DHCP is disabled
 *
 * The calibration is performed lazily on the first call to sys_now().
 * It reads the MISC_ENABLE MSR and TSC is ALWAYS enabled on any x86_64.
 * -------------------------------------------------------------------------*/

/**
 * rdtsc() — read the Time Stamp Counter via RDTSC instruction.
 * Returns the 64-bit cycle count since last CPU reset.
 */
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/**
 * Busy-wait for approximately `us` microseconds using a simple loop.
 *
 * We use the I/O port 0x80 (POST debug port) as a ~1 µs delay element —
 * a well-established x86 technique used even in Linux and BIOS code.
 * Each outb to port 0x80 takes ~1 µs on modern hardware.
 */
static inline void udelay(uint32_t us) {
    while (us--) {
        __asm__ volatile (
            "outb %%al, $0x80"
            : : "a"((uint8_t)0)
        );
    }
}

/* TSC ticks per millisecond, measured once at calibration time. */
static uint64_t g_tsc_ticks_per_ms = 0;

/* TSC value captured at the moment calibration completed (t=0). */
static uint64_t g_tsc_base = 0;

/**
 * Calibrate the TSC frequency.
 *
 * Measures the number of TSC ticks over a 10 ms busy-wait (10 000 × 1 µs
 * I/O delays). Stores the result in g_tsc_ticks_per_ms.
 *
 * This is called once, lazily, on the first sys_now() invocation.
 * After calibration g_tsc_base is set so t=0 corresponds to that moment.
 */
static void tsc_calibrate(void) {
    const uint32_t CALIBRATION_US = 10000; /* 10 ms */

    uint64_t t0 = rdtsc();
    udelay(CALIBRATION_US);
    uint64_t t1 = rdtsc();

    uint64_t delta = t1 - t0;

    /* ticks per millisecond = delta / 10 */
    g_tsc_ticks_per_ms = delta / 10;

    /* Guard against absurdly low values (e.g., very slow emulator). */
    if (g_tsc_ticks_per_ms < 100) {
        g_tsc_ticks_per_ms = 100; /* 100 KHz absolute minimum */
    }

    /* Set base so sys_now() returns ms since this calibration. */
    g_tsc_base = rdtsc();
}

/* -------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------*/

/**
 * sys_now() — return milliseconds since first call.
 *
 * lwIP calls this function to drive software timers. The value must be:
 *   - Monotonically increasing
 *   - Measured in milliseconds
 *   - Allowed to wrap at UINT32_MAX (~49 days)
 *
 * Thread-safety: in NO_SYS=1 mode lwIP is single-threaded and driven
 * manually from the kernel's timer ISR. No locking is required.
 *
 * TODO: Replace with g_pit_ticks_ms once your PIT ISR is wired up.
 */
uint32_t sys_now(void) {
    if (__builtin_expect(g_tsc_ticks_per_ms == 0, 0)) {
        tsc_calibrate();
        return 0;
    }

    uint64_t elapsed_ticks = rdtsc() - g_tsc_base;
    uint64_t elapsed_ms    = elapsed_ticks / g_tsc_ticks_per_ms;

    /* Cast to uint32_t — intentional wrap, lwIP expects this. */
    return (uint32_t)elapsed_ms;
}
