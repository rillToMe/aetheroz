#include "paging.h"
#include "pmm.h"
#include "string.h"
#include <stdint.h>

// HHDM offset dari kernel.c: physical P accessible di hhdm_offset + P
extern uint64_t hhdm_offset;

// Macro konversi: physical address → virtual address via HHDM
#define PHYS_TO_VIRT(phys) ((uint64_t)(phys) + hhdm_offset)
// Inverse: virtual (HHDM) → physical
#define VIRT_TO_PHYS(virt) ((uint64_t)(virt) - hhdm_offset)

// Current PML4: disimpan sebagai VIRTUAL address (sudah ditambah HHDM)
uint64_t* current_pml4 = 0;

void init_paging(uint32_t unused) {
    (void)unused;
    // Baca CR3 (physical address PML4), lalu konversi ke virtual via HHDM
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    uint64_t phys_pml4 = cr3 & 0xFFFFFFFFFFFFF000ULL;
    current_pml4 = (uint64_t*)PHYS_TO_VIRT(phys_pml4);
}

// Helper: alokasi page fisik dan kembalikan virtual address-nya (via HHDM)
static uint64_t* alloc_page_virt(void) {
    phys_addr_t phys = pmm_alloc_page();
    if (phys == PHYS_NULL) return 0;
    uint64_t* virt = (uint64_t*)PHYS_TO_VIRT(phys);
    memset(virt, 0, 4096);
    return virt;
}

// ===================================================================
// VMM 64-BIT: Walk 4-Level Page Tables menggunakan HHDM untuk akses
// ===================================================================
void vmm_map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    if (!current_pml4) return;

    uint64_t pml4_idx = (vaddr >> 39) & 0x1FF;
    uint64_t pdpt_idx = (vaddr >> 30) & 0x1FF;
    uint64_t pd_idx   = (vaddr >> 21) & 0x1FF;
    uint64_t pt_idx   = (vaddr >> 12) & 0x1FF;

    // Level 4 (PML4) → Level 3 (PDPT)
    if (!(current_pml4[pml4_idx] & 1)) {
        uint64_t* new_pdpt = alloc_page_virt();
        if (!new_pdpt) return;
        // Simpan physical address di PTE (bukan virtual!)
        current_pml4[pml4_idx] = VIRT_TO_PHYS(new_pdpt) | 7;
    }
    // Akses PDPT via virtual address
    uint64_t* pdpt = (uint64_t*)PHYS_TO_VIRT(current_pml4[pml4_idx] & 0xFFFFFFFFFFFFF000ULL);

    // Level 3 (PDPT) → Level 2 (PD)
    // Guard: bit 7 (PS) = 1GB huge page — jangan dereference!
    if (pdpt[pdpt_idx] & (1ULL << 7)) return; // huge page, tidak bisa split
    if (!(pdpt[pdpt_idx] & 1)) {
        uint64_t* new_pd = alloc_page_virt();
        if (!new_pd) return;
        pdpt[pdpt_idx] = VIRT_TO_PHYS(new_pd) | 7;
    }
    uint64_t* pd = (uint64_t*)PHYS_TO_VIRT(pdpt[pdpt_idx] & 0xFFFFFFFFFFFFF000ULL);

    // Level 2 (PD) → Level 1 (PT)
    // Guard: bit 7 (PS) = 2MB huge page — jangan dereference!
    if (pd[pd_idx] & (1ULL << 7)) return; // huge page, tidak bisa split
    if (!(pd[pd_idx] & 1)) {
        uint64_t* new_pt = alloc_page_virt();
        if (!new_pt) return;
        pd[pd_idx] = VIRT_TO_PHYS(new_pt) | 7;
    }
    uint64_t* pt = (uint64_t*)PHYS_TO_VIRT(pd[pd_idx] & 0xFFFFFFFFFFFFF000ULL);

    // Tanamkan physical address di PT (PTE menyimpan phys, bukan virt)
    pt[pt_idx] = (paddr & 0xFFFFFFFFFFFFF000ULL) | flags;

    __asm__ volatile("invlpg (%0)" ::"r"(vaddr) : "memory");
}

// Wrapper: alokasi halaman fisik dari PMM dan map ke vaddr
int vmm_alloc_page(uint64_t vaddr, uint64_t flags) {
    phys_addr_t paddr = pmm_alloc_page();
    if (paddr == PHYS_NULL) return 0;
    vmm_map_page(vaddr, paddr, flags);
    return 1;
}

