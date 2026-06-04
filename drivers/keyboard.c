#include "io.h"
#include <stdint.h>

extern void push_event(uint32_t type, int32_t p1, int32_t p2, int32_t p3);

#define KBD_BUFFER_SIZE 256
volatile uint8_t kbd_buffer[KBD_BUFFER_SIZE];
volatile uint32_t kbd_head = 0;
volatile uint32_t kbd_tail = 0;

// Variabel pelacak status tombol modifier
static uint8_t shift_pressed = 0;

// Tabel Scancode Normal (Tanpa Shift)
const unsigned char kbdus[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
  '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-',
    0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Tabel Scancode Kapital & Simbol (Dengan Shift)
const unsigned char kbdus_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
  '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
  '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-',
    0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void pic_remap() {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xF8); outb(0xA1, 0xEF);
}

void keyboard_handler() {
    uint8_t status = inb(0x64);
    
    // Tambahkan pelindung: JANGAN BACA jika Bit 5 (Mouse) menyala!
    if ((status & 0x01) && !(status & 0x20)) {
        uint8_t scancode = inb(0x60);
        
        // 1. Cek apakah tombol yang ditekan adalah SHIFT Kiri (0x2A) atau SHIFT Kanan (0x36)
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = 1;
        } 
        // 2. Cek apakah tombol SHIFT Kiri/Kanan dilepas (Scancode + 0x80)
        else if (scancode == 0xAA || scancode == 0xB6) {
            shift_pressed = 0;
        } 
        // 3. Jika tombol biasa ditekan (bukan dilepas)
        else if (!(scancode & 0x80)) { 
            uint8_t ascii = 0;
            
            // Gunakan tabel yang sesuai dengan status Shift
            if (shift_pressed) {
                ascii = kbdus_shift[scancode];
            } else {
                ascii = kbdus[scancode];
            }

            if (ascii != 0) {
                uint32_t next_head = (kbd_head + 1) % KBD_BUFFER_SIZE;
                if (next_head != kbd_tail) { 
                    kbd_buffer[kbd_head] = ascii;
                    kbd_head = next_head;

                    push_event(1, ascii, 0, 0); // EVENT_KEY_PRESS (1) -> P1: Kode ASCII
                }
            }
        }
    }
    outb(0x20, 0x20); // End of Interrupt
}

uint32_t keyboard_read(uint8_t *buffer, uint32_t size) {
    uint32_t bytes_read = 0;
    while (bytes_read < size && kbd_head != kbd_tail) {
        buffer[bytes_read] = kbd_buffer[kbd_tail];
        kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
        bytes_read++;
    }
    return bytes_read;
}

extern void keyboard_isr_stub(); // <-- Ubah namanya menjadi _stub!
extern void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags); 

void init_keyboard() {
    // 1. Reset status buffer
    kbd_head = 0;
    kbd_tail = 0;
    shift_pressed = 0;
    
    // 2. Daftarkan Keyboard_ISR ke IDT dengan nama stub yang benar
    idt_set_gate(33, (uint32_t)keyboard_isr_stub, 0x08, 0x8E);
}