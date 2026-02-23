#include <kernel/memory/vmm.h>
#include <kernel/arch/paging.h>
#include <kernel/lib/util.h>
#include <kernel/memory/heap.h>
#include <kernel/memory/pmm.h>

static address_space_t kernel_address_space;
static const uint64_t PAGE_PS = 0x080ULL;

static uint64_t alloc_table(uint64_t **out_virt) {
    uint64_t phys = (uint64_t)(uintptr_t)pmm_alloc_page();
    if (!phys) {
        return 0;
    }
    uint64_t *virt = (uint64_t *)(uintptr_t)hhdm_phys_to_virt(phys);
    mem_set((uint8_t *)virt, 0, 0x1000);
    if (out_virt) {
        *out_virt = virt;
    }
    return phys;
}

void vmm_init(uint64_t *kernel_pml4) {
    kernel_address_space.pml4 = kernel_pml4;
}

address_space_t *address_space_kernel(void) {
    return &kernel_address_space;
}

void address_space_switch(address_space_t *as) {
    if (!as || !as->pml4) {
        return;
    }
    uint64_t cr3 = hhdm_virt_to_phys((uint64_t)(uintptr_t)as->pml4);
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

address_space_t *address_space_create(void) {
    address_space_t *as = kmalloc(sizeof(address_space_t));
    if (!as) {
        return 0;
    }
    uint64_t *pml4 = 0;
    if (!alloc_table(&pml4)) {
        return 0;
    }
    uint64_t *kernel_pml4 = paging_kernel_pml4();
    for (uint64_t i = 256; i < 512; i++) {
        pml4[i] = kernel_pml4[i];
    }
    as->pml4 = pml4;
    return as;
}

static uint64_t table_flags(uint64_t flags) {
    uint64_t out = VMM_PAGE_PRESENT | VMM_PAGE_WRITE;
    if (flags & VMM_PAGE_USER) {
        out |= VMM_PAGE_USER;
    }
    return out;
}

static uint64_t split_huge_pd(uint64_t pd_entry, uint64_t **out_virt) {
    uint64_t pt_phys = alloc_table(out_virt);
    if (!pt_phys) {
        return 0;
    }
    uint64_t *pt = *out_virt;
    uint64_t base_phys = pd_entry & ~0x1FFFFFULL;
    uint64_t flags = pd_entry & 0xFFFULL;
    flags &= ~PAGE_PS;
    for (uint64_t i = 0; i < 512; i++) {
        uint64_t phys = base_phys + (i * 0x1000);
        pt[i] = phys | flags;
    }
    return pt_phys;
}

int vmm_map_page(address_space_t *as, uint64_t virt, uint64_t phys, uint64_t flags) {
    if (!as || !as->pml4) {
        return -1;
    }
    uint64_t pml4_index = (virt >> 39) & 0x1FF;
    uint64_t pdpt_index = (virt >> 30) & 0x1FF;
    uint64_t pd_index = (virt >> 21) & 0x1FF;
    uint64_t pt_index = (virt >> 12) & 0x1FF;

    uint64_t *pml4 = as->pml4;
    uint64_t *pdpt;
    if ((pml4[pml4_index] & VMM_PAGE_PRESENT) == 0) {
        uint64_t pdpt_phys = alloc_table(&pdpt);
        if (!pdpt_phys) {
            return -1;
        }
        pml4[pml4_index] = pdpt_phys | table_flags(flags);
    } else {
        pdpt = (uint64_t *)(uintptr_t)hhdm_phys_to_virt(pml4[pml4_index] & ~0xFFFULL);
    }

    uint64_t *pd;
    if ((pdpt[pdpt_index] & VMM_PAGE_PRESENT) == 0) {
        uint64_t pd_phys = alloc_table(&pd);
        if (!pd_phys) {
            return -1;
        }
        pdpt[pdpt_index] = pd_phys | table_flags(flags);
    } else {
        pd = (uint64_t *)(uintptr_t)hhdm_phys_to_virt(pdpt[pdpt_index] & ~0xFFFULL);
    }

    uint64_t *pt;
    if ((pd[pd_index] & VMM_PAGE_PRESENT) == 0) {
        uint64_t pt_phys = alloc_table(&pt);
        if (!pt_phys) {
            return -1;
        }
        pd[pd_index] = pt_phys | table_flags(flags);
    } else if (pd[pd_index] & PAGE_PS) {
        uint64_t *new_pt = 0;
        uint64_t new_pt_phys = split_huge_pd(pd[pd_index], &new_pt);
        if (!new_pt_phys) {
            return -1;
        }
        uint64_t new_flags = pd[pd_index] & 0xFFFULL;
        new_flags &= ~PAGE_PS;
        pd[pd_index] = new_pt_phys | new_flags;
        pt = new_pt;
    } else {
        pt = (uint64_t *)(uintptr_t)hhdm_phys_to_virt(pd[pd_index] & ~0xFFFULL);
    }

    pt[pt_index] = (phys & ~0xFFFULL) | flags | VMM_PAGE_PRESENT;
    return 0;
}

int vmm_unmap_page(address_space_t *as, uint64_t virt) {
    if (!as || !as->pml4) {
        return -1;
    }
    uint64_t pml4_index = (virt >> 39) & 0x1FF;
    uint64_t pdpt_index = (virt >> 30) & 0x1FF;
    uint64_t pd_index = (virt >> 21) & 0x1FF;
    uint64_t pt_index = (virt >> 12) & 0x1FF;

    uint64_t *pml4 = as->pml4;
    if ((pml4[pml4_index] & VMM_PAGE_PRESENT) == 0) {
        return -1;
    }
    uint64_t *pdpt = (uint64_t *)(uintptr_t)hhdm_phys_to_virt(pml4[pml4_index] & ~0xFFFULL);
    if ((pdpt[pdpt_index] & VMM_PAGE_PRESENT) == 0) {
        return -1;
    }
    uint64_t *pd = (uint64_t *)(uintptr_t)hhdm_phys_to_virt(pdpt[pdpt_index] & ~0xFFFULL);
    if ((pd[pd_index] & VMM_PAGE_PRESENT) == 0) {
        return -1;
    }
    if (pd[pd_index] & PAGE_PS) {
        return -1;
    }
    uint64_t *pt = (uint64_t *)(uintptr_t)hhdm_phys_to_virt(pd[pd_index] & ~0xFFFULL);
    pt[pt_index] = 0;
    __asm__ __volatile__("invlpg (%0)" : : "r"(virt) : "memory");
    return 0;
}

int vmm_map_range(address_space_t *as, uint64_t virt, uint64_t phys, uint64_t size, uint64_t flags) {
    uint64_t aligned = align_up(size);
    for (uint64_t off = 0; off < aligned; off += 0x1000) {
        if (vmm_map_page(as, virt + off, phys + off, flags) != 0) {
            return -1;
        }
    }
    return 0;
}
