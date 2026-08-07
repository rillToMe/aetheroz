// ============================================================
// Software GPU Backend (graphics/backend/software.c)
//
// Backend fallback murni CPU: ghal_surface_t = buffer system RAM
// (XRGB8888). Semua operasi = memcpy/loop langsung ke pixels.
//
// ghal_surface_t adalah OPAQUE dari sisi HAL; definisi konkret ada
// di sini. Backend ini tidak pernah gagal init (murni RAM).
// ============================================================

#include "ghal.h"
#include "heap.h"
#include <string.h>
#include <stddef.h>   // NULL

// Struct konkret surface untuk backend ini (tidak diekspos ke HAL).
struct ghal_surface {
    uint32_t    width;
    uint32_t    height;
    ghal_format_t format;
    uint32_t*   pixels;      // width*height, tight-packed
};

static struct ghal_surface* sw_surface_create(uint32_t w, uint32_t h, ghal_format_t fmt) {
    if (w == 0 || h == 0) return NULL;
    // Cek overflow width*height*4 (pola audit 5.6).
    uint64_t bytes = (uint64_t)w * h * 4;
    if (w != 0 && (bytes / 4 / w) != h) return NULL;

    struct ghal_surface* s = (struct ghal_surface*)kmalloc(sizeof(*s));
    if (!s) return NULL;
    s->pixels = (uint32_t*)kmalloc((size_t)bytes);
    if (!s->pixels) { kfree(s); return NULL; }
    memset(s->pixels, 0, (size_t)bytes);
    s->width = w;
    s->height = h;
    s->format = fmt;
    return s;
}

static void sw_surface_destroy(ghal_surface_t* s) {
    if (!s) return;
    if (s->pixels) kfree(s->pixels);
    kfree(s);
}

static void sw_surface_upload(ghal_surface_t* s, const uint32_t* src,
                              uint32_t src_pitch, ghal_rect_t rect) {
    if (!s || !src) return;
    // Clamp ke surface.
    if (rect.x >= s->width || rect.y >= s->height) return;
    if (rect.w == 0 || rect.h == 0) return;
    uint32_t maxw = s->width - rect.x;
    uint32_t maxh = s->height - rect.y;
    if (rect.w > maxw) rect.w = maxw;
    if (rect.h > maxh) rect.h = maxh;

    const uint32_t* src_row = src + (uint64_t)rect.y * src_pitch + rect.x;
    uint32_t* dst_row = s->pixels + (uint64_t)rect.y * s->width + rect.x;
    for (uint32_t y = 0; y < rect.h; y++) {
        memcpy(dst_row, src_row, rect.w * 4);
        src_row += src_pitch;
        dst_row += s->width;
    }
}

static void sw_fill_rect(ghal_surface_t* dst, ghal_rect_t rect, uint32_t argb) {
    if (!dst) return;
    if (rect.x >= dst->width || rect.y >= dst->height) return;
    uint32_t maxw = dst->width - rect.x;
    uint32_t maxh = dst->height - rect.y;
    if (rect.w > maxw) rect.w = maxw;
    if (rect.h > maxh) rect.h = maxh;
    uint32_t color = argb & 0xFFFFFF;   // XRGB: top byte = alpha mask
    for (uint32_t y = 0; y < rect.h; y++) {
        uint32_t* row = dst->pixels + (uint64_t)(rect.y + y) * dst->width + rect.x;
        for (uint32_t x = 0; x < rect.w; x++) row[x] = color;
    }
}

static void sw_blit(ghal_surface_t* dst, ghal_rect_t dst_rect,
                    ghal_surface_t* src, ghal_rect_t src_rect) {
    if (!dst || !src) return;
    // v1: scaling tidak didukung (roadmap §8.6) — dst ukuran harus == src.
    if (dst_rect.w != src_rect.w || dst_rect.h != src_rect.h) return;

    // Clamp dst.
    if (dst_rect.x >= dst->width || dst_rect.y >= dst->height) return;
    uint32_t maxw = dst->width - dst_rect.x;
    uint32_t maxh = dst->height - dst_rect.y;
    if (dst_rect.w > maxw) dst_rect.w = maxw;
    if (dst_rect.h > maxh) dst_rect.h = maxh;

    // Clamp src rect terhadap bounds source.
    if (src_rect.x >= src->width || src_rect.y >= src->height) return;
    uint32_t smaxw = src->width - src_rect.x;
    uint32_t smaxh = src->height - src_rect.y;
    if (src_rect.w > smaxw) src_rect.w = smaxw;
    if (src_rect.h > smaxh) src_rect.h = smaxh;

    // Ambil ukuran terkecil (dst & src sudah di-clamp).
    uint32_t w = dst_rect.w < src_rect.w ? dst_rect.w : src_rect.w;
    uint32_t h = dst_rect.h < src_rect.h ? dst_rect.h : src_rect.h;

    const uint32_t* srow = src->pixels + (uint64_t)src_rect.y * src->width + src_rect.x;
    uint32_t* drow = dst->pixels + (uint64_t)dst_rect.y * dst->width + dst_rect.x;
    for (uint32_t y = 0; y < h; y++) {
        memcpy(drow, srow, w * 4);
        srow += src->width;
        drow += dst->width;
    }
}

static void sw_present(ghal_surface_t* s, const ghal_rect_t* rect) {
    // Software backend: tidak ada scanout device — surface sudah di RAM.
    // Present software ditangani compositor (menyalin ke fb_ptr). No-op.
    (void)s; (void)rect;
}

static int software_init(void) { return 0; }
static void software_shutdown(void) {}

const ghal_backend_ops_t software_backend_ops = {
    .name           = "software",
    .capabilities   = GHAL_CAP_PARTIAL_FLUSH,
    .init           = software_init,
    .shutdown       = software_shutdown,
    .surface_create = sw_surface_create,
    .surface_destroy= sw_surface_destroy,
    .surface_upload = sw_surface_upload,
    .fill_rect      = sw_fill_rect,
    .blit           = sw_blit,
    .present        = sw_present,
    .cursor_update  = NULL,
    .cursor_move    = NULL,
};
