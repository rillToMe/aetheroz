#ifndef KERNEL_ARCH_PAGING_H
#define KERNEL_ARCH_PAGING_H

#include <stdint.h>

void paging_init(uint64_t hhdm_offset, uint64_t kernel_phys, uint64_t kernel_virt);

#endif
