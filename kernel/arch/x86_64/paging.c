#include <kernel/arch/paging.h>
#include <kernel/lib/util.h>
#include <kernel/memory/pmm.h>

#define PAGE_PRESENT 0x001ULL
#define PAGE_WRITE 0x002ULL
#define PAGE_PS 0x080ULL
#define PAGE_USER 0x004ULL
#define KERNEL_MAP_SIZE (16ULL * 1024 * 1024)

static uint64_t *paging_pml4 = 0;
static uint64_t g_hhdm_offset = 0;

void paging_set_hhdm_offset(uint64_t hhdm_offset) {
    g_hhdm_offset = hhdm_offset;
}

uint64_t hhdm_phys_to_virt(uint64_t phys) {
    return phys + g_hhdm_offset;
}

uint64_t hhdm_virt_to_phys(uint64_t virt) {
    return virt - g_hhdm_offset;
}

static uint64_t alloc_page_table(uint64_t **out_virt) {
    uint64_t phys = (uint64_t)(uintptr_t)pmm_alloc_page();
    if (!phys) {
        for (;;) {
            __asm__ __volatile__("hlt");
        }
    }
    uint64_t *virt = (uint64_t *)(uintptr_t)hhdm_phys_to_virt(phys);
    mem_set((uint8_t *)virt, 0, 0x1000);
    if (out_virt) {
        *out_virt = virt;
    }
    return phys;
}

static void map_page_4k(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4_index = (virt >> 39) & 0x1FF;
    uint64_t pdpt_index = (virt >> 30) & 0x1FF;
    uint64_t pd_index = (virt >> 21) & 0x1FF;
    uint64_t pt_index = (virt >> 12) & 0x1FF;

    uint64_t *pml4 = paging_pml4;
    uint64_t *pdpt;
    if ((pml4[pml4_index] & PAGE_PRESENT) == 0) {
        uint64_t pdpt_phys = alloc_page_table(&pdpt);
        pml4[pml4_index] = pdpt_phys | PAGE_PRESENT | PAGE_WRITE;
    } else {
        pdpt = (uint64_t *)(uintptr_t)hhdm_phys_to_virt(pml4[pml4_index] & ~0xFFFULL);
    }

    uint64_t *pd;
    if ((pdpt[pdpt_index] & PAGE_PRESENT) == 0) {
        uint64_t pd_phys = alloc_page_table(&pd);
        pdpt[pdpt_index] = pd_phys | PAGE_PRESENT | PAGE_WRITE;
    } else {
        pd = (uint64_t *)(uintptr_t)hhdm_phys_to_virt(pdpt[pdpt_index] & ~0xFFFULL);
    }

    uint64_t *pt;
    if ((pd[pd_index] & PAGE_PRESENT) == 0 || (pd[pd_index] & PAGE_PS)) {
        uint64_t pt_phys = alloc_page_table(&pt);
        pd[pd_index] = pt_phys | PAGE_PRESENT | PAGE_WRITE;
    } else {
        pt = (uint64_t *)(uintptr_t)hhdm_phys_to_virt(pd[pd_index] & ~0xFFFULL);
    }

    pt[pt_index] = (phys & ~0xFFFULL) | flags | PAGE_PRESENT;
}

static void map_2m_region(uint64_t *pd, uint64_t base_phys, uint64_t count, uint64_t flags) {
    for (uint64_t i = 0; i < count; i++) {
        uint64_t phys = base_phys + (i * 0x200000);
        pd[i] = phys | flags | PAGE_PRESENT | PAGE_PS;
    }
}

void paging_init(uint64_t hhdm_offset, uint64_t kernel_phys, uint64_t kernel_virt) {
    g_hhdm_offset = hhdm_offset;

    uint64_t paging_pml4_phys = alloc_page_table(&paging_pml4);

    uint64_t *pdpt_identity = 0;
    uint64_t pdpt_identity_phys = alloc_page_table(&pdpt_identity);
    paging_pml4[0] = pdpt_identity_phys | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;

    uint64_t *pd_identity[4];
    uint64_t pd_identity_phys[4];
    for (uint64_t i = 0; i < 4; i++) {
        pd_identity_phys[i] = alloc_page_table(&pd_identity[i]);
        pdpt_identity[i] = pd_identity_phys[i] | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
        map_2m_region(pd_identity[i], i * 0x40000000ULL, 512, PAGE_WRITE | PAGE_USER);
    }

    uint64_t hhdm_index = (hhdm_offset >> 39) & 0x1FF;
    uint64_t *pdpt_hhdm = 0;
    uint64_t pdpt_hhdm_phys = alloc_page_table(&pdpt_hhdm);
    paging_pml4[hhdm_index] = pdpt_hhdm_phys | PAGE_PRESENT | PAGE_WRITE;
    for (uint64_t i = 0; i < 4; i++) {
        pdpt_hhdm[i] = pd_identity_phys[i] | PAGE_PRESENT | PAGE_WRITE;
    }

    uint64_t kernel_pages = align_up(KERNEL_MAP_SIZE) / 0x1000;
    for (uint64_t i = 0; i < kernel_pages; i++) {
        map_page_4k(kernel_virt + i * 0x1000, kernel_phys + i * 0x1000, PAGE_WRITE);
    }

    __asm__ __volatile__("mov %0, %%cr3" : : "r"(paging_pml4_phys) : "memory");
}

uint64_t *paging_kernel_pml4(void) {
    return paging_pml4;
}
