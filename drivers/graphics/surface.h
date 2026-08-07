#ifndef GRAPHICS_SURFACE_H
#define GRAPHICS_SURFACE_H

// ============================================================
// Graphics HAL — surface representation (include/graphics/surface.h)
//
// A surface is the common unit passed through the HAL. It stores
// geometry, pixel format, memory location and a GPU handle. The
// software backend treats `pixels` as a plain RAM buffer; a VRAM
// backend would map `gpu_handle` to a device allocation and only
// touch pixels via upload/download.
//
// The struct is owned by the Surface Manager (surface_manager.c);
// backends read it, they do not allocate it.
// ============================================================

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Pixel formats (subset; extend as backends support more).
typedef enum {
    GPU_PIXEL_XRGB8888 = 0,   // 32bpp, top byte = opaque/alpha mask
    GPU_PIXEL_ARGB8888 = 1,   // 32bpp, 8-bit alpha in top byte
    GPU_PIXEL_RGB565   = 2,   // 16bpp
} gpu_pixel_format_t;

static inline int gpu_pixel_bpp(gpu_pixel_format_t f) {
    switch (f) {
        case GPU_PIXEL_XRGB8888:
        case GPU_PIXEL_ARGB8888: return 4;
        case GPU_PIXEL_RGB565:   return 2;
        default:                 return 4;
    }
}

// A 2D region.
typedef struct {
    int32_t x, y;
    uint32_t width, height;
} gpu_rect_t;

// Memory location of surface pixels.
typedef enum {
    GPU_MEM_NONE     = 0,   // no backing yet
    GPU_MEM_SYSTEM   = 1,   // system RAM (shared memory)
    GPU_MEM_VRAM     = 2,   // dedicated VRAM
} gpu_mem_t;

// Surface kind (semantics for the manager/compositor).
typedef enum {
    GPU_SURFACE_FRONT   = 0,
    GPU_SURFACE_BACK    = 1,
    GPU_SURFACE_WINDOW  = 2,
    GPU_SURFACE_OFFSCREEN = 3,
    GPU_SURFACE_TEXTURE = 4,
} gpu_surface_kind_t;

typedef struct gpu_surface {
    uint32_t          width;      // pixel width
    uint32_t          height;     // pixel height
    uint32_t          stride;     // pixels per row (>= width)
    gpu_pixel_format_t format;    // enum above
    gpu_mem_t         memory;     // GPU_MEM_SYSTEM / GPU_MEM_VRAM / NONE
    gpu_surface_kind_t kind;
    uint32_t          flags;      // backend/manager flags
    uint32_t          handle;     // manager-assigned handle (0 = unmanaged)
    uint32_t          refcount;   // surface-manager reference count
    uint32_t          vram_offset;// VRAM backend: offset into VRAM heap (0 = n/a)
    void*             pixels;     // CPU-addressable pixels (software / shared)
    uint8_t           owns_pixels;// 1 = manager frees pixels, 0 = borrowed
} gpu_surface_t;

// --- Pure helpers (implemented in graphics/hal/surface.c) ---
// Clip `r` to surface bounds. Returns 1 if any area remains.
int gpu_surface_clip(const gpu_surface_t* s, gpu_rect_t* r);
// Row pointer (row 0..height-1), or NULL when invalid.
uint32_t* gpu_surface_row(const gpu_surface_t* s, uint32_t row);
// Bytes per pixel of the surface format.
int gpu_surface_bpp(const gpu_surface_t* s);

#ifdef __cplusplus
}
#endif

#endif // GRAPHICS_SURFACE_H
