#include "display.h"
#include "heap.h"
#include "string.h"

static uint8_t rect_clip_to_buffer(const DisplayBuffer* buffer, Rect* area) {
    int64_t x0 = area->x;
    int64_t y0 = area->y;
    int64_t x1 = x0 + (int64_t)area->width;
    int64_t y1 = y0 + (int64_t)area->height;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int64_t)buffer->width)  x1 = buffer->width;
    if (y1 > (int64_t)buffer->height) y1 = buffer->height;

    if (x1 <= x0 || y1 <= y0) return 0;

    area->x = (int32_t)x0;
    area->y = (int32_t)y0;
    area->width  = (uint32_t)(x1 - x0);
    area->height = (uint32_t)(y1 - y0);
    return 1;
}

DisplayBuffer* display_buffer_create(uint32_t width, uint32_t height, ColorFormat format) {
    if (width == 0 || height == 0) return NULL;

    DisplayBuffer* buffer = (DisplayBuffer*)kmalloc(sizeof(DisplayBuffer));
    if (!buffer) return NULL;

    buffer->pixels = (uint32_t*)kmalloc((size_t)width * height * sizeof(uint32_t));
    if (!buffer->pixels) {
        kfree(buffer);
        return NULL;
    }

    buffer->width = width;
    buffer->height = height;
    buffer->stride = width;
    buffer->format = format;
    buffer->owns_pixels = 1;
    buffer->dirty = NULL;
    return buffer;
}

DisplayBuffer* display_buffer_wrap(uint32_t* pixels, uint32_t width, uint32_t height,
                                   uint32_t stride, ColorFormat format) {
    if (!pixels || width == 0 || height == 0 || stride < width) return NULL;

    DisplayBuffer* buffer = (DisplayBuffer*)kmalloc(sizeof(DisplayBuffer));
    if (!buffer) return NULL;

    buffer->pixels = pixels;
    buffer->width = width;
    buffer->height = height;
    buffer->stride = stride;
    buffer->format = format;
    buffer->owns_pixels = 0;
    buffer->dirty = NULL;
    return buffer;
}

void display_buffer_destroy(DisplayBuffer* buffer) {
    if (!buffer) return;
    if (buffer->owns_pixels) kfree(buffer->pixels);
    kfree(buffer);
}

void display_buffer_write_pixel(DisplayBuffer* buffer, int32_t x, int32_t y, Color color) {
    if (!buffer) return;
    if (x < 0 || y < 0 || (uint32_t)x >= buffer->width || (uint32_t)y >= buffer->height) return;
    buffer->pixels[(uint32_t)y * buffer->stride + (uint32_t)x] = color;
    if (buffer->dirty) {
        Rect r = { x, y, 1, 1 };
        dirty_region_mark(buffer->dirty, r);
    }
}

void display_buffer_fill_rect(DisplayBuffer* buffer, Rect area, Color color) {
    if (!buffer) return;
    if (!rect_clip_to_buffer(buffer, &area)) return;

    for (uint32_t row = 0; row < area.height; row++) {
        uint32_t* line = buffer->pixels + ((uint32_t)area.y + row) * buffer->stride + (uint32_t)area.x;
        for (uint32_t col = 0; col < area.width; col++) {
            line[col] = color;
        }
    }
    if (buffer->dirty) dirty_region_mark(buffer->dirty, area);
}

int rect_intersect(Rect a, Rect b, Rect* out) {
    int64_t x0 = a.x > b.x ? a.x : b.x;
    int64_t y0 = a.y > b.y ? a.y : b.y;
    int64_t ax1 = (int64_t)a.x + a.width,  ay1 = (int64_t)a.y + a.height;
    int64_t bx1 = (int64_t)b.x + b.width,  by1 = (int64_t)b.y + b.height;
    int64_t x1 = ax1 < bx1 ? ax1 : bx1;
    int64_t y1 = ay1 < by1 ? ay1 : by1;

    if (x1 <= x0 || y1 <= y0) return 0;
    out->x = (int32_t)x0;
    out->y = (int32_t)y0;
    out->width  = (uint32_t)(x1 - x0);
    out->height = (uint32_t)(y1 - y0);
    return 1;
}

