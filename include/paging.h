#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include "pmm.h"

// --- PAGING FLAGS ---
#define PAGING_PRESENT   (1ULL << 0)
#define PAGING_WRITABLE  (1ULL << 1)
#define PAGING_USER      (1ULL << 2)
#define PAGING_DEFAULT   (PAGING_PRESENT | PAGING_WRITABLE | PAGING_USER)

// --- FUNGSI UTAMA PAGING 64-BIT ---
void init_paging(uint32_t fb_phys_addr);

// Map: returns 1 on success, 0 on failure (alloc fail, huge page collision)
int  vmm_map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags);

// Unmap a single page: returns 1 if unmapped, 0 if not mapped
int  vmm_unmap_page(uint64_t vaddr);

// Allocate a physical page and map it to vaddr: returns 1/0
int  vmm_alloc_page(uint64_t vaddr, uint64_t flags);

// Convenience: alloc + map with default flags (RW+Present)
int  paging_map_region(uint64_t vaddr);

// Check if virtual address has a mapping (SMP-safe)
int  paging_is_mapped(uint64_t vaddr);

// --- ADDRESS SPACE MANAGEMENT ---

// Create a new PML4 with kernel mappings cloned.
// Returns physical address of new PML4, or PHYS_NULL on failure.
phys_addr_t vmm_create_address_space(void);

// Destroy an entire address space:
//   - Frees all user-range pages (PML4 indices 0..255)
//   - Frees PT, PD, PDPT hierarchy pages
//   - If free_pml4=1, also frees the PML4 page itself
void vmm_destroy_address_space(phys_addr_t pml4_phys, int free_pml4);

// Legacy wrapper: clean user-range mappings in current (global) PML4.
// Does NOT free the PML4 page itself.
void vmm_unmap_user_space(void);

// --- TLB MANAGEMENT ---

// Invalidate TLB for a single virtual address (current CPU)
void vmm_tlb_shootdown(uint64_t vaddr);

// Full TLB flush via CR3 reload (current CPU)
void vmm_flush_tlb_all(void);

// Read current CR3 (physical address)
phys_addr_t vmm_read_cr3(void);

#endif