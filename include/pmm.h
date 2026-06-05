#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PAGE_SIZE 4096            // 1 Page = 4 Kilobytes
#define MAX_MEM_SIZE 0x100000000ULL   // 4GB (Max 32bit architecture)
#define PMM_BITMAP_SIZE (MAX_MEM_SIZE / PAGE_SIZE / 8) // Ukuran array bitmap

// Kontrak fungsi Manajemen Memori Fisik
void pmm_init(void);
void* pmm_alloc_page(void);
void pmm_free_page(void* ptr);

uint32_t pmm_get_used_ram(void);
uint32_t pmm_get_total_ram(void);

void pmm_set_total_ram(uint32_t size);

void pmm_init_dynamic(void* memmap_entries, uint64_t entry_count);
#endif