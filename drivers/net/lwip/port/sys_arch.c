/**
 * @file sys_arch.c
 * @brief lwIP OS abstraction layer — Kyuzen OS (NO_SYS=1)
 *
 * Kyuzen OS — Monolithic 64-bit Higher-Half Kernel
 * Toolchain: Clang/LLVM (x86_64-pc-none-elf), -ffreestanding, -mcmodel=kernel
 *
 * Dalam NO_SYS=1 mode, satu-satunya fungsi yang wajib diimplementasikan adalah:
 *   sys_now() — millisecond counter monotonically increasing
 *
 * Implementasi ini mendelegasikan langsung ke timer_get_ms() yang sudah
 * dikalibrasi via PIT di drivers/timer.c. Ini jauh lebih akurat daripada
 * TSC busy-wait calibration.
 *
 * sys_check_timeouts() dipanggil dari cb_network() di timer_callbacks.c,
 * bukan dari sini — untuk memastikan pemisahan concern yang bersih.
 */

#include "lwip/opt.h"
#include "arch/sys_arch.h"

/*
 * timer_get_ms() dari drivers/timer.c:
 * mengembalikan millisecond sejak boot, berbasis PIT ticks.
 * uint64_t → kita cast ke uint32_t (wrap di ~49 hari, lwIP handle ini).
 */
extern uint64_t timer_get_ms(void);

/**
 * sys_now() — return milliseconds since boot.
 *
 * lwIP memanggil ini di setiap sys_check_timeouts() untuk menentukan
 * kapan timer internal harus fire (TCP retransmit, ARP expiry, dll).
 *
 * Kontrak:
 *   - Unit: milliseconds
 *   - Monotonically increasing (tidak pernah turun)
 *   - Boleh wrap di UINT32_MAX (~49 hari) — lwIP sudah handle ini
 *   - Thread-safe: timer_get_ms() membaca volatile uint64_t, aman di NO_SYS=1
 */
uint32_t sys_now(void) {
    return (uint32_t)timer_get_ms();
}
