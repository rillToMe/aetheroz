#include <stdint.h>
#include <kernel/arch/x86_64/gdt.h>

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

extern void gdt_load(struct gdt_ptr *ptr);

static uint64_t gdt_entries[] = {
    0x0000000000000000ULL,
    0x00AF9A000000FFFFULL,
    0x00AF92000000FFFFULL,
    0x00AFFA000000FFFFULL,
    0x00AFF2000000FFFFULL
};

void gdt_init(void) {
    struct gdt_ptr ptr;
    ptr.limit = sizeof(gdt_entries) - 1;
    ptr.base = (uint64_t)(uintptr_t)gdt_entries;
    gdt_load(&ptr);
}
