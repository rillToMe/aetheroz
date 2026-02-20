#ifndef KERNEL_MEMORY_MEMMAP_H
#define KERNEL_MEMORY_MEMMAP_H

#include <stdint.h>
#include <kernel/boot/limine.h>

#define MAX_USABLE_REGIONS 64

struct usable_region {
    uint64_t base;
    uint64_t length;
};

void memmap_init(struct limine_memmap_response *memmap);
uint64_t memmap_total_usable(void);
struct usable_region *memmap_usable_regions(void);
uint64_t memmap_usable_region_count(void);

#endif
