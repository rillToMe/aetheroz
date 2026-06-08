/**
 * @file lwipopts.h
 * @brief lwIP compile-time configuration for Kyuzen OS.
 *
 * Kyuzen OS — Monolithic 64-bit Higher-Half Kernel
 * Toolchain: Clang/LLVM (x86_64), -ffreestanding, -mcmodel=kernel
 *
 * This file is included by lwIP internally via <lwipopts.h> and
 * overrides defaults from lwip/src/include/lwip/opt.h.
 * Keep all options explicit so the build is fully deterministic.
 */

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* ===========================================================================
 * 1. OPERATING SYSTEM / THREADING MODEL
 * =========================================================================*/

/**
 * NO_SYS = 1: Run lwIP in "raw" (no-OS) mode.
 * No threads, no semaphores, no POSIX sockets. The kernel drives the stack
 * manually by calling sys_check_timeouts() on every timer tick.
 */
#define NO_SYS                          1

/**
 * Disable POSIX-style socket and netconn APIs — they require an OS task model.
 */
#define LWIP_SOCKET                     0
#define LWIP_NETCONN                    0

/* ===========================================================================
 * 2. FREESTANDING HEADER EXCLUSIONS  (CRITICAL for -ffreestanding builds)
 *
 * lwIP's arch.h conditionally includes several hosted C standard headers.
 * In a freestanding toolchain (Clang with -ffreestanding), these headers
 * either don't exist or use #include_next which fails because there is no
 * underlying platform libc to chain into.
 *
 * Setting these macros tells lwIP's arch.h to skip those includes.
 * We supply all required types and macros ourselves in arch/cc.h instead.
 *
 * Suppressed hosted headers (NOT in the freestanding C11 subset):
 *   <inttypes.h>  -- PRIx32, SCNx32 format macros, not needed (no printf)
 *   <limits.h>    -- INT_MAX, UINT_MAX, provided inline in arch/cc.h
 *   <unistd.h>    -- POSIX API, irrelevant in freestanding kernel
 *   <ctype.h>     -- tolower/toupper; lwIP arch.h provides its own macros
 *                    when this guard is set
 * =========================================================================*/

/** Prevent arch.h from including <inttypes.h> (not freestanding-safe). */
#define LWIP_NO_INTTYPES_H              1

/** Prevent arch.h from including <limits.h> (not guaranteed freestanding). */
#define LWIP_NO_LIMITS_H                1

/** Prevent arch.h from including <unistd.h> (POSIX, non-freestanding). */
#define LWIP_NO_UNISTD_H                1

/**
 * Prevent arch.h from including <ctype.h> (hosted, not freestanding).
 * lwIP's arch.h defines its own tolower/toupper macros when this is set.
 */
#define LWIP_NO_CTYPE_H                 1

/* ===========================================================================
 * 3. TIMER SUPPORT
 * =========================================================================*/

/**
 * LWIP_TIMERS = 1: Enable the lwIP internal timer mechanism.
 * The kernel must call sys_check_timeouts() periodically (e.g., on every PIT
 * or APIC timer interrupt) to service TCP retransmits, ARP expiry, etc.
 */
#define LWIP_TIMERS                     1

/* ===========================================================================
 * 3. PROTOCOL MODULES
 * =========================================================================*/

/* --- ARP & Ethernet --------------------------------------------------------*/
#define LWIP_ARP                        1
#define LWIP_ETHERNET                   1

/* --- IP v4 ----------------------------------------------------------------*/
#define LWIP_IPV4                       1
#define LWIP_IPV6                       0   /* Not needed for initial bringup */

/* --- ICMP (ping support) ---------------------------------------------------*/
#define LWIP_ICMP                       1

/* --- RAW sockets -----------------------------------------------------------*/
/**
 * LWIP_RAW = 1: Enable raw API (raw_new, raw_send, raw_recv, raw_bind).
 * WAJIB untuk implementasi ping — net_ping.c menggunakan raw ICMP socket.
 * Tanpa ini semua fungsi raw_* dikompilasi sebagai no-op (LWIP_RAW=0 default).
 */
#define LWIP_RAW                        1

/* --- UDP ------------------------------------------------------------------*/
#define LWIP_UDP                        1

/* --- TCP ------------------------------------------------------------------*/
#define LWIP_TCP                        1

/* --- DHCP -----------------------------------------------------------------*/
/**
 * LWIP_DHCP = 1: Aktifkan DHCP client.
 * QEMU -nic user secara otomatis menyediakan DHCP server (10.0.2.x).
 * Setelah netif_set_up() + dhcp_start(), lwIP akan otomatis mendapat IP.
 */
#define LWIP_DHCP                       1

/* Ukuran pool untuk DHCP PCB */
#define MEMP_NUM_DHCP_SERVERS           0   /* Client only, bukan server */

/* --- DNS ------------------------------------------------------------------*/
/**
 * LWIP_DNS = 1: Aktifkan DNS resolver.
 * Wajib untuk `ping google.com` (resolve nama domain ke IP).
 * QEMU -nic user otomatis forward DNS ke 8.8.8.8.
 */
#define LWIP_DNS                        1
#define DNS_TABLE_SIZE                  4   /* Max 4 hostname concurrent */
#define DNS_MAX_NAME_LENGTH             256

/* ===========================================================================
 * 4. MEMORY & ALIGNMENT
 * =========================================================================*/

