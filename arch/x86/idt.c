#include <stdint.h>

extern void keyboard_isr_stub();
extern void timer_isr_stub();
extern void isr14_stub();
extern void isr128_stub();
extern void mouse_isr_stub();

// Exception handlers baru (dari exceptions.asm)
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

// Struktur 1 entry IDT (Gerbang Interupsi)
struct idt_entry {
    uint16_t base_lo;   // 16 bit bawah dari alamat fungsi yang akan dipanggil
    uint16_t sel;       // Code Segment selector (kita set ke 0x08 sesuai GDT kita)
    uint8_t  always0;   // Selalu 0
    uint8_t  flags;     // Aturan hak akses (siapa yang boleh memanggil)
    uint16_t base_hi;   // 16 bit atas dari alamat fungsi
} __attribute__((packed));

// Pointer untuk instruksi lidt
struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

// Kita butuh 256 entry sesuai standar x86
volatile struct idt_entry idt[256];
volatile struct idt_ptr idtp;

extern void idt_flush(volatile struct idt_ptr *ptr);

// Fungsi untuk mengisi satu gerbang (entry) IDT
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_lo = base & 0xFFFF;
    idt[num].base_hi = (base >> 16) & 0xFFFF;
    idt[num].sel     = sel;
    idt[num].always0 = 0;
    idt[num].flags   = flags;
}

void init_idt() {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base  = (uint32_t)&idt;

    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    // --- EXCEPTION HANDLERS (Ring 0, DPL=0, 0x8E) ---
    idt_set_gate(0,  (uint32_t)isr0_stub,  0x08, 0x8E); // Divide by Zero
    idt_set_gate(1,  (uint32_t)isr1_stub,  0x08, 0x8E); // Debug
    idt_set_gate(2,  (uint32_t)isr2_stub,  0x08, 0x8E); // NMI
    idt_set_gate(3,  (uint32_t)isr3_stub,  0x08, 0x8E); // Breakpoint
    idt_set_gate(4,  (uint32_t)isr4_stub,  0x08, 0x8E); // Overflow
    idt_set_gate(5,  (uint32_t)isr5_stub,  0x08, 0x8E); // Bound Range
    idt_set_gate(6,  (uint32_t)isr6_stub,  0x08, 0x8E); // Invalid Opcode
    idt_set_gate(7,  (uint32_t)isr7_stub,  0x08, 0x8E); // Device Not Available
    idt_set_gate(8,  (uint32_t)isr8_stub,  0x08, 0x8E); // Double Fault
    idt_set_gate(10, (uint32_t)isr10_stub, 0x08, 0x8E); // Invalid TSS
    idt_set_gate(11, (uint32_t)isr11_stub, 0x08, 0x8E); // Segment Not Present
    idt_set_gate(12, (uint32_t)isr12_stub, 0x08, 0x8E); // Stack-Segment Fault
    idt_set_gate(13, (uint32_t)isr13_stub, 0x08, 0x8E); // General Protection Fault
    idt_set_gate(14, (uint32_t)isr14_stub, 0x08, 0x8E); // Page Fault

    // --- HARDWARE IRQ ---
    idt_set_gate(32, (uint32_t)timer_isr_stub,    0x08, 0x8E);
    idt_set_gate(33, (uint32_t)keyboard_isr_stub, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)mouse_isr_stub,    0x08, 0x8E);

    // --- SYSCALL (DPL=3 agar Ring 3 bisa panggil via int 0x80) ---
    idt_set_gate(128, (uint32_t)isr128_stub, 0x08, 0xEE);

    idt_flush(&idtp);
}