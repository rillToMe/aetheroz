#include "tty.h"
#include <stddef.h>
#include <stdint.h>

// 1. Impor variabel dan fungsi GUI dari kernel.c
extern uint32_t* fb_ptr;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;
extern void draw_rect(uint32_t start_x, uint32_t start_y, uint32_t width, uint32_t height, uint32_t color);
extern void draw_char(char c, uint32_t x, uint32_t y, uint32_t color);

// --- TAMBAHAN: Impor fungsi kendali Mouse ---
extern void draw_mouse();
extern void erase_mouse();

// 2. Konstanta Terminal GUI
#define FONT_WIDTH 8
#define FONT_HEIGHT 16 // Tinggi diset 16 agar ada jarak kosong 8px di bawah setiap huruf
#define BG_COLOR 0x1E1E1E 
#define FG_COLOR 0xFFFFFF

size_t terminal_row;
size_t terminal_column;
fs_node_t tty_node; 

// --- ANIMASI KURSOR ---
int cursor_state = 1; // 1 = Menyala, 0 = Mati

// --- TAMBAHKAN FUNGSI INI KEMBALI ---
void tty_draw_cursor() {
    cursor_state = 1; // Paksa status menyala
    draw_rect(terminal_column * FONT_WIDTH, (terminal_row * FONT_HEIGHT) + 14, FONT_WIDTH, 2, FG_COLOR);
}
// -----------------------------------

void tty_blink_cursor() {
    erase_mouse(); // Sembunyikan mouse agar tidak tertimpa kedipan kursor
    
    cursor_state = !cursor_state; 
    if (cursor_state) {
        draw_rect(terminal_column * FONT_WIDTH, (terminal_row * FONT_HEIGHT) + 14, FONT_WIDTH, 2, FG_COLOR);
    } else {
        draw_rect(terminal_column * FONT_WIDTH, (terminal_row * FONT_HEIGHT) + 14, FONT_WIDTH, 2, BG_COLOR);
    }
    
    draw_mouse(); // Tampilkan mouse kembali
}

void tty_erase_cursor() {
    draw_rect(terminal_column * FONT_WIDTH, (terminal_row * FONT_HEIGHT) + 14, FONT_WIDTH, 2, BG_COLOR);
}
// ----------------------

// --- FITUR SCROLLING LAYAR GUI ---
void tty_scroll() {
    erase_mouse(); // Sembunyikan mouse sebelum layar digulir ke atas
    
    uint32_t copy_height = fb_height - FONT_HEIGHT;
    for (uint32_t y = 0; y < copy_height; y++) {
        for (uint32_t x = 0; x < fb_width; x++) {
            fb_ptr[(y * (fb_pitch / 4)) + x] = fb_ptr[((y + FONT_HEIGHT) * (fb_pitch / 4)) + x];
        }
    }
    draw_rect(0, fb_height - FONT_HEIGHT, fb_width, FONT_HEIGHT, BG_COLOR);
    terminal_row--; 
    
    draw_mouse(); // Tampilkan mouse kembali setelah scroll selesai
}

void terminal_putchar(char c) {
    erase_mouse();      // 1. Sembunyikan mouse sementara
    tty_erase_cursor(); // 2. Hapus kursor teks lama

    if (c == '\n') {
        terminal_column = 0;
        if (++terminal_row >= (fb_height / FONT_HEIGHT)) tty_scroll();
    }
    else if (c == '\b') {
        if (terminal_column > 0) {
            terminal_column--;
        } else if (terminal_row > 0) {
            terminal_row--;
            terminal_column = (fb_width / FONT_WIDTH) - 1;
        }
        draw_rect(terminal_column * FONT_WIDTH, terminal_row * FONT_HEIGHT, FONT_WIDTH, FONT_HEIGHT, BG_COLOR);
    }
    else {
        draw_rect(terminal_column * FONT_WIDTH, terminal_row * FONT_HEIGHT, FONT_WIDTH, FONT_HEIGHT, BG_COLOR);
        draw_char(c, terminal_column * FONT_WIDTH, terminal_row * FONT_HEIGHT, FG_COLOR);
        
        if (++terminal_column >= (fb_width / FONT_WIDTH)) {
            terminal_column = 0;
            if (++terminal_row >= (fb_height / FONT_HEIGHT)) tty_scroll();
        }
    }

    tty_draw_cursor(); // 3. Gambar ulang kursor teks di posisi baru
    draw_mouse();      // 4. Gambar ulang mouse di lapisan paling atas!
}

void tty_clear(void) {
    erase_mouse(); // Hapus mouse yang menyimpan background lama
    
    draw_rect(0, 0, fb_width, fb_height, BG_COLOR);
    terminal_row = 0;
    terminal_column = 0;
    
    tty_draw_cursor();
    draw_mouse();  // Langsung cetak mouse di atas layar yang sudah bersih
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
    tty_clear();
    tty_node.name[0] = 't'; tty_node.name[1] = 't'; tty_node.name[2] = 'y'; tty_node.name[3] = '0'; tty_node.name[4] = '\0';
    tty_node.flags = FS_CHARDEVICE;
    tty_node.write = tty_write; 
    tty_node.read = tty_read; 
    return &tty_node;
}