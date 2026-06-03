#include "io.h"
#include <stdint.h>

// Ukuran memori penyangga keyboard
#define KBD_BUFFER_SIZE 256
volatile uint8_t kbd_buffer[KBD_BUFFER_SIZE];
volatile uint32_t kbd_head = 0; // Penunjuk lokasi tulis
volatile uint32_t kbd_tail = 0; // Penunjuk lokasi baca

// Peta Scancode ke ASCII (Layout US QWERTY)
const unsigned char kbdus[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
  '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-',
    0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void pic_remap() {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    
    // 0xFC = 11111100 dalam biner. 
    // Bit 0 (Timer) = 0 (Aktif)
    // Bit 1 (Keyboard) = 0 (Aktif)
    // Sisanya 1 (Diblokir)
    outb(0x21, 0xFC);  
    outb(0xA1, 0xFF); 
}
// Interupsi: Menyimpan ketikan ke dalam Buffer
void keyboard_handler() {
    uint8_t status = inb(0x64);
    if (status & 0x01) {
        uint8_t scancode = inb(0x60);
        
        if (!(scancode & 0x80)) { // Jika tombol ditekan (bukan dilepas)
            uint8_t ascii = kbdus[scancode];
            if (ascii != 0) {
                // Masukkan huruf ke Ring Buffer
                uint32_t next_head = (kbd_head + 1) % KBD_BUFFER_SIZE;
                if (next_head != kbd_tail) { 
                    kbd_buffer[kbd_head] = ascii;
                    kbd_head = next_head;
                }
            }
        }
    }
    outb(0x20, 0x20); // Beritahu CPU interupsi selesai
}

// Fungsi abstrak yang akan dipanggil oleh VFS untuk membaca buffer
uint32_t keyboard_read(uint8_t *buffer, uint32_t size) {
    uint32_t bytes_read = 0;
    // Baca selama ada permintaan dan buffer tidak kosong
    while (bytes_read < size && kbd_head != kbd_tail) {
        buffer[bytes_read] = kbd_buffer[kbd_tail];
        kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
        bytes_read++;
    }
    return bytes_read;
}