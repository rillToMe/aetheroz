#ifndef GRAPHICS_GRAPHICS_H
#define GRAPHICS_GRAPHICS_H

// ============================================================
// High-level 2D Graphics API (include/graphics/graphics.h)
//
// The renderer is the layer above the HAL. It groups primitive
// calls into command batches, tracks a dirty rectangle set, and
// drives double buffering / present. The compositor (KWM) will
// call this instead of touching raw framebuffers.
//
// This is a thin, allocation-free front-end over the HAL + surface
// manager. It does not know which backend is active.
// ============================================================

#include <stdint.h>
#include "graphics/surface.h"
#include "graphics/gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

// A software "canvas" the renderer draws into, then presents.
// For now this is just a convenience handle to a back surface.
typedef struct gfx_canvas {
    gpu_surface_t* surface;   // back buffer surface
    gpu_rect_t     damage;    // accumulated damage (union of dirty rects)
    uint32_t       dirty;     // nonzero = damage valid
} gfx_canvas_t;

// Initialize the renderer: pick backend, create front+back surfaces.
// front_kind tells the manager how to treat the front buffer.
gpu_result_t gfx_graphics_init(void);

// Get / release a render target (back buffer). Borrowed by caller.
gfx_canvas_t* gfx_canvas_acquire(void);
void          gfx_canvas_release(gfx_canvas_t* c);

// --- Primitive draws (all go to the given canvas, mark it dirty) ---

void gfx_fill_rect(gfx_canvas_t* c, const gpu_rect_t* r, uint32_t color);
void gfx_draw_line(gfx_canvas_t* c, int32_t x0, int32_t y0,
                   int32_t x1, int32_t y1, uint32_t color);
void gfx_draw_image(gfx_canvas_t* c, int32_t dx, int32_t dy,
                    const gpu_surface_t* src, const gpu_rect_t* src_rect);
void gfx_blit(gfx_canvas_t* c, int32_t dx, int32_t dy,
              const gpu_surface_t* src, const gpu_rect_t* src_rect,
              uint32_t flags);
void gfx_stretch_blit(gfx_canvas_t* c, const gpu_rect_t* dr,
                      const gpu_surface_t* src, const gpu_rect_t* sr,
                      uint32_t flags);

// --- Presentation ---
// Present the canvas's dirty region(s). On the software backend this
// copies the damage from back to front buffer. `full` forces the whole
// surface.
void gfx_present(gfx_canvas_t* c, int full);
void gfx_clear_damage(gfx_canvas_t* c);

// --- Misc ---
void gfx_graphics_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // GRAPHICS_GRAPHICS_H
