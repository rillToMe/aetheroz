#ifndef GFX_H
#define GFX_H

#include <stdint.h>

// --- Framebuffer global (didefinisikan di kernel/gfx/fb.c) ---
extern uint32_t* fb_ptr;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;

// Buffer resolusi maksimal 1920x1080 — cukup untuk semua konfigurasi QEMU/HW
extern uint32_t backbuffer[1920 * 1080];
extern uint32_t base_canvas[1920 * 1080];

// --- Primitif gambar (kernel/gfx/fb.c) ---
void draw_pixel(uint32_t x, uint32_t y, uint32_t color);
void draw_rect(uint32_t start_x, uint32_t start_y, uint32_t width, uint32_t height, uint32_t color);
void draw_image(int start_x, int start_y, int width, int height, uint32_t* buffer);
void draw_char(char c, uint32_t x, uint32_t y, uint32_t color);
void draw_string(const char* str, uint32_t x, uint32_t y, uint32_t color);

// --- Compositor dirty-region (kernel/gfx/compositor.c) ---
void screen_mark_dirty(int32_t x, int32_t y, uint32_t width, uint32_t height);
void compositor_flush(void);

#endif
