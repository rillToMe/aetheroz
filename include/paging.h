#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

// --- FUNGSI UTAMA PAGING 64-BIT ---
void init_paging(uint32_t fb_phys_addr);
void vmm_map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags);
int vmm_alloc_page(uint64_t vaddr, uint64_t flags);
int paging_map_region(uint64_t vaddr);
int paging_is_mapped(uint64_t vaddr);

#endif