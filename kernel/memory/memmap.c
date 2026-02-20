#include <kernel/arch/serial.h>
#include <kernel/lib/util.h>
#include <kernel/memory/memmap.h>

static struct usable_region usable_regions[MAX_USABLE_REGIONS];
static uint64_t usable_region_count = 0;
static uint64_t total_usable = 0;

void memmap_init(struct limine_memmap_response *memmap) {
    usable_region_count = 0;
    total_usable = 0;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        serial_write('[');
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            serial_write('U');
            uint64_t base = align_up(entry->base);
            uint64_t end = align_down(entry->base + entry->length);
            if (end > base) {
                uint64_t length = end - base;
                total_usable += length;
                if (usable_region_count < MAX_USABLE_REGIONS) {
                    usable_regions[usable_region_count].base = base;
                    usable_regions[usable_region_count].length = length;
                    usable_region_count++;
                }
            }
        } else {
            serial_write('X');
        }
        serial_write(']');
    }

    serial_write(' ');
    serial_write('T');
    serial_write('=');
    serial_write('0');
    serial_write('x');
    serial_write_hex64(total_usable);
}

uint64_t memmap_total_usable(void) {
    return total_usable;
}

struct usable_region *memmap_usable_regions(void) {
    return usable_regions;
}

uint64_t memmap_usable_region_count(void) {
    return usable_region_count;
}
