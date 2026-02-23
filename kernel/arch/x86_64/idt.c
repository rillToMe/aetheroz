#include <kernel/arch/x86_64/idt.h>

extern void isr80_stub(void);
extern void isr32_stub(void);
extern void isr13_stub(void);
extern void isr14_stub(void);

static struct idt_entry idt[256];
static struct idt_ptr idtr;

static void idt_set_gate(int vector, void *handler, uint8_t flags) {
    uint64_t addr = (uint64_t)handler;
    uint16_t cs;
    __asm__ __volatile__("mov %%cs, %0" : "=r"(cs));
    idt[vector].offset_low = addr & 0xFFFF;
    idt[vector].selector = cs;
    idt[vector].ist = 0;
    idt[vector].type_attr = flags;
    idt[vector].offset_mid = (addr >> 16) & 0xFFFF;
    idt[vector].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].zero = 0;
}

void idt_init(void) {
    idt_set_gate(0x20, isr32_stub, 0x8E);
    idt_set_gate(0x80, isr80_stub, 0x8E);
    idt_set_gate(0x0D, isr13_stub, 0x8E);
    idt_set_gate(0x0E, isr14_stub, 0x8E);
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint64_t)(uintptr_t)&idt;
    __asm__ __volatile__("lidt %0" : : "m"(idtr));
}
