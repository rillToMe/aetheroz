// ============================================================
// kernel/paging.c — Virtual Memory Manager, Kyuzen OS
//
// SMP-SAFE: All page table operations protected by paging_lock.
// ERROR-RETURNING: vmm_map_page/vmm_unmap_page return int status.
// FULL CLEANUP: vmm_destroy_address_space frees entire hierarchy.
// PER-PROCESS: vmm_create_address_space clones kernel mappings.
//
// ARCHITECTURE:
//   - 4-level page table walk (PML4 → PDPT → PD → PT)
//   - HHDM for accessing page tables (phys + hhdm_offset)
//   - Kernel mappings in PML4[256..511] (higher-half)
//   - User mappings in PML4[0..255] (lower-half)
// ============================================================

#include "paging.h"
#include "pmm.h"
#include "string.h"
#include "spinlock.h"
#include "smp.h"
#include <stdint.h>

// HHDM offset dari kernel.c: physical P accessible di hhdm_offset + P
extern uint64_t hhdm_offset;

// Macro konversi: physical address → virtual address via HHDM
#define PHYS_TO_VIRT(phys) ((uint64_t)(phys) + hhdm_offset)
// Inverse: virtual (HHDM) → physical
#define VIRT_TO_PHYS(virt) ((uint64_t)(virt) - hhdm_offset)

// Current PML4: disimpan sebagai VIRTUAL address (sudah ditambah HHDM)
uint64_t* current_pml4 = 0;

// Boot kernel PML4 physical address — saved at init, never changes.
// Used to restore kernel AS when returning from user processes.
static phys_addr_t kernel_pml4_phys = PHYS_NULL;

// SMP-safe lock for all page table operations
static spinlock_t paging_lock = SPINLOCK_INIT;

// ============================================================
// TLB MANAGEMENT
// ============================================================

void vmm_tlb_shootdown(uint64_t vaddr) {
    // Current CPU only — future: IPI to other CPUs sharing this AS
    __asm__ volatile("invlpg (%0)" ::"r"(vaddr) : "memory");
}

void vmm_flush_tlb_all(void) {
    __asm__ volatile(
        "mov %%cr3, %%rax\n"
        "mov %%rax, %%cr3\n"
        ::: "rax", "memory"
    );
}

phys_addr_t vmm_read_cr3(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return (phys_addr_t)(cr3 & 0xFFFFFFFFFFFFF000ULL);
}

// ============================================================
// INIT
// ============================================================

void init_paging(uint32_t unused) {
    (void)unused;
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    uint64_t phys_pml4 = cr3 & 0xFFFFFFFFFFFFF000ULL;
    current_pml4 = (uint64_t*)PHYS_TO_VIRT(phys_pml4);
    kernel_pml4_phys = (phys_addr_t)phys_pml4;  // Save boot PML4 forever
}

// ============================================================
// HELPERS (caller MUST hold paging_lock)
// ============================================================

static uint64_t* alloc_page_virt(void) {
    phys_addr_t phys = pmm_alloc_page();
    if (phys == PHYS_NULL) return 0;
    uint64_t* virt = (uint64_t*)PHYS_TO_VIRT(phys);
    memset(virt, 0, 4096);
    return virt;
}

// Page table index extraction
#define PML4_IDX(va) (((va) >> 39) & 0x1FF)
#define PDPT_IDX(va) (((va) >> 30) & 0x1FF)
#define PD_IDX(va)   (((va) >> 21) & 0x1FF)
#define PT_IDX(va)   (((va) >> 12) & 0x1FF)
#define PAGE_MASK     0xFFFFFFFFFFFFF000ULL

