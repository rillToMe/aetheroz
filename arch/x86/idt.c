#include <stdint.h>
#include "string.h"

extern void keyboard_isr_stub();
extern void timer_isr_stub();
extern void lapic_timer_isr_stub();
extern void reschedule_isr_stub();
extern void isr14_stub();
extern void isr128_stub();
extern void mouse_isr_stub();

// Exception handlers (dari exceptions.asm)
extern void isr0_stub();    // Divide by Zero
extern void isr1_stub();    // Debug
extern void isr2_stub();    // NMI
extern void isr3_stub();    // Breakpoint
extern void isr4_stub();    // Overflow
extern void isr5_stub();    // Bound Range Exceeded
extern void isr6_stub();    // Invalid Opcode
extern void isr7_stub();    // Device Not Available
extern void isr8_stub();    // Double Fault
extern void isr10_stub();   // Invalid TSS
extern void isr11_stub();   // Segment Not Present
extern void isr12_stub();   // Stack-Segment Fault
extern void isr13_stub();   // General Protection Fault

// ============================================================
// IDT Entry 64-bit (16 byte per entry — standar x86-64)
// Layout sesuai Intel Manual Vol.3 Table 6-2
// ============================================================
struct idt_entry {
    uint16_t base_lo;    // Bits 15:0 dari handler address
    uint16_t sel;        // Code segment selector
    uint8_t  ist;        // Interrupt Stack Table offset (0 = tidak pakai IST)
    uint8_t  flags;      // Type & Attributes (0x8E = interrupt gate, 0xEE = trap DPL3)
    uint16_t base_mid;   // Bits 31:16 dari handler address
    uint32_t base_hi;    // Bits 63:32 dari handler address
    uint32_t reserved;   // Harus 0
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;       // Pointer 64-bit ke tabel IDT
} __attribute__((packed));

struct idt_entry idt[256];
struct idt_ptr idtp;

extern void idt_flush(uint64_t ptr);

void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_lo  = (uint16_t)(base & 0xFFFF);
    idt[num].base_mid = (uint16_t)((base >> 16) & 0xFFFF);
    idt[num].base_hi  = (uint32_t)((base >> 32) & 0xFFFFFFFF); // FIX: bukan >> 64 (UB!)
    idt[num].sel      = sel;
    idt[num].ist      = 0;
    idt[num].flags    = flags;
    idt[num].reserved = 0;
}

void init_idt() {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base  = (uint64_t)&idt;

    memset(&idt, 0, sizeof(struct idt_entry) * 256);

    // --- EXCEPTION HANDLERS (Ring 0, interrupt gate, DPL=0) ---
    idt_set_gate(0,  (uint64_t)isr0_stub,  0x08, 0x8E); // #DE Divide by Zero
    idt_set_gate(1,  (uint64_t)isr1_stub,  0x08, 0x8E); // #DB Debug
    idt_set_gate(2,  (uint64_t)isr2_stub,  0x08, 0x8E); // #NMI
    idt_set_gate(3,  (uint64_t)isr3_stub,  0x08, 0x8E); // #BP Breakpoint
    idt_set_gate(4,  (uint64_t)isr4_stub,  0x08, 0x8E); // #OF Overflow
    idt_set_gate(5,  (uint64_t)isr5_stub,  0x08, 0x8E); // #BR Bound Range
    idt_set_gate(6,  (uint64_t)isr6_stub,  0x08, 0x8E); // #UD Invalid Opcode
    idt_set_gate(7,  (uint64_t)isr7_stub,  0x08, 0x8E); // #NM Device Not Available
    idt_set_gate(8,  (uint64_t)isr8_stub,  0x08, 0x8E); // #DF Double Fault
    idt_set_gate(10, (uint64_t)isr10_stub, 0x08, 0x8E); // #TS Invalid TSS
    idt_set_gate(11, (uint64_t)isr11_stub, 0x08, 0x8E); // #NP Segment Not Present
    idt_set_gate(12, (uint64_t)isr12_stub, 0x08, 0x8E); // #SS Stack-Segment Fault
    idt_set_gate(13, (uint64_t)isr13_stub, 0x08, 0x8E); // #GP General Protection
    idt_set_gate(14, (uint64_t)isr14_stub, 0x08, 0x8E); // #PF Page Fault

    // --- HARDWARE IRQ (setelah PIC remap: Master → 0x20..0x27, Slave → 0x28..0x2F) ---
    idt_set_gate(32, (uint64_t)timer_isr_stub,    0x08, 0x8E); // IRQ0  → INT 0x20 = 32 (Timer)
    idt_set_gate(33, (uint64_t)keyboard_isr_stub, 0x08, 0x8E); // IRQ1  → INT 0x21 = 33 (Keyboard)
    idt_set_gate(44, (uint64_t)mouse_isr_stub,    0x08, 0x8E); // IRQ12 → INT 0x2C = 44 (Mouse)
    idt_set_gate(240, (uint64_t)lapic_timer_isr_stub, 0x08, 0x8E); // LAPIC timer / AP scheduler tick
    idt_set_gate(253, (uint64_t)reschedule_isr_stub, 0x08, 0x8E); // IPI reschedule

    // --- SYSCALL (DPL=3 agar Ring 3 bisa panggil via int 0x80) ---
    idt_set_gate(128, (uint64_t)isr128_stub, 0x08, 0xEE); // int 0x80

    idt_flush((uint64_t)&idtp);
}

void idt_load(void) {
    idt_flush((uint64_t)&idtp);
}
