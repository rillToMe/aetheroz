#include "display.h"
#include "heap.h"

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

void viewport_render(Viewport* vp, DisplayBuffer* target) {
    if (!vp || !target || !vp->source) return;

    const DisplayBuffer* src = vp->source;
    for (uint32_t row = 0; row < vp->bounds.height; row++) {
        int32_t sy = vp->scroll_y + (int32_t)row;
        int32_t ty = vp->bounds.y + (int32_t)row;
        if (sy < 0 || (uint32_t)sy >= src->height) continue;
        if (ty < 0 || (uint32_t)ty >= target->height) continue;

        for (uint32_t col = 0; col < vp->bounds.width; col++) {
            int32_t sx = vp->scroll_x + (int32_t)col;
            int32_t tx = vp->bounds.x + (int32_t)col;
            if (sx < 0 || (uint32_t)sx >= src->width) continue;
            if (tx < 0 || (uint32_t)tx >= target->width) continue;
            target->pixels[(uint32_t)ty * target->stride + (uint32_t)tx] =
                src->pixels[(uint32_t)sy * src->stride + (uint32_t)sx];
        }
    }
}