// ============================================================
// vmm_map_page — Map vaddr → paddr with flags
//
// Returns: 1 = success, 0 = failure (OOM or huge page collision)
// Caller MUST NOT hold paging_lock (we acquire it here).
// ============================================================
int vmm_map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    if (!current_pml4) return 0;

    uint64_t pml4_idx = PML4_IDX(vaddr);
    uint64_t pdpt_idx = PDPT_IDX(vaddr);
    uint64_t pd_idx   = PD_IDX(vaddr);
    uint64_t pt_idx   = PT_IDX(vaddr);

    uint64_t irq = spinlock_lock_irqsave(&paging_lock);

    // Level 4 (PML4) → Level 3 (PDPT)
    if (!(current_pml4[pml4_idx] & 1)) {
        uint64_t* new_pdpt = alloc_page_virt();
        if (!new_pdpt) {
            spinlock_unlock_irqrestore(&paging_lock, irq);
            return 0;
        }
        current_pml4[pml4_idx] = VIRT_TO_PHYS(new_pdpt) | 7;
    }
    uint64_t* pdpt = (uint64_t*)PHYS_TO_VIRT(current_pml4[pml4_idx] & PAGE_MASK);

    // Level 3 (PDPT) → Level 2 (PD)
    if (pdpt[pdpt_idx] & (1ULL << 7)) {
        // 1GB huge page — cannot split
        spinlock_unlock_irqrestore(&paging_lock, irq);
        return 0;
    }
    if (!(pdpt[pdpt_idx] & 1)) {
        uint64_t* new_pd = alloc_page_virt();
        if (!new_pd) {
            spinlock_unlock_irqrestore(&paging_lock, irq);
            return 0;
        }
        pdpt[pdpt_idx] = VIRT_TO_PHYS(new_pd) | 7;
    }
    uint64_t* pd = (uint64_t*)PHYS_TO_VIRT(pdpt[pdpt_idx] & PAGE_MASK);

    // Level 2 (PD) → Level 1 (PT)
    if (pd[pd_idx] & (1ULL << 7)) {
        // 2MB huge page — cannot split
        spinlock_unlock_irqrestore(&paging_lock, irq);
        return 0;
    }
    if (!(pd[pd_idx] & 1)) {
        uint64_t* new_pt = alloc_page_virt();
        if (!new_pt) {
            spinlock_unlock_irqrestore(&paging_lock, irq);
            return 0;
        }
        pd[pd_idx] = VIRT_TO_PHYS(new_pt) | 7;
    }
    uint64_t* pt = (uint64_t*)PHYS_TO_VIRT(pd[pd_idx] & PAGE_MASK);

    // Install the mapping
    pt[pt_idx] = (paddr & PAGE_MASK) | flags;

    spinlock_unlock_irqrestore(&paging_lock, irq);

    vmm_tlb_shootdown(vaddr);
    return 1;
}

// ============================================================
// vmm_unmap_page — Remove mapping for vaddr
//
// Returns: 1 if unmapped, 0 if not mapped
// ============================================================
int vmm_unmap_page(uint64_t vaddr) {
    if (!current_pml4) return 0;

    uint64_t pml4_idx = PML4_IDX(vaddr);
    uint64_t pdpt_idx = PDPT_IDX(vaddr);
    uint64_t pd_idx   = PD_IDX(vaddr);
    uint64_t pt_idx   = PT_IDX(vaddr);

    uint64_t irq = spinlock_lock_irqsave(&paging_lock);

    if (!(current_pml4[pml4_idx] & 1)) {
        spinlock_unlock_irqrestore(&paging_lock, irq);
        return 0;
    }
    uint64_t* pdpt = (uint64_t*)PHYS_TO_VIRT(current_pml4[pml4_idx] & PAGE_MASK);

    if (!(pdpt[pdpt_idx] & 1)) {
        spinlock_unlock_irqrestore(&paging_lock, irq);
        return 0;
    }
    if (pdpt[pdpt_idx] & (1ULL << 7)) {
        spinlock_unlock_irqrestore(&paging_lock, irq);
        return 0; // huge page
    }
    uint64_t* pd = (uint64_t*)PHYS_TO_VIRT(pdpt[pdpt_idx] & PAGE_MASK);

    if (!(pd[pd_idx] & 1)) {
        spinlock_unlock_irqrestore(&paging_lock, irq);
        return 0;
    }
    if (pd[pd_idx] & (1ULL << 7)) {
        spinlock_unlock_irqrestore(&paging_lock, irq);
        return 0; // huge page
    }
    uint64_t* pt = (uint64_t*)PHYS_TO_VIRT(pd[pd_idx] & PAGE_MASK);

    if (!(pt[pt_idx] & 1)) {
        spinlock_unlock_irqrestore(&paging_lock, irq);
        return 0;
    }

    pt[pt_idx] = 0;

    spinlock_unlock_irqrestore(&paging_lock, irq);

    vmm_tlb_shootdown(vaddr);
    return 1;
}

// ============================================================
// vmm_alloc_page — Allocate physical page + map to vaddr
// ============================================================
int vmm_alloc_page(uint64_t vaddr, uint64_t flags) {
    phys_addr_t paddr = pmm_alloc_page();
    if (paddr == PHYS_NULL) return 0;
    if (!vmm_map_page(vaddr, paddr, flags)) {
        pmm_free_page(paddr);
        return 0;
    }
    return 1;
}

