#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>

void init_heap(void);
void* kmalloc(size_t size);
void kfree(void* ptr);

#endif