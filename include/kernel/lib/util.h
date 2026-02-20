#ifndef KERNEL_LIB_UTIL_H
#define KERNEL_LIB_UTIL_H

#include <stdint.h>

uint64_t align_up(uint64_t val);
uint64_t align_down(uint64_t val);
void mem_set(uint8_t *dst, uint8_t value, uint64_t count);

#endif
