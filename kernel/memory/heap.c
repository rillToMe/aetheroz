#include <kernel/arch/paging.h>
#include <kernel/memory/heap.h>
#include <kernel/memory/pmm.h>

#define HEAP_INITIAL_PAGES 16
#define PAGE_SIZE 0x1000

static uint8_t *heap_base = 0;
static uint8_t *heap_current = 0;
static uint8_t *heap_end = 0;

void heap_init(void) {
    uint64_t block_phys = (uint64_t)(uintptr_t)pmm_alloc_pages(HEAP_INITIAL_PAGES);
    if (!block_phys) {
        for (;;) {
            __asm__ __volatile__("hlt");
        }
    }

    heap_base = (uint8_t *)(uintptr_t)hhdm_phys_to_virt(block_phys);
    heap_current = heap_base;
    heap_end = heap_base + (HEAP_INITIAL_PAGES * PAGE_SIZE);
}

void *kmalloc(uint64_t size) {
    if (size == 0) {
        return 0;
    }

    size = (size + 15) & ~15ULL;

    if (heap_current + size > heap_end) {
        return 0;
    }

    void *ptr = heap_current;
    heap_current += size;
    return ptr;
}
