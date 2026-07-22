#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>  // size_t

typedef struct heap_block {
    uint32_t magic;
    size_t size;               // Ukuran blok data (tidak termasuk header)
    uint8_t is_free;           // 1 = bebas, 0 = terpakai
    struct heap_block* next;   // Pointer ke blok berikutnya di linked list
} heap_block_t;

void  init_heap(void);
void* kmalloc(size_t size);
void  kfree(void* ptr);
void* krealloc(void* ptr, size_t old_size, size_t new_size);

#endif