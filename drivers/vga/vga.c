#include "vga.h"

static const uint16_t vga_width = 80;
static const uint16_t vga_height = 25;
static const uint8_t vga_default_fg = 7;
static const uint8_t vga_default_bg = 0;
static const unsigned long vga_buffer_address = 0xB8000;

static uint16_t vga_row = 0;
static uint16_t vga_column = 0;
static uint8_t vga_color = 0;
static volatile uint16_t* const vga_buffer = (volatile uint16_t*)vga_buffer_address;

static uint16_t vga_make_entry(char c, uint8_t color_attr) {
    return (uint16_t)c | (uint16_t)(color_attr << 8);
}

static void vga_clear_row(uint16_t y) {
    uint16_t x;
    for (x = 0; x < vga_width; ++x) {
        vga_buffer[y * vga_width + x] = vga_make_entry(' ', vga_color);
    }
}

static void vga_scroll(void) {
    uint16_t y;
    uint16_t x;
    for (y = 1; y < vga_height; ++y) {
        for (x = 0; x < vga_width; ++x) {
            vga_buffer[(y - 1) * vga_width + x] = vga_buffer[y * vga_width + x];
        }
    }
    vga_clear_row(vga_height - 1);
    vga_row = vga_height - 1;
    vga_column = 0;
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    vga_color = (uint8_t)((bg << 4) | (fg & 0x0F));
}

void vga_init(void) {
    vga_row = 0;
    vga_column = 0;
    vga_set_color(vga_default_fg, vga_default_bg);
}

void vga_clear(void) {
    uint16_t y;
    for (y = 0; y < vga_height; ++y) {
        vga_clear_row(y);
    }
    vga_row = 0;
    vga_column = 0;
}

void vga_putc(char c) {
    if (c == '\n') {
        vga_column = 0;
        if (vga_row + 1 >= vga_height) {
            vga_scroll();
        } else {
            vga_row++;
        }
        return;
    }

    vga_buffer[vga_row * vga_width + vga_column] = vga_make_entry(c, vga_color);
    vga_column++;

    if (vga_column >= vga_width) {
        vga_column = 0;
        if (vga_row + 1 >= vga_height) {
            vga_scroll();
        } else {
            vga_row++;
        }
    }
}

void vga_print(const char* str) {
    size_t i;
    if (!str) {
        return;
    }
    for (i = 0; str[i] != '\0'; ++i) {
        vga_putc(str[i]);
    }
}
