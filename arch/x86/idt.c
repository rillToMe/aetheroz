#include <stdint.h>

extern void keyboard_isr_stub();
extern void timer_isr_stub();
extern void isr14_stub();

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

// Fungsi utama yang dipanggil oleh kernel
void init_idt() {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base  = (uint32_t)&idt;

    // Bersihkan seluruh 256 entry agar memori tidak berisi data sampah
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    idt_set_gate(14, (uint32_t)isr14_stub, 0x08, 0x8E);

    // Pintu 32 untuk Timer, Pintu 33 untuk Keyboard
    idt_set_gate(32, (uint32_t)timer_isr_stub, 0x08, 0x8E); 
    idt_set_gate(33, (uint32_t)keyboard_isr_stub, 0x08, 0x8E);
    
    // Load tabel IDT ke CPU
    idt_flush(&idtp);
}