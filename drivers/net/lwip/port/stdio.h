/**
 * @file stdio.h
 * @brief Freestanding stub for <stdio.h> — Kyuzen OS lwIP porting layer.
 *
 * lwIP's mem.c unconditionally includes <stdio.h> for snprintf().
 * In a freestanding kernel, <stdio.h> does not exist. This stub is
 * placed in drivers/net/lwip/port/ so the -I path finds it before
 * any system include directory, satisfying the #include without pulling
 * in the hosted libc.
 *
 * snprintf() is only used inside mem.c's MEM_OVERFLOW_CHECK paths (guarded
 * by #if MEM_OVERFLOW_CHECK). Since MEM_OVERFLOW_CHECK=0 by default (and we
 * do not enable it in lwipopts.h), this function body is never compiled.
 * We still provide a declaration so the translation unit compiles cleanly.
 *
 * DO NOT add printf(), scanf(), or any other hosted I/O here — this stub
 * exists solely to satisfy the linker dependency chain, not to provide
 * actual I/O functionality.
 */

#ifndef KYUZEN_LWIP_STDIO_STUB_H
#define KYUZEN_LWIP_STDIO_STUB_H

#include <stddef.h>   /* size_t  -- freestanding */
#include <stdarg.h>   /* va_list -- freestanding */

/**
 * snprintf stub — required by mem.c mem_overflow_check_raw().
 *
 * This path is only reachable when MEM_OVERFLOW_CHECK != 0, which we
 * keep disabled in lwipopts.h. The implementation here is a safety net
 * in case the guard is ever enabled: it writes a null-terminated empty
 * string and returns 0, preventing any actual formatted output.
 */
static inline int snprintf(char *buf, size_t size,
                           const char *fmt, ...) {
    (void)fmt;
    if (buf != (void *)0 && size > 0) {
        buf[0] = '\0';
    }
    return 0;
}

/**
 * vsnprintf stub — some lwIP paths may call this variant.
 */
static inline int vsnprintf(char *buf, size_t size,
                            const char *fmt, va_list args) {
    (void)fmt;
    (void)args;
    if (buf != (void *)0 && size > 0) {
        buf[0] = '\0';
    }
    return 0;
}

#endif /* KYUZEN_LWIP_STDIO_STUB_H */
