#ifndef KERNEL_ARCH_PAGING_H
#define KERNEL_ARCH_PAGING_H

#include <stdint.h>

void paging_set_hhdm_offset(uint64_t hhdm_offset);
uint64_t hhdm_phys_to_virt(uint64_t phys);
uint64_t hhdm_virt_to_phys(uint64_t virt);
void paging_init(uint64_t hhdm_offset, uint64_t kernel_phys, uint64_t kernel_virt);
uint64_t *paging_kernel_pml4(void);

#endif
