#ifndef GRAPHICS_GPU_H
#define GRAPHICS_GPU_H

// ============================================================
// Graphics HAL — GPU interface (include/graphics/gpu.h)
//
// Vendor-agnostic 2D acceleration API. Higher layers (renderer,
// compositor) talk ONLY to this interface. Backends (software,
// virtio-gpu, svga, bochs, intel, ...) implement gpu_backend_ops.
// Vendor registers never leak above the backend.
//
// Everything here is deliberately header-only types + a small
// dispatch layer implemented in hal/gpu.c. Hot rendering paths do
// NOT allocate: fill/blit/blend take caller-owned surfaces.
// ============================================================

#include <stdint.h>
#include <stddef.h>
#include "graphics/surface.h"

#ifdef __cplusplus
extern "C" {
#endif

// ------------------------------------------------------------
// Ops results
// ------------------------------------------------------------
typedef enum {
    GPU_OK            = 0,
    GPU_ERR_UNSUPPORTED = -1,   // backend has no such op / pixel format
    GPU_ERR_NOMEM       = -2,   // allocation failed
    GPU_ERR_INVALID_ARG = -3,
    GPU_ERR_NO_GPU      = -4,   // no backend registered
    GPU_ERR_BUSY        = -5,   // wait_idle / command queue busy
} gpu_result_t;

// ------------------------------------------------------------
// Fill / blend flags (gpu_rect_t & gpu_pixel_format_t in surface.h)
// ------------------------------------------------------------
#define GPU_BLEND_NONE   0x00      // overwrite
#define GPU_BLEND_ALPHA  0x01      // src alpha over dst
#define GPU_BLEND_KEY    0x02      // color-key transparency (top byte 0 = skip)

// ------------------------------------------------------------
// Backend ops vtable.
//
// A backend fills every member. "surface" here is a generic
// software-addressable buffer; the software backend operates on
// surface->pixels directly. A future VRAM backend translates these
// ops to device commands but keeps the same signatures.
// ------------------------------------------------------------
typedef struct gpu_backend gpu_backend_t;

typedef struct {
    // Lifecycle
    gpu_result_t (*initialize)(gpu_backend_t* self);
    void         (*shutdown)(gpu_backend_t* self);

    // Surface lifetime (front/back/window/offscreen/texture)
    gpu_result_t (*create_surface)(gpu_backend_t* self, gpu_surface_t* s);
    void         (*destroy_surface)(gpu_backend_t* self, gpu_surface_t* s);

    // Texture upload (source bytes -> surface pixels)
    gpu_result_t (*upload_texture)(gpu_backend_t* self, gpu_surface_t* dst,
                                   const void* src, size_t src_stride_bytes);

    // Drawing (all clip to surface bounds)
    gpu_result_t (*fill_rect)(gpu_backend_t* self, gpu_surface_t* dst,
                              const gpu_rect_t* r, uint32_t color);
    gpu_result_t (*draw_line)(gpu_backend_t* self, gpu_surface_t* dst,
                              int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                              uint32_t color);
    gpu_result_t (*draw_image)(gpu_backend_t* self, gpu_surface_t* dst,
                               int32_t dst_x, int32_t dst_y,
                               const gpu_surface_t* src,
                               const gpu_rect_t* src_rect);
    gpu_result_t (*blit)(gpu_backend_t* self, gpu_surface_t* dst,
                         int32_t dst_x, int32_t dst_y,
                         const gpu_surface_t* src, const gpu_rect_t* src_rect,
                         uint32_t flags);
    gpu_result_t (*stretch_blit)(gpu_backend_t* self, gpu_surface_t* dst,
                                 const gpu_rect_t* dst_rect,
                                 const gpu_surface_t* src,
                                 const gpu_rect_t* src_rect, uint32_t flags);
    gpu_result_t (*alpha_blend)(gpu_backend_t* self, gpu_surface_t* dst,
                                const gpu_rect_t* dst_rect,
                                const gpu_surface_t* src,
                                const gpu_rect_t* src_rect);

    // Presentation / sync. present() may be a no-op on software backend.
    gpu_result_t (*present)(gpu_backend_t* self, const gpu_surface_t* front,
                            const gpu_rect_t* damage, uint32_t damage_count);
    gpu_result_t (*flush)(gpu_backend_t* self);
    gpu_result_t (*wait_idle)(gpu_backend_t* self);
} gpu_backend_ops_t;

// A registered backend instance. name is static storage.
struct gpu_backend {
    const char*        name;
    const gpu_backend_ops_t* ops;
    void*              priv;        // backend-private state
    // Active front buffer (what present() copies/draws to).
    gpu_surface_t      front;
    uint8_t            initialized;
};

// ------------------------------------------------------------
// HAL dispatch (implemented in hal/gpu.c)
// ------------------------------------------------------------

// Register a backend; the first successfully-initialized backend becomes
// active. Returns GPU_OK or GPU_ERR_NOMEM/GPU_ERR_UNSUPPORTED.
gpu_result_t gpu_register_backend(gpu_backend_t* backend);

// Probe+init all registered backends, pick the first that works.
gpu_result_t gpu_initialize(void);

// Active backend (NULL if none). Lowercase convenience for callers.
gpu_backend_t* gpu_active(void);

void gpu_shutdown(void);

// Thin wrappers over gpu_active()->ops (NULL-safe, return GPU_ERR_NO_GPU).
gpu_result_t gpu_create_surface(gpu_surface_t* s);
void         gpu_destroy_surface(gpu_surface_t* s);
gpu_result_t gpu_upload_texture(gpu_surface_t* dst, const void* src, size_t src_stride);
gpu_result_t gpu_fill_rect(gpu_surface_t* dst, const gpu_rect_t* r, uint32_t color);
gpu_result_t gpu_draw_line(gpu_surface_t* dst, int32_t x0, int32_t y0,
                           int32_t x1, int32_t y1, uint32_t color);
gpu_result_t gpu_draw_image(gpu_surface_t* dst, int32_t dx, int32_t dy,
                            const gpu_surface_t* src, const gpu_rect_t* src_rect);
gpu_result_t gpu_blit(gpu_surface_t* dst, int32_t dx, int32_t dy,
                      const gpu_surface_t* src, const gpu_rect_t* src_rect,
                      uint32_t flags);
gpu_result_t gpu_stretch_blit(gpu_surface_t* dst, const gpu_rect_t* dr,
                              const gpu_surface_t* src, const gpu_rect_t* sr,
                              uint32_t flags);
gpu_result_t gpu_alpha_blend(gpu_surface_t* dst, const gpu_rect_t* dr,
                             const gpu_surface_t* src, const gpu_rect_t* sr);
gpu_result_t gpu_present(const gpu_rect_t* damage, uint32_t damage_count);
gpu_result_t gpu_flush(void);
gpu_result_t gpu_wait_idle(void);

#ifdef __cplusplus
}
#endif

#endif // GRAPHICS_GPU_H
