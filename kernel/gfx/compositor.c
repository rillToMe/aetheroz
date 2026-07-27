#include <stdint.h>
#include "gfx.h"
#include "kwm_internal.h"
#include "display.h"
#include "spinlock.h"

extern int32_t mouse_x;
extern int32_t mouse_y;
extern const uint8_t cursor_bitmap[16][12];

#define CURSOR_WIDTH  12
#define CURSOR_HEIGHT 16

// Dirty-region state (Phase 3B). Every draw into base_canvas and every window
// move marks the touched rect here; compositor_flush recomposites and presents
// only these rects instead of the whole screen. Idle frames are near no-ops.
static DirtyRegionList g_screen_dirty;
static spinlock_t g_dirty_lock = SPINLOCK_INIT;
static int32_t g_last_cursor_x = -1;
static int32_t g_last_cursor_y = -1;

void screen_mark_dirty(int32_t x, int32_t y, uint32_t width, uint32_t height) {
    Rect r = { x, y, width, height };
    uint64_t flags = spinlock_lock_irqsave(&g_dirty_lock);
    dirty_region_mark(&g_screen_dirty, r);
    spinlock_unlock_irqrestore(&g_dirty_lock, flags);
}

// Copy a screen-space rect between two full-screen buffers.
// Phase 3C ADOPSI: blit = viewport_render dengan scroll = posisi rect —
// Viewport adalah mesin blit standar compositor (dipakai lagi oleh WM
// untuk surface di Phase 5).
static void blit_rect_db(DisplayBuffer* dst, DisplayBuffer* src, Rect r) {
    Viewport vp = { r, r.x, r.y, src };
    viewport_render(&vp, dst);
}

// Composite every window overlapping `r` (z-order low→high) onto the backbuffer.
// Caller must hold kwm_lock. Surface window = DisplayBuffer (Phase 3A/5);
// tetap loop custom (bukan viewport_render) karena butuh alpha-mask per pixel.
static void composite_windows_in_rect(Rect r, int pitch4) {
    for (uint32_t z = 1; z <= next_z_index; z++) {
        for (int w = 0; w < MAX_WINDOWS; w++) {
            if (!(kwm_windows[w].active && kwm_windows[w].z_index == z && kwm_windows[w].canvas))
                continue;

            Rect wrect = { kwm_windows[w].x, kwm_windows[w].y,
                           kwm_windows[w].width, kwm_windows[w].height };
            Rect clip;
            if (!rect_intersect(wrect, r, &clip)) continue;

            const int32_t win_x = kwm_windows[w].x;
            const int32_t win_y = kwm_windows[w].y;
            const DisplayBuffer* canvas = kwm_windows[w].canvas;

            for (uint32_t yy = 0; yy < clip.height; yy++) {
                int32_t sy = clip.y + (int32_t)yy - win_y;
                const uint32_t* src = canvas->pixels + (uint32_t)sy * canvas->stride + (uint32_t)(clip.x - win_x);
                uint32_t* dst = backbuffer + ((uint32_t)clip.y + yy) * (uint32_t)pitch4 + (uint32_t)clip.x;
                for (uint32_t xx = 0; xx < clip.width; xx++) {
                    uint32_t pixel = src[xx];
                    // Alpha byte acts as a per-pixel mask: 0 = transparent.
                    if (pixel >> 24) dst[xx] = pixel & 0xFFFFFF;
                }
            }
        }
    }
}

void compositor_flush() {
    if (fb_width == 0) return;
    DisplayBuffer* screen_db = gfx_screen_buffer();  // base_canvas
    DisplayBuffer* back_db   = gfx_back_buffer();
    DisplayBuffer* fb_db     = gfx_fb_buffer();
    if (!screen_db || !back_db || !fb_db) return;
    const int pitch4 = (int)(fb_pitch / 4);
    Rect screen = { 0, 0, fb_width, fb_height };

    uint64_t flags = spinlock_lock_irqsave(&g_dirty_lock);
    DirtyRegionList dirty = g_screen_dirty;
    dirty_region_clear(&g_screen_dirty);
    spinlock_unlock_irqrestore(&g_dirty_lock, flags);

    // Cursor moves every frame it's dragged; both the vacated and the new cell
    // must repaint, so fold them into the dirty set.
    int32_t cx = mouse_x, cy = mouse_y;
    if (g_last_cursor_x >= 0) {
        Rect old = { g_last_cursor_x, g_last_cursor_y, CURSOR_WIDTH, CURSOR_HEIGHT };
        dirty_region_mark(&dirty, old);
    }
    Rect cur = { cx, cy, CURSOR_WIDTH, CURSOR_HEIGHT };
    dirty_region_mark(&dirty, cur);

    if (dirty.count == 0) return;

    uint64_t kwm_flags = spinlock_lock_irqsave(&kwm_lock);
    for (uint32_t i = 0; i < dirty.count; i++) {
        Rect r;
        if (!rect_intersect(dirty.regions[i], screen, &r)) continue;
        blit_rect_db(back_db, screen_db, r);
        composite_windows_in_rect(r, pitch4);
    }
    spinlock_unlock_irqrestore(&kwm_lock, kwm_flags);

    for (int y = 0; y < CURSOR_HEIGHT; y++) {
        for (int x = 0; x < CURSOR_WIDTH; x++) {
            if (cy + y >= (int32_t)fb_height || cx + x >= (int32_t)fb_width) continue;
            uint32_t offset = ((cy + y) * pitch4) + (cx + x);
            if (cursor_bitmap[y][x] == 1) backbuffer[offset] = 0xFFFFFF;
            else if (cursor_bitmap[y][x] == 2) backbuffer[offset] = 0x000000;
        }
    }
    g_last_cursor_x = cx;
    g_last_cursor_y = cy;

    for (uint32_t i = 0; i < dirty.count; i++) {
        Rect r;
        if (!rect_intersect(dirty.regions[i], screen, &r)) continue;
        blit_rect_db(fb_db, back_db, r);
    }
}
