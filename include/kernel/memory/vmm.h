#pragma once
#include <stdint.h>

typedef struct address_space {
    uint64_t *pml4;
} address_space_t;

#define VMM_PAGE_PRESENT 0x001ULL
#define VMM_PAGE_WRITE 0x002ULL
#define VMM_PAGE_USER 0x004ULL

void vmm_init(uint64_t *kernel_pml4);
address_space_t *address_space_kernel(void);
address_space_t *address_space_create(void);
void address_space_switch(address_space_t *as);
int vmm_map_page(address_space_t *as, uint64_t virt, uint64_t phys, uint64_t flags);
int vmm_unmap_page(address_space_t *as, uint64_t virt);
int vmm_map_range(address_space_t *as, uint64_t virt, uint64_t phys, uint64_t size, uint64_t flags);
