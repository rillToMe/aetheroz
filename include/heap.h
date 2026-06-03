#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>

typedef struct heap_block {
    uint32_t size;              
    uint8_t is_free;            
    struct heap_block* next;    
} heap_block_t;

void init_heap(void);
void* kmalloc(uint32_t size);
void kfree(void* ptr);
void* krealloc(void* ptr, uint32_t old_size, uint32_t new_size); // FITUR BARU!

#endif