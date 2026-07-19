/**
 * @file cc.h
 * @brief lwIP compiler/platform adaptation layer for Kyuzen OS.
 *
 * Kyuzen OS — Monolithic 64-bit Higher-Half Kernel
 * Toolchain: Clang/LLVM (x86_64), -ffreestanding, -mcmodel=kernel
 *
 * This header is included by every lwIP source file via <lwip/arch.h>
 * as #include "arch/cc.h". With -I drivers/net/port in LWIP_CFLAGS,
 * the compiler resolves this to: drivers/net/port/arch/cc.h (this file).
 *
 * It maps all primitive types and platform macros to their freestanding
 * equivalents. No hosted libc (stdio, string, stdlib) is used here.
 */

#ifndef CC_H
#define CC_H

#include <stdint.h>     /* Fixed-width integer types -- freestanding header */
#include <stddef.h>     /* size_t, ptrdiff_t, NULL  -- freestanding header  */

/*
 * Clang 21 + msys2 clang64 freestanding stdint.h workaround:
 *
 * When targeting x86_64-pc-none-elf with -ffreestanding, Clang's built-in
 * stdint.h defines uint64_t via __UINT64_TYPE__ but NOT int64_t, because
 * __INT64_TYPE__ is left undefined for this target/OS combination.
 *
 * lwIP's arch.h line 133 does: typedef int64_t s64_t;
 * That fails unless we provide int64_t here.
 *
 * Guard: if INT64_MAX is absent, stdint.h did not define int64_t.
 * 'long long' is guaranteed to be exactly 64 bits on x86_64.
 */
#ifndef INT64_MAX
typedef signed long long    int64_t;
typedef unsigned long long  uint64_t;   /* may already exist, but #ifndef guard */
# define INT64_MAX   ((int64_t)0x7FFFFFFFFFFFFFFFLL)
# define INT64_MIN   ((-INT64_MAX) - 1LL)
# define UINT64_MAX  ((uint64_t)0xFFFFFFFFFFFFFFFFULL)
#endif

/*
 * Pull in Kyuzen OS's own memory/string utilities.
 * These provide memcpy, memset, strcmp — required by the lwIP core.
 * Path: drivers/net/port/arch/ → ../../../../include/string.h
 */
#include "../../../../include/string.h"

/*
 * ---------------------------------------------------------------------------
 * HOSTED HEADER REPLACEMENTS
 *
 * lwipopts.h sets LWIP_NO_INTTYPES_H=1 and LWIP_NO_LIMITS_H=1, which
 * prevents lwIP's arch.h from including <inttypes.h> and <limits.h>.
 * Those are hosted headers — not available in a freestanding toolchain.
 *
 * We provide the minimal subset that lwIP actually uses right here.
 * ---------------------------------------------------------------------------
 */

/* --- Replacements for <limits.h> ---------------------------------------- */
#ifndef INT_MAX
#define INT_MAX     0x7FFFFFFF
#endif
#ifndef INT_MIN
#define INT_MIN     (-INT_MAX - 1)
#endif
#ifndef UINT_MAX
#define UINT_MAX    0xFFFFFFFFU
#endif
#ifndef LONG_MAX
#define LONG_MAX    0x7FFFFFFFFFFFFFFFL
#endif
#ifndef ULONG_MAX
#define ULONG_MAX   0xFFFFFFFFFFFFFFFFUL
#endif

/* --- Replacements for <inttypes.h> PRI* format macros -------------------- */
/*
 * These are only used by lwIP's debug/stats printf paths.
 * Since LWIP_DEBUG=0 and LWIP_STATS=0 in lwipopts.h, they are compiled out.
 * Defining them here prevents any #ifdef / sizeof guard from failing to
 * compile if referenced in a header we cannot easily exclude.
 */
#ifndef PRIu8
#define PRIu8   "u"
#endif
#ifndef PRId8
#define PRId8   "d"
#endif
#ifndef PRIx8
#define PRIx8   "x"
#endif
#ifndef PRIu16
#define PRIu16  "u"
#endif
#ifndef PRId16
#define PRId16  "d"
#endif
#ifndef PRIx16
#define PRIx16  "x"
#endif
#ifndef PRIu32
#define PRIu32  "u"
#endif
#ifndef PRId32
#define PRId32  "d"
#endif
#ifndef PRIx32
#define PRIx32  "x"
#endif
#ifndef PRIu64
#define PRIu64  "lu"
#endif
#ifndef PRId64
#define PRId64  "ld"
#endif
#ifndef PRIx64
#define PRIx64  "lx"
#endif

/*
 * lwIP-specific format specifiers used in debug strings like:
 *   LWIP_DEBUGF(INET_DEBUG, ("val=%"X32_F"\n", val));
 *
 * These are normally defined in lwip/arch.h via the PRI* macros above,
 * but we define them here DIRECTLY as string literals to guarantee they
 * are available regardless of whether the PRI* chain resolves correctly.
 * The #ifndef guards let arch.h's definitions take precedence if present.
 */
#ifndef X8_F
#define X8_F   "02x"
#endif
#ifndef U16_F
#define U16_F  "u"
#endif
#ifndef S16_F
#define S16_F  "d"
#endif
#ifndef X16_F
#define X16_F  "x"
#endif
#ifndef U32_F
#define U32_F  "u"
#endif
#ifndef S32_F
#define S32_F  "d"
#endif
#ifndef X32_F
#define X32_F  "x"
#endif
#ifndef SZT_F
#define SZT_F  "u"
#endif

