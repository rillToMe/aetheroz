#ifndef KERNEL_ARCH_SERIAL_H
#define KERNEL_ARCH_SERIAL_H

#include <stdint.h>

void serial_init(void);
void serial_write(char c);
void serial_write_hex64(uint64_t value);

#endif