int paging_map_region(uint64_t vaddr) {
    return vmm_alloc_page(vaddr, 7);
}

// Cek apakah virtual address sudah punya mapping
int paging_is_mapped(uint64_t vaddr) {
    if (!current_pml4) return 0;

    uint64_t pml4_idx = (vaddr >> 39) & 0x1FF;
    if (!(current_pml4[pml4_idx] & 1)) return 0;
    uint64_t* pdpt = (uint64_t*)PHYS_TO_VIRT(current_pml4[pml4_idx] & 0xFFFFFFFFFFFFF000ULL);

    uint64_t pdpt_idx = (vaddr >> 30) & 0x1FF;
    if (!(pdpt[pdpt_idx] & 1)) return 0;
    uint64_t* pd = (uint64_t*)PHYS_TO_VIRT(pdpt[pdpt_idx] & 0xFFFFFFFFFFFFF000ULL);

    uint64_t pd_idx = (vaddr >> 21) & 0x1FF;
    if (!(pd[pd_idx] & 1)) return 0;
    uint64_t* pt = (uint64_t*)PHYS_TO_VIRT(pd[pd_idx] & 0xFFFFFFFFFFFFF000ULL);

    uint64_t pt_idx = (vaddr >> 12) & 0x1FF;
    return (pt[pt_idx] & 1);
}

// ===================================================================
// vmm_unmap_user_space() — Bebaskan physical pages milik user app SAJA
//
// HANYA menghapus mapping di range 0x4000000–0x4FFFFFF (app virtual slot).
// TIDAK menyentuh mapping Limine (identity map, framebuffer, MMIO).
//
// Sebelumnya: iterasi PML4[0-255] → hancurkan SEMUA low-half mappings
//   → free pages milik Limine → corrupt kernel → BSOD
//
// Sekarang: surgical unmap — hanya pages yang kita alokasi via vmm_alloc_page
//   untuk ELF segments di 0x4000000.
// ===================================================================
void vmm_unmap_user_space(void) {
    if (!current_pml4) return;
    extern void pmm_free_page(phys_addr_t addr);

    // App virtual range: 0x4000000 – 0x4FFFFFF (16MB slot)
    // PML4[0] → PDPT[0] → PD[32..39] → PT[0..511]
    //
    // 0x4000000 >> 39 = 0  → PML4 index 0
    // 0x4000000 >> 30 = 0  → PDPT index 0
    // 0x4000000 >> 21 = 32 → PD index 32
    // 0x4FFFFFF >> 21 = 39  → PD index 39

    // Check PML4[0] exists
    if (!(current_pml4[0] & 1)) return;
    uint64_t* pdpt = (uint64_t*)PHYS_TO_VIRT(current_pml4[0] & 0xFFFFFFFFFFFFF000ULL);

    // Check PDPT[0] exists (not huge page)
    if (!(pdpt[0] & 1)) return;
    if (pdpt[0] & (1ULL << 7)) return; // 1GB huge page, don't touch

    uint64_t* pd = (uint64_t*)PHYS_TO_VIRT(pdpt[0] & 0xFFFFFFFFFFFFF000ULL);

    // Iterate PD entries 32–39 (covers 0x4000000–0x4FFFFFF)
    for (int p2 = 32; p2 < 40; p2++) {
        if (!(pd[p2] & 1)) continue;

        // Skip 2MB huge pages (bukan milik kita)
        if (pd[p2] & (1ULL << 7)) {
            pd[p2] = 0;
            continue;
        }

        uint64_t pt_phys = pd[p2] & 0xFFFFFFFFFFFFF000ULL;
        uint64_t* pt = (uint64_t*)PHYS_TO_VIRT(pt_phys);

        // Free setiap 4KB page di PT
        for (int p1 = 0; p1 < 512; p1++) {
            if (!(pt[p1] & 1)) continue;
            uint64_t page_phys = pt[p1] & 0xFFFFFFFFFFFFF000ULL;
            pmm_free_page(page_phys);
            pt[p1] = 0;
        }

        // Free PT page sendiri
        pmm_free_page(pt_phys);
        pd[p2] = 0;
    }

    // Flush TLB
    __asm__ volatile(
        "mov %%cr3, %%rax\n"
        "mov %%rax, %%cr3\n"
        ::: "rax", "memory"
    );
}