/*
 * ---------------------------------------------------------------------------
 * STDLIB REPLACEMENTS
 *
 * lwIP's netif.c includes <stdlib.h> solely for atoi(). Rather than
 * relying on port/stdlib.h being found through the -I chain, we define
 * atoi here in cc.h — the universal entry point included by EVERY lwIP
 * translation unit via lwip/arch.h. This guarantees the declaration is
 * always visible before any lwIP source uses it.
 *
 * static inline: each TU gets its own copy so no multiple-definition
 * link errors can arise.
 * ---------------------------------------------------------------------------
 */
#ifndef KYUZEN_ATOI_DEFINED
#define KYUZEN_ATOI_DEFINED 1
static inline int atoi(const char *s) {
    int result = 0, sign = 1;
    while (*s == ' ' || *s == '\t' || *s == '\n' ||
           *s == '\r' || *s == '\f' || *s == '\v') { s++; }
    if      (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }
    return sign * result;
}
#endif /* KYUZEN_ATOI_DEFINED */

/* ===========================================================================
 * 1. PRIMITIVE TYPE MAPPINGS
 *
 * lwIP expects these exact typedef names. We map them directly to the
 * <stdint.h> guaranteed-width types so they are correct on every target.
 * =========================================================================*/

typedef uint8_t     u8_t;
typedef int8_t      s8_t;
typedef uint16_t    u16_t;
typedef int16_t     s16_t;
typedef uint32_t    u32_t;
typedef int32_t     s32_t;

/**
 * mem_ptr_t: used internally by lwIP for pointer arithmetic inside mem.c.
 * On x86_64 a pointer is 8 bytes — uintptr_t guarantees the correct width.
 */
typedef uintptr_t   mem_ptr_t;

/* ===========================================================================
 * 2. BYTE ORDER
 *
 * x86_64 is little-endian. lwIP uses these macros to decide whether to
 * byte-swap multi-byte fields in protocol headers.
 * =========================================================================*/

#define BYTE_ORDER      LITTLE_ENDIAN

/*
 * Provide the endian constants that lwIP's arch.h checks for (some lwIP
 * versions guard LITTLE_ENDIAN / BIG_ENDIAN with #ifndef).
 */
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN   1234
#endif
#ifndef BIG_ENDIAN
#define BIG_ENDIAN      4321
#endif

/* ===========================================================================
 * 3. PACKED STRUCT ATTRIBUTE
 *
 * lwIP uses PACK_STRUCT_BEGIN / PACK_STRUCT_END / PACK_STRUCT_FIELD macros
 * to declare protocol header structs without padding. Clang supports GCC's
 * __attribute__((packed)) syntax natively.
 * =========================================================================*/

#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x)    x
#define PACK_STRUCT_STRUCT      __attribute__((packed))

/* ===========================================================================
 * 4. COMPILER / PLATFORM HINTS
 * =========================================================================*/

/** lwIP uses LWIP_UNUSED_ARG() to silence "unused parameter" warnings. */
#define LWIP_UNUSED_ARG(x)      ((void)(x))

/**
 * LWIP_PLATFORM_ASSERT: Called by LWIP_ASSERT macros on failure.
 * In a kernel context we have no printf, so we halt the CPU.
 * Replace the inline asm with your own kernel panic() once it exists.
 */
#define LWIP_PLATFORM_ASSERT(x)                         \
    do {                                                \
        (void)(x);                                      \
        __asm__ volatile ("cli; hlt" : : : "memory");  \
    } while (0)

/**
 * LWIP_PLATFORM_DIAG: Used for lwIP debug/stats output.
 * Silenced here; wire it to your kernel's kprintf() for debugging.
 *
 * CRITICAL: The body must be EMPTY -- do NOT write (void)(x) here.
 * Using (void)(x) forces the C compiler to parse 'x' as an expression,
 * which then fails on lwIP format strings like:
 *   ("val=%"X32_F"\n", val)
 * because the string+macro+string syntax only works via string literal
 * concatenation, which breaks if evaluated as a parenthesized expression.
 *
 * With an empty body, the preprocessor still expands macros inside x,
 * but those expanded tokens are discarded before the C parser sees them.
 *
 * To enable debug output later:
 *   extern void kprintf(const char *fmt, ...);
 *   #define LWIP_PLATFORM_DIAG(x)  kprintf x
 */
#define LWIP_PLATFORM_DIAG(x)   do { } while (0)

/* ===========================================================================
 * 5. ATOMIC / CRITICAL SECTION STUBS
 *
 * lwIP's NO_SYS=1 mode does not use mutexes, but some lwIP distributions
 * still reference SYS_ARCH_DECL_PROTECT / SYS_ARCH_PROTECT macros.
 * In a single-core kernel, disabling interrupts is sufficient. On SMP you
 * will need a real spinlock here.
 * =========================================================================*/

/**
 * SYS_ARCH_DECL_PROTECT(level): Declares a local variable to save RFLAGS.
 * SYS_ARCH_PROTECT(level):      Saves RFLAGS and clears IF (cli).
 * SYS_ARCH_UNPROTECT(level):    Restores RFLAGS (re-enables interrupts if
 *                                they were on before).
 */
typedef uint64_t    sys_prot_t;

#define SYS_ARCH_DECL_PROTECT(level)    sys_prot_t level
#define SYS_ARCH_PROTECT(level)                                 \
    __asm__ volatile (                                          \
        "pushfq\n\t"                                            \
        "popq %0\n\t"                                           \
        "cli"                                                   \
        : "=r"(level) : : "memory", "cc")
#define SYS_ARCH_UNPROTECT(level)                               \
    __asm__ volatile (                                          \
        "pushq %0\n\t"                                          \
        "popfq"                                                 \
        : : "r"(level) : "memory", "cc")

#endif /* CC_H */
