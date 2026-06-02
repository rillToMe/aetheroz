#include <stdint.h>
#include "io.h"

// Ambil fungsi print dari kernel.c
extern void terminal_putchar(char c);

// Peta Scancode ke huruf ASCII (US QWERTY Layout sederhana)
const unsigned char kbdus[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
  '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-',
    0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// Menggeser jalur interupsi hardware ke nomor 32 dan seterusnya
void pic_remap() {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28); // Master PIC di 32 (0x20), Slave di 40
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0x0);  outb(0xA1, 0x0); 
}

// Fungsi ini dipanggil setiap kali tombol keyboard ditekan/dilepas
void keyboard_handler() {
    uint8_t status = inb(0x64); // Cek status controller
    
    if (status & 0x01) {
        uint8_t scancode = inb(0x60); // Baca tombol apa yang ditekan (Port 0x60)
        
        // Cek jika ini adalah "Key Press" (bukan Key Release)
        if (!(scancode & 0x80)) {
            terminal_putchar(kbdus[scancode]); // Cetak ke layar
        }
    }
    
    // Beritahu PIC bahwa kita sudah selesai memproses interupsi (End of Interrupt)
    outb(0x20, 0x20);
}