#include <stdint.h>
#include "string.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

// Di 64-bit, TSS memakan 2 blok (16 byte), jadi strukturnya lebih panjang
struct tss_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base; // <-- Pointer sekarang 64-bit (8 byte)
} __attribute__((packed));

// Struktur TSS 64-bit
struct tss_entry_struct {
    uint32_t reserved0;
    uint64_t rsp0;       // Stack Kernel untuk interupsi
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1, ist2, ist3, ist4, ist5, ist6, ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

// Kapasitas GDT menjadi 7 (TSS memakan 2 indeks: 5 dan 6)
volatile struct gdt_entry gdt[7]; 
volatile struct gdt_ptr gp;
struct tss_entry_struct tss_entry;

extern void gdt_flush(uint64_t ptr);
extern void tss_flush();

void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;
    gdt[num].granularity |= (gran & 0xF0);
    gdt[num].access = access;
}

void write_tss(int32_t num, uint64_t rsp0) {
    uint64_t base = (uint64_t)&tss_entry;
    uint32_t limit = sizeof(tss_entry);

    struct tss_descriptor* tss_desc = (struct tss_descriptor*)&gdt[num];
    tss_desc->base_low = (base & 0xFFFF);
    tss_desc->base_middle = (base >> 16) & 0xFF;
    tss_desc->base_high = (base >> 24) & 0xFF;
    tss_desc->base_upper = (base >> 32) & 0xFFFFFFFF;
    tss_desc->limit_low = (limit & 0xFFFF);
    tss_desc->granularity = 0x00;
    tss_desc->access = 0x89; // TSS Present & Executable
    tss_desc->reserved = 0;

    memset((void*)&tss_entry, 0, sizeof(tss_entry));
    tss_entry.rsp0 = rsp0;
    tss_entry.iomap_base = sizeof(tss_entry);
}

void init_gdt() {
    gp.limit = (sizeof(struct gdt_entry) * 7) - 1;
    gp.base = (uint64_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);                // Null segment
    // GRANULARITY 0xAF: Bendera 'Long Mode' dinyalakan! (L=1, D=0)
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xAF); // Kernel Code
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Kernel Data
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xAF); // User Code 
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User Data

    write_tss(5, 0); // TSS masuk ke index 5 & 6

    gdt_flush((uint64_t)&gp);
    tss_flush();
}

void gdt_load(void) {
    gdt_flush((uint64_t)&gp);
}