int paging_map_region(uint64_t vaddr) {
    return vmm_alloc_page(vaddr, 7);
}

// ============================================================
// paging_is_mapped — Check if vaddr has a mapping (SMP-safe)
// ============================================================
int paging_is_mapped(uint64_t vaddr) {
    if (!current_pml4) return 0;

    uint64_t irq = spinlock_lock_irqsave(&paging_lock);

    uint64_t pml4_idx = PML4_IDX(vaddr);
    if (!(current_pml4[pml4_idx] & 1)) {
        spinlock_unlock_irqrestore(&paging_lock, irq);
        return 0;
    }
    uint64_t* pdpt = (uint64_t*)PHYS_TO_VIRT(current_pml4[pml4_idx] & PAGE_MASK);

    uint64_t pdpt_idx = PDPT_IDX(vaddr);
    if (!(pdpt[pdpt_idx] & 1)) {
        spinlock_unlock_irqrestore(&paging_lock, irq);
        return 0;
    }
    if (pdpt[pdpt_idx] & (1ULL << 7)) {
        spinlock_unlock_irqrestore(&paging_lock, irq);
        return 1; // 1GB huge page is "mapped"
    }
    uint64_t* pd = (uint64_t*)PHYS_TO_VIRT(pdpt[pdpt_idx] & PAGE_MASK);

    uint64_t pd_idx = PD_IDX(vaddr);
    if (!(pd[pd_idx] & 1)) {
        spinlock_unlock_irqrestore(&paging_lock, irq);
        return 0;
    }
    if (pd[pd_idx] & (1ULL << 7)) {
        spinlock_unlock_irqrestore(&paging_lock, irq);
        return 1; // 2MB huge page is "mapped"
    }
    uint64_t* pt = (uint64_t*)PHYS_TO_VIRT(pd[pd_idx] & PAGE_MASK);

    uint64_t pt_idx = PT_IDX(vaddr);
    int result = (pt[pt_idx] & 1) ? 1 : 0;

    spinlock_unlock_irqrestore(&paging_lock, irq);
    return result;
}

// ============================================================
// vmm_create_address_space — Create new PML4 for a process
//
// Allocates a fresh PML4 page and clones kernel mappings
// (PML4 entries 256..511) from the current (boot) PML4.
//
// Returns: physical address of new PML4, or PHYS_NULL on failure.
// ============================================================
phys_addr_t vmm_create_address_space(void) {
    if (!current_pml4) return PHYS_NULL;

    uint64_t irq = spinlock_lock_irqsave(&paging_lock);

    // Allocate new PML4
    uint64_t* new_pml4 = alloc_page_virt();
    if (!new_pml4) {
        spinlock_unlock_irqrestore(&paging_lock, irq);
        return PHYS_NULL;
    }

    // Clone kernel mappings: PML4[256..511] (higher-half)
    for (int i = 256; i < 512; i++) {
        new_pml4[i] = current_pml4[i];
    }

    // User entries [0..255] are already zero (alloc_page_virt zeroes)
    phys_addr_t new_pml4_phys = VIRT_TO_PHYS(new_pml4);

    spinlock_unlock_irqrestore(&paging_lock, irq);
    return new_pml4_phys;
}