Rect rect_union(Rect a, Rect b) {
    int64_t x0 = a.x < b.x ? a.x : b.x;
    int64_t y0 = a.y < b.y ? a.y : b.y;
    int64_t ax1 = (int64_t)a.x + a.width,  ay1 = (int64_t)a.y + a.height;
    int64_t bx1 = (int64_t)b.x + b.width,  by1 = (int64_t)b.y + b.height;
    int64_t x1 = ax1 > bx1 ? ax1 : bx1;
    int64_t y1 = ay1 > by1 ? ay1 : by1;

    Rect out;
    out.x = (int32_t)x0;
    out.y = (int32_t)y0;
    out.width  = (uint32_t)(x1 - x0);
    out.height = (uint32_t)(y1 - y0);
    return out;
}

void dirty_region_clear(DirtyRegionList* list) {
    if (!list) return;
    list->count = 0;
    list->collapsed = 0;
}

void dirty_region_mark(DirtyRegionList* list, Rect r) {
    if (!list || r.width == 0 || r.height == 0) return;

    if (list->collapsed) {
        list->regions[0] = rect_union(list->regions[0], r);
        return;
    }

    if (list->count >= MAX_DIRTY_REGIONS) {
        Rect box = list->regions[0];
        for (uint32_t i = 1; i < list->count; i++) box = rect_union(box, list->regions[i]);
        box = rect_union(box, r);
        list->regions[0] = box;
        list->count = 1;
        list->collapsed = 1;
        return;
    }

    list->regions[list->count++] = r;
}

void viewport_scroll(Viewport* vp, int32_t dx, int32_t dy) {
    if (!vp) return;
    vp->scroll_x += dx;
    vp->scroll_y += dy;
    if (vp->scroll_x < 0) vp->scroll_x = 0;
    if (vp->scroll_y < 0) vp->scroll_y = 0;
}

// Jalur panas compositor (blit base→back & back→fb tiap frame): clipping
// dihitung SEKALI per panggilan, inner loop = memcpy per baris — bukan
// bounds check per pixel.
void viewport_render(Viewport* vp, DisplayBuffer* target) {
    if (!vp || !target || !vp->source) return;
    const DisplayBuffer* src = vp->source;

    // Rentang kolom [c0, c1): source & target sama-sama in-bounds.
    int64_t c0 = 0, c1 = (int64_t)vp->bounds.width;
    if (-(int64_t)vp->scroll_x > c0)                    c0 = -(int64_t)vp->scroll_x;
    if ((int64_t)src->width - vp->scroll_x < c1)        c1 = (int64_t)src->width - vp->scroll_x;
    if (-(int64_t)vp->bounds.x > c0)                    c0 = -(int64_t)vp->bounds.x;
    if ((int64_t)target->width - vp->bounds.x < c1)     c1 = (int64_t)target->width - vp->bounds.x;
    if (c1 <= c0) return;

    // Rentang baris [r0, r1): idem.
    int64_t r0 = 0, r1 = (int64_t)vp->bounds.height;
    if (-(int64_t)vp->scroll_y > r0)                    r0 = -(int64_t)vp->scroll_y;
    if ((int64_t)src->height - vp->scroll_y < r1)       r1 = (int64_t)src->height - vp->scroll_y;
    if (-(int64_t)vp->bounds.y > r0)                    r0 = -(int64_t)vp->bounds.y;
    if ((int64_t)target->height - vp->bounds.y < r1)    r1 = (int64_t)target->height - vp->bounds.y;
    if (r1 <= r0) return;

    uint64_t row_bytes = (uint64_t)(c1 - c0) * 4;
    for (int64_t row = r0; row < r1; row++) {
        const uint32_t* s = src->pixels +
            (uint64_t)(vp->scroll_y + row) * src->stride + (uint64_t)(vp->scroll_x + c0);
        uint32_t* d = target->pixels +
            (uint64_t)(vp->bounds.y + row) * target->stride + (uint64_t)(vp->bounds.x + c0);
        memcpy(d, s, row_bytes);
    }
}
