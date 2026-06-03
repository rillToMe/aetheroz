#include "tty.h"
#include <stddef.h>

extern uint32_t keyboard_read(uint8_t *buffer, uint32_t size);

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
volatile uint16_t* terminal_buffer;


// Ini adalah "objek" file untuk layar kita
fs_node_t tty_node; 

static inline uint8_t vga_entry_color(uint8_t fg, uint8_t bg) { return fg | bg << 4; }
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) { return (uint16_t) uc | (uint16_t) color << 8; }

void terminal_putchar(char c) {
    // 1. Tangani Enter (Newline)
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) terminal_row = 0;
        return;
    }

    // 2. Tangani Backspace
    if (c == '\b') {
        // Cek agar tidak kebablasan menghapus sampai keluar layar kiri
        if (terminal_column > 0) {
            terminal_column--;
        } else if (terminal_row > 0) {
            // Kalau mentok di kiri, naik ke ujung kanan baris atasnya
            terminal_row--;
            terminal_column = VGA_WIDTH - 1;
        }
        
        // Timpa posisi kursor saat ini dengan spasi kosong (' ')
        const size_t index = terminal_row * VGA_WIDTH + terminal_column;
        terminal_buffer[index] = vga_entry(' ', terminal_color);
        return; // Selesai, jangan majukan kursor
    }

    // 3. Tangani Karakter Normal
    const size_t index = terminal_row * VGA_WIDTH + terminal_column;
    terminal_buffer[index] = vga_entry(c, terminal_color);
    
    // Majukan kursor ke kanan
    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) terminal_row = 0;
    }
}

// Fungsi write yang mematuhi standar VFS
uint32_t tty_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node;   // Parameter ini diabaikan karena ini murni untuk layar
    (void)offset; // TTY (layar) tidak peduli offset, selalu lanjut di kursor terakhir
    
    // Cetak per byte sesuai ukuran (size) yang diminta
    for (uint32_t i = 0; i < size; i++) {
        terminal_putchar(buffer[i]);
    }
    return size; // Kembalikan jumlah byte yang berhasil ditulis
}

uint32_t tty_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset; // Diabaikan untuk character device
    return keyboard_read(buffer, size);
}

fs_node_t* init_tty(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(7, 0); 
    terminal_buffer = (volatile uint16_t*) 0xB8000;

    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }

    // Daftarkan layar sebagai Node VFS (Character Device)
    // Penulisan nama file /dev/tty0 secara manual array
    tty_node.name[0] = 't'; tty_node.name[1] = 't'; tty_node.name[2] = 'y'; tty_node.name[3] = '0'; tty_node.name[4] = '\0';
    tty_node.flags = FS_CHARDEVICE;
    
    // Hubungkan fungsi write kita ke dalam pointer fungsi node
    tty_node.write = tty_write; 
    tty_node.read = tty_read;

    return &tty_node;
}