// ============================================================
// vmm_destroy_address_space — Free entire page table hierarchy
//
// Walks PML4 entries 0..255 (user range) and frees:
//   - All 4KB data pages (PT entries)
//   - All PT pages
//   - All PD pages
//   - All PDPT pages
//   - Optionally the PML4 page itself
//
// Does NOT touch kernel mappings (entries 256..511).
// ============================================================
void vmm_destroy_address_space(phys_addr_t pml4_phys, int free_pml4) {
    if (pml4_phys == PHYS_NULL) return;

    uint64_t* pml4 = (uint64_t*)PHYS_TO_VIRT(pml4_phys);

    uint64_t irq = spinlock_lock_irqsave(&paging_lock);

    // Walk user-half PML4 entries: 0..255
    for (int p4 = 0; p4 < 256; p4++) {
        if (!(pml4[p4] & 1)) continue;

        phys_addr_t pdpt_phys = pml4[p4] & PAGE_MASK;
        uint64_t* pdpt = (uint64_t*)PHYS_TO_VIRT(pdpt_phys);

        // Skip 1GB huge pages (shouldn't exist in user space, but guard)
        if (pml4[p4] & (1ULL << 7)) {
            pml4[p4] = 0;
            continue;
        }

        for (int p3 = 0; p3 < 512; p3++) {
            if (!(pdpt[p3] & 1)) continue;

            phys_addr_t pd_phys = pdpt[p3] & PAGE_MASK;

            // Skip 1GB huge pages
            if (pdpt[p3] & (1ULL << 7)) {
                pdpt[p3] = 0;
                continue;
            }

            uint64_t* pd = (uint64_t*)PHYS_TO_VIRT(pd_phys);

            for (int p2 = 0; p2 < 512; p2++) {
                if (!(pd[p2] & 1)) continue;

                phys_addr_t pt_phys = pd[p2] & PAGE_MASK;

                // Skip 2MB huge pages — just clear entry
                if (pd[p2] & (1ULL << 7)) {
                    pd[p2] = 0;
                    continue;
                }

                uint64_t* pt = (uint64_t*)PHYS_TO_VIRT(pt_phys);

                // Free every 4KB data page in this PT
                for (int p1 = 0; p1 < 512; p1++) {
                    if (!(pt[p1] & 1)) continue;
                    phys_addr_t page_phys = pt[p1] & PAGE_MASK;
                    pmm_free_page(page_phys);
                    pt[p1] = 0;
                }

                // Free the PT page itself
                pmm_free_page(pt_phys);
                pd[p2] = 0;
            }

            // Free the PD page itself
            pmm_free_page(pd_phys);
            pdpt[p3] = 0;
        }

        // Free the PDPT page itself
        pmm_free_page(pdpt_phys);
        pml4[p4] = 0;
    }

    spinlock_unlock_irqrestore(&paging_lock, irq);

    // Optionally free the PML4 page
    if (free_pml4) {
        pmm_free_page(pml4_phys);
    }

    // Flush TLB since we just nuked mappings
    vmm_flush_tlb_all();
}

// ============================================================
// vmm_unmap_user_space — Legacy wrapper
//
// Cleans user-range mappings in the GLOBAL current_pml4.
// Does NOT free the PML4 page itself (it's the boot PML4).
// Called from sys_exec / sys_exit when no per-process PML4 exists.
// ============================================================
void vmm_unmap_user_space(void) {
    if (!current_pml4) return;

    // Compute physical address of the global PML4
    phys_addr_t pml4_phys = VIRT_TO_PHYS(current_pml4);

    // Destroy user mappings but keep the PML4 page
    vmm_destroy_address_space(pml4_phys, 0);
}

// ============================================================
// vmm_switch_pml4 — Switch CR3 + current_pml4 to a given PML4
// ============================================================
void vmm_switch_pml4(phys_addr_t pml4_phys) {
    if (pml4_phys == PHYS_NULL) return;

    current_pml4 = (uint64_t*)PHYS_TO_VIRT(pml4_phys);
    __asm__ volatile("mov %0, %%cr3" :: "r"((uint64_t)pml4_phys) : "memory");

    uint32_t cpu_id = smp_current_cpu_index();
    percpu_t *cpu = smp_get_cpu(cpu_id);
    if (cpu) cpu->current_cr3 = (uint64_t)pml4_phys;
}

// ============================================================
// vmm_switch_to_kernel_as — Restore boot kernel PML4
// ============================================================
void vmm_switch_to_kernel_as(void) {
    if (kernel_pml4_phys == PHYS_NULL) return;

    current_pml4 = (uint64_t*)PHYS_TO_VIRT(kernel_pml4_phys);
    __asm__ volatile("mov %0, %%cr3" :: "r"((uint64_t)kernel_pml4_phys) : "memory");

    uint32_t cpu_id = smp_current_cpu_index();
    percpu_t *cpu = smp_get_cpu(cpu_id);
    if (cpu) cpu->current_cr3 = (uint64_t)kernel_pml4_phys;
}

// ============================================================
// vmm_get_kernel_pml4_phys — Return boot PML4 physical address
// ============================================================
phys_addr_t vmm_get_kernel_pml4_phys(void) {
    return kernel_pml4_phys;
}

// ============================================================
// vmm_destroy_task_as — Destroy a task's AS and restore kernel
//
// Safe to call even if current_pml4 points to the task's PML4.
// Flow: switch to kernel PML4 first → then destroy old PML4.
// ============================================================
void vmm_destroy_task_as(phys_addr_t pml4_phys) {
    if (pml4_phys == PHYS_NULL) return;

    // Switch to kernel PML4 BEFORE destroying (since current_pml4
    // might point into the PML4 we're about to free)
    vmm_switch_to_kernel_as();

    // Now safe to destroy the old address space
    vmm_destroy_address_space(pml4_phys, 1);
}
