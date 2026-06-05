#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

// =====================================================
// LAYOUT MEMORI KYUZEN OS
// =====================================================
// 0x00000000 - 0x003FFFFF : Kernel Code/Data  (Ring 0, Supervisor)
// 0x00400000 - 0x007FFFFF : Kernel Heap        (Ring 0, Supervisor)
// 0x00800000 - 0x00BFFFFF : User Space Base    (Ring 3, termasuk third_page_table)
// 0x00C00000+             : User Apps Dinamis  (Ring 3, dipetakan saat load ELF)
// 0xFD000000+             : Framebuffer VRAM   (Ring 0, Supervisor)
// =====================================================
#define KERNEL_SPACE_END    0x00800000U  // Batas atas zona Kernel
#define USER_SPACE_START    0x00800000U  // Zona User mulai di 8MB
#define USER_SPACE_MAX      0x02000000U  // Batas atas 32MB (User Apps)
#define MAX_DYNAMIC_TABLES  8            // Kapasitas pool = 8 x 4MB = 32MB

// --- FUNGSI UTAMA PAGING ---
void init_paging(uint32_t fb_phys_addr);

// --- PEMETAAN DINAMIS ---
// Petakan blok 4MB yang mencakup 'vaddr' agar CPU mengenalinya.
// Mengembalikan 1 jika berhasil, 0 jika pool habis.
int paging_map_region(uint32_t vaddr);

// Periksa apakah alamat virtual sudah dipetakan di Page Directory.
// Mengembalikan 1 jika sudah dipetakan, 0 jika belum.
int paging_is_mapped(uint32_t addr);

// --- VIRTUAL MEMORY MANAGER (VMM) ---
void vmm_map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags);
int vmm_alloc_page(uint32_t vaddr, uint32_t flags);
#endif