/**
 * MEM_ALIGNMENT: All buffers aligned to 8 bytes — matches x86_64 ABI
 * requirements and avoids any unaligned-access faults in the kernel.
 */
#define MEM_ALIGNMENT                   8

/**
 * MEM_SIZE: Heap used by mem_malloc() (lwIP's internal heap, distinct from
 * your kernel PMM). 64 KiB is a conservative starting point; increase when
 * you add more concurrent TCP connections.
 */
#define MEM_SIZE                        (64 * 1024)

/* --- Packet buffer (pbuf) pool size ---------------------------------------*/
/**
 * PBUF_POOL_SIZE: Number of pbufs in the pool (used for RX path).
 * Each MEMP_PBUF_POOL element is PBUF_POOL_BUFSIZE bytes.
 */
#define PBUF_POOL_SIZE                  16

/**
 * PBUF_POOL_BUFSIZE: Size of each pool pbuf. 1536 = 1500 (MTU) + Ethernet
 * header (14) + some alignment headroom.
 */
#define PBUF_POOL_BUFSIZE               1536

/* --- Memory pools (memp) sizes --------------------------------------------*/
#define MEMP_NUM_PBUF                   16  /* ROM/RAM pbufs            */
#define MEMP_NUM_UDP_PCB                4   /* Simultaneous UDP sockets */
#define MEMP_NUM_TCP_PCB                8   /* Active TCP connections   */
#define MEMP_NUM_TCP_PCB_LISTEN         4   /* Listening TCP ports      */
#define MEMP_NUM_TCP_SEG                32  /* TCP segment descriptors  */
#define MEMP_NUM_ARP_QUEUE              8   /* Queued ARP packets       */
#define MEMP_NUM_NETBUF                 0   /* Not used (LWIP_NETCONN=0)*/
#define MEMP_NUM_NETCONN                0   /* Not used (LWIP_NETCONN=0)*/

/* ===========================================================================
 * 5. TCP PARAMETERS
 * =========================================================================*/

#define TCP_MSS                         1460    /* Standard Ethernet MSS    */
#define TCP_WND                         (4 * TCP_MSS)
#define TCP_SND_BUF                     (4 * TCP_MSS)
#define TCP_SND_QUEUELEN                (2 * TCP_SND_BUF / TCP_MSS)

/* ===========================================================================
 * 6. STATISTICS & DEBUGGING
 * =========================================================================*/

/**
 * Disable all stats and debug output by default.
 * Enable LWIP_STATS temporarily when debugging the porting layer.
 */
#define LWIP_STATS                      0
#define LWIP_DEBUG                      0

/*
 * LWIP_PLATFORM_DIAG: Sink for debug/stats output.
 * CRITICAL: Do NOT use (void)(x) here. That forces the C compiler to parse
 * and type-check x as a full C expression, which breaks on lwIP format strings
 * like `%"X32_F"\n` when the format-specifier macro chain has any issue.
 * An empty body discards x after preprocessor expansion — the C compiler
 * never evaluates it, so no type/syntax errors can arise from it.
 */
#define LWIP_PLATFORM_DIAG(x)           do { } while (0)

/* ===========================================================================
 * 7. CHECKSUM OFFLOAD
 * =========================================================================*/

/**
 * All checksums computed in software. Once you confirm the NIC supports
 * hardware offload, set the corresponding CHECKSUM_BY_HARDWARE flags.
 */
#define CHECKSUM_BY_HARDWARE            0
#define CHECKSUM_CHECK_IP               1
#define CHECKSUM_CHECK_UDP              1
#define CHECKSUM_CHECK_TCP              1
#define CHECKSUM_CHECK_ICMP             1

/* ===========================================================================
 * 8. LOOPBACK INTERFACE
 * =========================================================================*/

/** Disable the loopback netif — add it later if needed. */
#define LWIP_HAVE_LOOPIF                0
#define LWIP_LOOPBACK_MAX_PBUFS         0

/* ===========================================================================
 * 9. MISCELLANEOUS
 * =========================================================================*/

/**
 * LWIP_NETIF_HOSTNAME: Allow a hostname string on the netif struct.
 * Useful for DHCP option 12, harmless when DHCP is off.
 */
#define LWIP_NETIF_HOSTNAME             1

/** Broadcast support on UDP */
#define IP_SOF_BROADCAST                1

/* ===========================================================================
 * 10. RANDOM NUMBER GENERATOR
 *
 * lwIP membutuhkan LWIP_RAND() untuk:
 *   - dns.c: generate DNS transaction ID (DNS_RAND_TXID macro)
 *   - dhcp.c: generate transaction ID dan port acak
 *
 * Di freestanding kernel tidak ada rand() dari libc.
 * Kita implementasikan LCG (Linear Congruential Generator) sederhana
 * langsung sebagai static inline di sini.
 *
 * Kualitas: cukup untuk DNS/DHCP (tidak perlu kriptografis).
 * Konstanta: Knuth MMIX (m=2^64, a=6364136223846793005, c=1442695040888963407)
 *            disederhanakan ke 32-bit untuk kemudahan.
 * =========================================================================*/

static inline unsigned int lwip_rand_impl(void) {
    static unsigned int lwip_rand_state = 0xDEADBEEFu;
    lwip_rand_state = lwip_rand_state * 1664525u + 1013904223u; /* Knuth LCG */
    return lwip_rand_state;
}

#define LWIP_RAND() lwip_rand_impl()

#endif /* LWIPOPTS_H */
