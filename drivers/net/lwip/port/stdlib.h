/**
 * @file stdlib.h
 * @brief Freestanding stub for <stdlib.h> — Kyuzen OS lwIP porting layer.
 *
 * lwIP's mem.c conditionally includes <stdlib.h> when MEM_LIBC_MALLOC=1.
 * Since we set MEM_LIBC_MALLOC=0 in lwipopts.h, this path is never taken.
 * This stub exists as a safety net in case another lwIP module includes
 * it unconditionally in a future lwIP version.
 *
 * Do NOT implement malloc/free/abort here — the kernel has its own
 * memory allocator. If lwIP ever needs MEM_LIBC_MALLOC=1, wire
 * MEM_CUSTOM_MALLOC and MEM_CUSTOM_FREE to your kernel PMM instead.
 */

#ifndef KYUZEN_LWIP_STDLIB_STUB_H
#define KYUZEN_LWIP_STDLIB_STUB_H

#include <stddef.h>   /* size_t -- freestanding */

/* malloc/free/abort are NOT available in this freestanding kernel.
 * If you see a link error for these symbols, ensure MEM_LIBC_MALLOC=0
 * in lwipopts.h so lwIP uses its built-in heap allocator. */

/**
 * atoi -- convert a decimal ASCII string to int.
 *
 * Used by lwIP's netif.c (netif_find) to parse interface numbers from
 * names like "kz0", "eth1", etc. Implemented here as a static inline
 * so it is available to every lwIP translation unit that includes
 * <stdlib.h> without causing multiple-definition link errors.
 *
 * Behaviour matches the C standard:
 *   - Skips leading whitespace
 *   - Handles an optional leading '+' or '-'
 *   - Stops at the first non-digit character
 *   - Returns 0 for an empty or non-numeric string
 */
static inline int atoi(const char *s) {
    int result = 0;
    int sign   = 1;

    /* skip leading whitespace */
    while (*s == ' ' || *s == '\t' || *s == '\n' ||
           *s == '\r' || *s == '\f' || *s == '\v') {
        s++;
    }

    /* optional sign */
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }

    /* consume digits */
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }

    return sign * result;
}

#endif /* KYUZEN_LWIP_STDLIB_STUB_H */
