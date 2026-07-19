/**
 * @file sys_arch.h
 * @brief lwIP OS abstraction layer for Kyuzen OS (NO_SYS = 1 mode).
 *
 * When NO_SYS = 1, lwIP does NOT use threading, semaphores, or mailboxes.
 * This header still needs to exist because <lwip/sys.h> unconditionally
 * includes <arch/sys_arch.h>. With -I drivers/net/port in LWIP_CFLAGS,
 * the compiler resolves this to: drivers/net/port/arch/sys_arch.h.
 *
 * In NO_SYS mode we only need:
 *   1. sys_prot_t type  (for SYS_ARCH_PROTECT in arch/cc.h)
 *   2. sys_now()        (millisecond tick counter for lwIP timers)
 *
 * Everything else (semaphores, mailboxes, threads) is compiled out by
 * the NO_SYS guard inside lwip/sys.h — we never define them here.
 *
 * NOTE: sys_now() MUST be implemented somewhere in the kernel.
 *       A minimal stub is shown below; wire it to your PIT/APIC counter.
 */

#ifndef SYS_ARCH_H
#define SYS_ARCH_H

#include <stdint.h>

/* ===========================================================================
 * CRITICAL SECTION TYPE
 *
 * sys_prot_t is the type used to save/restore the CPU interrupt flag
 * (RFLAGS.IF) across SYS_ARCH_PROTECT / SYS_ARCH_UNPROTECT in cc.h.
 * A 64-bit value is required on x86_64 (RFLAGS is 64 bits wide).
 * =========================================================================*/
typedef uint64_t sys_prot_t;

/* ===========================================================================
 * TICK COUNTER  —  sys_now()
 *
 * lwIP calls sys_now() internally to drive its software timers (TCP
 * retransmits, ARP expiry, DHCP, etc.). The return value must be a
 * monotonically increasing count of milliseconds since boot.
 *
 * Implementation contract:
 *   - Unit:        milliseconds
 *   - Must NOT go backwards (monotonic)
 *   - Wraps at UINT32_MAX (~49 days) — lwIP handles this correctly
 *
 * You MUST provide a real definition in a kernel .c file.
 * Suggested location: kernel/timer.c or arch/x86/pit.c
 *
 *   // Example — driven by PIT at 1000 Hz:
 *   static volatile uint32_t g_tick_ms = 0;
 *
 *   void pit_irq_handler(void) {
 *       g_tick_ms++;
 *       sys_check_timeouts();   // ← call from here, or from scheduler tick
 *   }
 *
 *   uint32_t sys_now(void) {
 *       return g_tick_ms;
 *   }
 *
 * The declaration below makes the linker enforce that definition exists.
 * =========================================================================*/
uint32_t sys_now(void);

#endif /* SYS_ARCH_H */
