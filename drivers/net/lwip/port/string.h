/**
 * @file string.h
 * @brief Freestanding stub for <string.h> — Kyuzen OS lwIP porting layer.
 *
 * lwIP's mem.c includes <string.h> for memcpy/memset/memcmp.
 * This stub is placed in drivers/net/lwip/port/ so the -I search path
 * resolves it before any system include, then re-exports the kernel's own
 * freestanding string utilities.
 *
 * The kernel's string library lives at:
 *   include/string.h        (declarations)
 *   kernel/string.c         (implementations)
 *
 * All symbols (memcpy, memmove, memset, memcmp, strlen, strcmp,
 * strncmp, strncpy) are provided by that implementation — no libc needed.
 */

#ifndef KYUZEN_LWIP_STRING_STUB_H
#define KYUZEN_LWIP_STRING_STUB_H

/* Re-export the kernel's own freestanding string utilities.
 * Path from drivers/net/lwip/port/ to the kernel include directory: */
#include "../../../../include/string.h"

#endif /* KYUZEN_LWIP_STRING_STUB_H */
