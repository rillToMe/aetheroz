// ============================================================
// GPU memory backing (graphics/memory/gpu_alloc.c)
// ============================================================

#include "gpu_alloc.h"
#include "pmm.h"
#include <string.h>

// hhdm_offset didefinisikan di kernel.c — base HHDM virtual.
extern uint64_t hhdm_offset;

uint32_t gpu_alloc_pages(uint32_t count, gpu_page_t* pages) {
    if (count == 0 || pages == NULL) return 0;
    uint32_t n = 0;
    for (; n < count; n++) {
        phys_addr_t pa = pmm_alloc_page();
        if (pa == PHYS_NULL) break;
        memset((void*)(pa + hhdm_offset), 0, 4096);
        pages[n].phys = pa;
        pages[n].virt = (void*)(pa + hhdm_offset);
    }
    return n;
}

int gpu_alloc_page(gpu_page_t* out) {
    if (out == NULL) return -1;
    phys_addr_t pa = pmm_alloc_page();
    if (pa == PHYS_NULL) return -1;
    memset((void*)(pa + hhdm_offset), 0, 4096);
    out->phys = pa;
    out->virt = (void*)(pa + hhdm_offset);
    return 0;
}

void gpu_free_pages(gpu_page_t* pages, uint32_t count) {
    if (pages == NULL) return;
    for (uint32_t i = 0; i < count; i++) {
        if (pages[i].phys != 0) pmm_free_page(pages[i].phys);
        pages[i].phys = 0;
        pages[i].virt = NULL;
    }
}
