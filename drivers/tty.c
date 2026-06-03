#include "tty.h"
#include "io.h" // Wajib di-include agar bisa memakai outb() untuk kursor
#include <stddef.h>

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
volatile uint16_t* terminal_buffer;
fs_node_t tty_node; 

static inline uint8_t vga_entry_color(uint8_t fg, uint8_t bg) { return fg | bg << 4; }
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) { return (uint16_t) uc | (uint16_t) color << 8; }

// --- FITUR KURSOR HARDWARE ---
void tty_enable_cursor(uint8_t cursor_start, uint8_t cursor_end) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
}

void tty_update_cursor(int x, int y) {
    uint16_t pos = y * VGA_WIDTH + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

// --- FITUR SCROLLING LAYAR ---
void tty_scroll() {
    // Geser seluruh baris memori VGA naik 1 baris
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            terminal_buffer[(y - 1) * VGA_WIDTH + x] = terminal_buffer[y * VGA_WIDTH + x];
        }
    }
    // Kosongkan baris paling bawah dengan spasi
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    }
    // Tahan kursor agar tetap di baris terakhir
    terminal_row = VGA_HEIGHT - 1;
}

// --- LOGIKA CETAK HURUF (UPDATE) ---
void terminal_putchar(char c) {
    // 1. Tangani Enter (Newline)
    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            tty_scroll(); // Panggil scroll kalau mentok bawah!
        }
        tty_update_cursor(terminal_column, terminal_row);
        return;
    }

    // 2. Tangani Backspace
    if (c == '\b') {
        if (terminal_column > 0) {
            terminal_column--;
        } else if (terminal_row > 0) {
            terminal_row--;
            terminal_column = VGA_WIDTH - 1;
        }
        const size_t index = terminal_row * VGA_WIDTH + terminal_column;
        terminal_buffer[index] = vga_entry(' ', terminal_color);
        tty_update_cursor(terminal_column, terminal_row); // Perbarui posisi kursor
        return;
    }

    // 3. Tangani Huruf Biasa
    const size_t index = terminal_row * VGA_WIDTH + terminal_column;
    terminal_buffer[index] = vga_entry(c, terminal_color);
    
    // Majukan kursor ke kanan
    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            tty_scroll(); // Panggil scroll kalau mentok kanan bawah!
        }
    }
    tty_update_cursor(terminal_column, terminal_row); // Kursor selalu ikuti huruf terakhir
}

// Panggil fungsi pembersih layar
void tty_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
    terminal_row = 0;
    terminal_column = 0;
    tty_update_cursor(0, 0); // Reset kursor ke pojok kiri atas
}

uint32_t tty_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset; 
    for (uint32_t i = 0; i < size; i++) {
        terminal_putchar(buffer[i]);
    }
    return size; 
}

extern uint32_t keyboard_read(uint8_t *buffer, uint32_t size);
uint32_t tty_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset; 
    return keyboard_read(buffer, size);
}

fs_node_t* init_tty(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(7, 0); 
    terminal_buffer = (volatile uint16_t*) 0xB8000;

    tty_clear();

    // Aktifkan kursor bergaris bawah (Scanline 14 sampai 15). 
    // Kalau mau bentuk kotak penuh (Block Cursor), ganti jadi (0, 15).
    tty_enable_cursor(14, 15);

    tty_node.name[0] = 't'; tty_node.name[1] = 't'; tty_node.name[2] = 'y'; tty_node.name[3] = '0'; tty_node.name[4] = '\0';
    tty_node.flags = FS_CHARDEVICE;
    tty_node.write = tty_write; 
    tty_node.read = tty_read; 

    return &tty_node;
}