#include <stdint.h>
#include "string.h" // Wajib untuk fungsi memset()

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

// Struktur TSS (Penunjuk jalan CPU saat pindah Ring 3 ke Ring 0)
struct tss_entry_struct {
    uint32_t prev_tss;
    uint32_t esp0;       // Stack Kernel
    uint32_t ss0;        // Segmen Data Kernel (0x10)
    uint32_t esp1, ss1, esp2, ss2, cr3;
    uint32_t eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap, iomap_base;
} __attribute__((packed));

// Ganti kapasitas dari 3 menjadi 6
volatile struct gdt_entry gdt[6]; 
volatile struct gdt_ptr gp;
volatile struct tss_entry_struct tss_entry;

extern void gdt_flush(volatile struct gdt_ptr *gdt_ptr_addr);
extern void tss_flush(void); // Fungsi baru di assembly

void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F);
    gdt[num].granularity |= (gran & 0xF0);
    gdt[num].access = access;
}

// Fungsi untuk membuat gerbang TSS
void write_tss(int32_t num, uint16_t ss0, uint32_t esp0) {
    uint32_t base = (uint32_t) &tss_entry;
    uint32_t limit = sizeof(tss_entry);

    // Daftarkan TSS ke GDT (Access 0xE9)
    gdt_set_gate(num, base, limit, 0xE9, 0x00); 

    memset((void*)&tss_entry, 0, sizeof(tss_entry));

    tss_entry.ss0  = ss0;
    tss_entry.esp0 = esp0; // Akan diupdate saat multitasking
    
    tss_entry.cs   = 0x0B;
    tss_entry.ss = tss_entry.ds = tss_entry.es = tss_entry.fs = tss_entry.gs = 0x13; 
    tss_entry.iomap_base = sizeof(tss_entry);
}

void init_gdt() {
    gp.limit = (sizeof(struct gdt_entry) * 6) - 1;
    gp.base = (uint32_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);                // 0x00: Null segment
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // 0x08: Kernel Code
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // 0x10: Kernel Data
    
    // --- GERBANG BARU UNTUK USER SPACE ---
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // 0x18: User Code (Ring 3)
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // 0x20: User Data (Ring 3)
    
    // --- GERBANG TSS ---
    write_tss(5, 0x10, 0x0); // 0x28: TSS Segment

    gdt_flush(&gp);
    tss_flush(); // Aktifkan TSS di prosesor!
}

// Fungsi ini akan dipanggil oleh sistem multitasking kita nanti
void set_kernel_stack(uint32_t stack) {
    tss_entry.esp0 = stack;
}