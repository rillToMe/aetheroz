#pragma once
#include <stdint.h>

void heap_init(void);
void *kmalloc(uint64_t size);
