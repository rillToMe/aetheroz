#ifndef KERNEL_MEMORY_PMM_H
#define KERNEL_MEMORY_PMM_H

#include <stdint.h>
#include <kernel/memory/memmap.h>

void pmm_init(uint64_t total_usable, struct usable_region *regions, uint64_t region_count);
void *pmm_alloc_pages(uint64_t pages);
void *pmm_alloc_page(void);
void pmm_free_page(void *addr);
uint64_t pmm_free_pages(void);

#endif
