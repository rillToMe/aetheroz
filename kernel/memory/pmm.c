#include <kernel/arch/serial.h>
#include <kernel/lib/util.h>
#include <kernel/memory/pmm.h>

static uint8_t *pmm_bitmap = 0;
static uint64_t pmm_total_pages = 0;
static uint64_t pmm_bitmap_size = 0;
static uint64_t pmm_last_alloc = 0;
static uint64_t pmm_free_pages_count = 0;

static int pmm_is_used(uint64_t page_index) {
    return (pmm_bitmap[page_index / 8] & (uint8_t)(1U << (page_index % 8))) != 0;
}

static void pmm_mark_used(uint64_t page_index) {
    if (!pmm_is_used(page_index)) {
        pmm_bitmap[page_index / 8] |= (uint8_t)(1U << (page_index % 8));
        if (pmm_free_pages_count > 0) {
            pmm_free_pages_count--;
        }
    }
}

static void pmm_mark_free(uint64_t page_index) {
    if (pmm_is_used(page_index)) {
        pmm_bitmap[page_index / 8] &= (uint8_t)~(1U << (page_index % 8));
        pmm_free_pages_count++;
    }
}

static void pmm_mark_range_used(uint64_t base, uint64_t length) {
    uint64_t start = base / 0x1000;
    uint64_t pages = length / 0x1000;
    for (uint64_t i = 0; i < pages; i++) {
        pmm_mark_used(start + i);
    }
}

static void pmm_mark_range_free(uint64_t base, uint64_t length) {
    uint64_t start = base / 0x1000;
    uint64_t pages = length / 0x1000;
    for (uint64_t i = 0; i < pages; i++) {
        pmm_mark_free(start + i);
    }
}

void *pmm_alloc_pages(uint64_t pages) {
    if (!pmm_bitmap || pmm_total_pages == 0 || pages == 0 || pages > pmm_total_pages) {
        return 0;
    }
    uint64_t start = pmm_last_alloc;
    for (int pass = 0; pass < 2; pass++) {
        uint64_t begin = pass == 0 ? start : 0;
        uint64_t end = pass == 0 ? pmm_total_pages : start;
        uint64_t run = 0;
        uint64_t run_start = 0;
        for (uint64_t i = begin; i < end; i++) {
            if (!pmm_is_used(i)) {
                if (run == 0) {
                    run_start = i;
                }
                run++;
                if (run == pages) {
                    for (uint64_t j = 0; j < pages; j++) {
                        pmm_mark_used(run_start + j);
                    }
                    pmm_last_alloc = (run_start + pages) % pmm_total_pages;
                    return (void *)(uintptr_t)(run_start * 0x1000);
                }
            } else {
                run = 0;
            }
        }
    }
    return 0;
}

void pmm_init(uint64_t total_usable, struct usable_region *regions, uint64_t region_count) {
    pmm_total_pages = total_usable / 0x1000;
    pmm_bitmap_size = (pmm_total_pages + 7) / 8;
    uint64_t bitmap_bytes = align_up(pmm_bitmap_size);

    uint64_t bitmap_phys = 0;
    for (uint64_t i = 0; i < region_count; i++) {
        if (regions[i].length >= bitmap_bytes) {
            bitmap_phys = regions[i].base;
            regions[i].base += bitmap_bytes;
            regions[i].length -= bitmap_bytes;
            break;
        }
    }

    if (bitmap_phys == 0) {
        for (;;) {
            __asm__ __volatile__("hlt");
        }
    }

    pmm_bitmap = (uint8_t *)(uintptr_t)bitmap_phys;
    mem_set(pmm_bitmap, 0xFF, pmm_bitmap_size);
    pmm_free_pages_count = 0;

    for (uint64_t i = 0; i < region_count; i++) {
        if (regions[i].length == 0) {
            continue;
        }
        pmm_mark_range_free(regions[i].base, regions[i].length);
    }

    pmm_mark_range_used(bitmap_phys, bitmap_bytes);

    serial_write(' ');
    serial_write('P');
    serial_write('=');
    serial_write_hex64(pmm_total_pages);
    serial_write(' ');
    serial_write('B');
    serial_write('=');
    serial_write_hex64(pmm_bitmap_size);
    serial_write(' ');
    serial_write('F');
    serial_write('=');
    serial_write_hex64(pmm_free_pages_count);
    serial_write(' ');
    serial_write('U');
    serial_write('=');
    serial_write_hex64(pmm_total_pages - pmm_free_pages_count);

    void *probe = pmm_alloc_page();
    serial_write(' ');
    serial_write('A');
    serial_write('=');
    serial_write('0');
    serial_write('x');
    serial_write_hex64((uint64_t)(uintptr_t)probe);
}

void *pmm_alloc_page(void) {
    return pmm_alloc_pages(1);
}

void pmm_free_page(void *addr) {
    if (!addr) {
        return;
    }
    uint64_t page_index = (uint64_t)addr / 0x1000;
    if (page_index < pmm_total_pages) {
        pmm_mark_free(page_index);
        if (page_index < pmm_last_alloc) {
            pmm_last_alloc = page_index;
        }
    }
}

uint64_t pmm_free_pages(void) {
    return pmm_free_pages_count;
}
