#include <stdint.h>
#include <stddef.h>
#define FONT8x16_IMPLEMENTATION
#include "font8x16.h"
#include "gfx.h"

// --- VARIABEL GLOBAL FRAMEBUFFER ---
uint32_t* fb_ptr = NULL;
uint32_t fb_width = 0;
uint32_t fb_height = 0;
uint32_t fb_pitch = 0;

// Buffer resolusi maksimal 1920x1080 — cukup untuk semua konfigurasi QEMU/HW
// Jika base_canvas terlalu kecil dari fb_width*fb_height, pixel wrap dan muncul dua kali
uint32_t backbuffer[1920 * 1080];
uint32_t base_canvas[1920 * 1080];

void draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= fb_width || y >= fb_height) return;
    base_canvas[(y * (fb_pitch / 4)) + x] = color;
}

void draw_rect(uint32_t start_x, uint32_t start_y, uint32_t width, uint32_t height, uint32_t color) {
    for (uint32_t y = start_y; y < start_y + height; y++) {
        for (uint32_t x = start_x; x < start_x + width; x++) {
            draw_pixel(x, y, color);
        }
    }
    screen_mark_dirty((int32_t)start_x, (int32_t)start_y, width, height);
}

void draw_image(int start_x, int start_y, int width, int height, uint32_t* buffer) {
    int i = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint32_t pixel = buffer[i++];
            uint8_t alpha = (pixel >> 24) & 0xFF;
            if (alpha > 0) {
                draw_pixel(start_x + x, start_y + y, pixel & 0xFFFFFF);
            }
        }
    }
    screen_mark_dirty(start_x, start_y, (uint32_t)width, (uint32_t)height);
}

void draw_char(char c, uint32_t x, uint32_t y, uint32_t color) {
    if (c < 0 || c > 127) return;
    const unsigned char* bitmap = font8x16[(int)c];
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            if (bitmap[row] & (0x80 >> col)) draw_pixel(x + col, y + row, color);
        }
    }
    screen_mark_dirty((int32_t)x, (int32_t)y, 8, 16);
}

void draw_string(const char* str, uint32_t x, uint32_t y, uint32_t color) {
    uint32_t curr_x = x;
    uint32_t curr_y = y;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') { curr_y += 16; curr_x = x; }
        else { draw_char(str[i], curr_x, curr_y, color); curr_x += 8; }
    }
}
