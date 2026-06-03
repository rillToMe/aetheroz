#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PAGE_SIZE 4096            // 1 Page = 4 Kilobytes
#define MAX_MEM_SIZE 0x10000000   // 256 MB (Bisa disesuaikan nanti dari bacaan GRUB/Multiboot)
#define PMM_BITMAP_SIZE (MAX_MEM_SIZE / PAGE_SIZE / 8) // Ukuran array bitmap

// Kontrak fungsi Manajemen Memori Fisik
void pmm_init(void);
void* pmm_alloc_page(void);
void pmm_free_page(void* ptr);

uint32_t pmm_get_used_ram(void);
uint32_t pmm_get_total_ram(void);
#endif