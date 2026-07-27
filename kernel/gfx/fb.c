#include <stdint.h>
#include <stddef.h>
#define FONT8x16_IMPLEMENTATION
#include "font8x16.h"
#include "gfx.h"
#include "display.h"

// --- VARIABEL GLOBAL FRAMEBUFFER ---
uint32_t* fb_ptr = NULL;
uint32_t fb_width = 0;
uint32_t fb_height = 0;
uint32_t fb_pitch = 0;

// Buffer resolusi maksimal 1920x1080 — cukup untuk semua konfigurasi QEMU/HW
// Jika base_canvas terlalu kecil dari fb_width*fb_height, pixel wrap dan muncul dua kali
uint32_t backbuffer[1920 * 1080];
uint32_t base_canvas[1920 * 1080];

// ============================================================
// Phase 3A ADOPSI: base_canvas / backbuffer / framebuffer dibungkus sebagai
// DisplayBuffer STATIS (borrow, tanpa kmalloc — heap belum hidup saat fb
// ditangkap kernel_main). Semua primitive di bawah menggambar lewat
// g_screen_db; compositor blit lewat accessor gfx_*_buffer().
//
// g_screen_db.dirty sengaja NULL: marking layar lewat screen_mark_dirty
// (jalur ber-lock compositor), bukan field dirty per-buffer.
// ============================================================
static DisplayBuffer g_screen_db;   // base_canvas
static DisplayBuffer g_back_db;     // backbuffer
static DisplayBuffer g_fb_db;       // framebuffer hardware

static void wrap_static(DisplayBuffer* db, uint32_t* pixels) {
    db->pixels      = pixels;
    db->width       = fb_width;
    db->height      = fb_height;
    db->stride      = fb_pitch / 4;
    db->format      = COLOR_FORMAT_XRGB8888;
    db->owns_pixels = 0;
    db->dirty       = NULL;
}

static inline int gfx_buffers_ready(void) {
    if (g_screen_db.pixels) return 1;
    if (fb_width == 0 || fb_pitch == 0) return 0;
    wrap_static(&g_screen_db, base_canvas);
    wrap_static(&g_back_db, backbuffer);
    wrap_static(&g_fb_db, fb_ptr);
    return 1;
}

DisplayBuffer* gfx_screen_buffer(void) { return gfx_buffers_ready() ? &g_screen_db : NULL; }
DisplayBuffer* gfx_back_buffer(void)   { return gfx_buffers_ready() ? &g_back_db   : NULL; }
DisplayBuffer* gfx_fb_buffer(void)     { return gfx_buffers_ready() ? &g_fb_db     : NULL; }

// Tulis satu pixel TANPA dirty-mark — dipakai loop internal yang menandai
// satu rect utuh di akhir (mark per-pixel = badai spinlock).
static inline void screen_put(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= g_screen_db.width || y >= g_screen_db.height) return;
    g_screen_db.pixels[y * g_screen_db.stride + x] = color;
}

// Kontrak Phase 3B: SEMUA primitive di bawah menandai dirty sendiri —
// caller tidak perlu (dan tidak boleh perlu) memanggil screen_mark_dirty.

void draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!gfx_buffers_ready()) return;
    screen_put(x, y, color);
    screen_mark_dirty((int32_t)x, (int32_t)y, 1, 1);
}

void draw_rect(uint32_t start_x, uint32_t start_y, uint32_t width, uint32_t height, uint32_t color) {
    if (!gfx_buffers_ready()) return;
    Rect r = { (int32_t)start_x, (int32_t)start_y, width, height };
    display_buffer_fill_rect(&g_screen_db, r, color);
    screen_mark_dirty((int32_t)start_x, (int32_t)start_y, width, height);
}

void draw_image(int start_x, int start_y, int width, int height, uint32_t* buffer) {
    if (!gfx_buffers_ready()) return;
    int i = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint32_t pixel = buffer[i++];
            uint8_t alpha = (pixel >> 24) & 0xFF;
            if (alpha > 0) {
                screen_put((uint32_t)(start_x + x), (uint32_t)(start_y + y), pixel & 0xFFFFFF);
            }
        }
    }
    screen_mark_dirty(start_x, start_y, (uint32_t)width, (uint32_t)height);
}

void draw_char(char c, uint32_t x, uint32_t y, uint32_t color) {
    if (c < 0 || c > 127) return;
    if (!gfx_buffers_ready()) return;
    const unsigned char* bitmap = font8x16[(int)c];
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            if (bitmap[row] & (0x80 >> col)) screen_put(x + col, y + row, color);
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
