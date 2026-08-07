#ifndef GRAPHICS_SURFACE_MANAGER_H
#define GRAPHICS_SURFACE_MANAGER_H

// ============================================================
// Surface Manager (include/graphics/surface_manager.h)
//
// Owns the set of live surfaces. Responsibilities:
//   - allocate/free surface structs (bounded static pool — no heap
//     churn in hot paths)
//   - assign unique handles
//   - reference counting (retain/release)
//   - optional VRAM offset allocation (dedicated VRAM backends)
//
// SMP-safe: all list/refcount mutation under one spinlock.
// ============================================================

#include <stdint.h>
#include "graphics/surface.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GPU_MAX_SURFACES 64

// Initialize the surface manager. Safe to call once at boot.
void gpu_surface_manager_init(void);

// Create a surface backed by system RAM. width/height in pixels.
// Returns handle (>= 1) or 0 on failure. Pixels are zeroed.
// kind selects front/back/window/offscreen/texture semantics.
uint32_t gpu_surface_create(uint32_t width, uint32_t height,
                            gpu_pixel_format_t format, gpu_surface_kind_t kind);

// Create a surface wrapping caller-owned memory (borrowed, not freed).
// Returns handle (>=1) or 0 on failure.
uint32_t gpu_surface_wrap(void* pixels, uint32_t width, uint32_t height,
                          uint32_t stride, gpu_pixel_format_t format,
                          gpu_surface_kind_t kind);

// Bump refcount. Returns handle or 0.
uint32_t gpu_surface_retain(uint32_t handle);

// Drop a reference; destroys surface when count hits 0.
void gpu_surface_release(uint32_t handle);

// Fetch a surface by handle (manager lock NOT held on return; caller
// must not free the returned pointer). Returns NULL if invalid.
gpu_surface_t* gpu_surface_get(uint32_t handle);

// Iterate live surfaces via callback. Callback runs while the manager
// lock is held — do not call surface manager functions inside it.
void gpu_surface_foreach(void (*cb)(const gpu_surface_t* s, void* ctx),
                         void* ctx);

// GPU memory manager for dedicated VRAM backends.
// Allocate/align a block of VRAM. Returns offset or 0 on failure.
uint32_t gpu_vram_alloc(uint32_t size, uint32_t align);
// Free a previously-allocated VRAM offset+size (size must match alloc).
void     gpu_vram_free(uint32_t offset, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif // GRAPHICS_SURFACE_MANAGER_H
