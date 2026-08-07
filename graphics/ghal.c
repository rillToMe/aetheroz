// ============================================================
// Graphics HAL — dispatch, registry & backend selection
// (graphics/ghal.c)
//
// Menyimpan daftar backend, memilih backend aktif saat ghal_init()
// (urutan tetap: virtio-gpu → software, roadmap §6.1), dan me-route
// semua panggilan compositor ke vtable backend aktif. Tidak pernah
// menyentuh MMIO/register GPU — itu urusan driver core.
//
// Concurrency: registry/selection di-lock; panggilan ghal_* ke backend
// aktif diasumsikan single-context per frame (compositor), konsisten
// dengan model v1 di roadmap §6.6.
// ============================================================

#include "ghal.h"
#include "spinlock.h"
#include <stddef.h>   // NULL

#define GHAL_MAX_BACKENDS 8

static const ghal_backend_ops_t* g_backends[GHAL_MAX_BACKENDS];
static const ghal_backend_ops_t* g_active;
static spinlock_t g_lock = SPINLOCK_INIT;
static const char* g_last_error = "";

// Didefinisikan di graphics/select.c (backend software) dan
// graphics/backend/virtio_gpu.c (Phase 2A). software tidak pernah gagal.
extern const ghal_backend_ops_t software_backend_ops;
extern const ghal_backend_ops_t virtio_gpu_backend_ops;

int ghal_register_backend(const ghal_backend_ops_t* ops) {
    if (ops == NULL || ops->init == NULL || ops->surface_create == NULL ||
        ops->surface_destroy == NULL || ops->surface_upload == NULL ||
        ops->fill_rect == NULL || ops->blit == NULL || ops->present == NULL) {
        g_last_error = "ghal_register_backend: incomplete ops";
        return -1;
    }
    uint64_t flags = spinlock_lock_irqsave(&g_lock);
    for (int i = 0; i < GHAL_MAX_BACKENDS; i++) {
        if (g_backends[i] == NULL) {
            g_backends[i] = ops;
            spinlock_unlock_irqrestore(&g_lock, flags);
            return 0;
        }
    }
    spinlock_unlock_irqrestore(&g_lock, flags);
    g_last_error = "ghal_register_backend: table full";
    return -1;
}

int ghal_init(void) {
    uint64_t flags = spinlock_lock_irqsave(&g_lock);
    if (g_active != NULL) {
        spinlock_unlock_irqrestore(&g_lock, flags);
        return 0;
    }

    // Urutan tetap (roadmap §6.1/§6.10): virtio-gpu dulu, software fallback.
    const ghal_backend_ops_t* order[GHAL_MAX_BACKENDS];
    int n = 0;
    order[n++] = &virtio_gpu_backend_ops;   // coba dulu
    order[n++] = &software_backend_ops;     // fallback tak pernah gagal

    for (int i = 0; i < n; i++) {
        const ghal_backend_ops_t* ops = order[i];
        if (ops->init && ops->init() == 0) {
            g_active = ops;
            g_last_error = "";
            spinlock_unlock_irqrestore(&g_lock, flags);
            return 0;
        }
    }

    g_active = NULL;
    g_last_error = "ghal_init: no backend initialized";
    spinlock_unlock_irqrestore(&g_lock, flags);
    return -1;
}

void ghal_shutdown(void) {
    uint64_t flags = spinlock_lock_irqsave(&g_lock);
    if (g_active && g_active->shutdown) g_active->shutdown();
    g_active = NULL;
    spinlock_unlock_irqrestore(&g_lock, flags);
}

const char* ghal_active_backend_name(void) {
    return g_active ? g_active->name : "(none)";
}

uint32_t ghal_capabilities(void) {
    return g_active ? g_active->capabilities : 0;
}

const char* ghal_last_error(void) {
    return g_last_error;
}

// ------------------------------------------------------------
// Dispatch ke backend aktif (NULL-safe).
// ------------------------------------------------------------

ghal_surface_t* ghal_surface_create(uint32_t w, uint32_t h, ghal_format_t fmt) {
    if (!g_active) { g_last_error = "no active backend"; return NULL; }
    return g_active->surface_create(w, h, fmt);
}

void ghal_surface_destroy(ghal_surface_t* s) {
    if (!g_active || !s) return;
    g_active->surface_destroy(s);
}

void ghal_surface_upload(ghal_surface_t* s, const uint32_t* src,
                         uint32_t src_pitch, ghal_rect_t rect) {
    if (!g_active || !s || !src) return;
    g_active->surface_upload(s, src, src_pitch, rect);
}

void ghal_fill_rect(ghal_surface_t* dst, ghal_rect_t rect, uint32_t argb) {
    if (!g_active || !dst) return;
    g_active->fill_rect(dst, rect, argb);
}

void ghal_blit(ghal_surface_t* dst, ghal_rect_t dst_rect,
               ghal_surface_t* src, ghal_rect_t src_rect) {
    if (!g_active || !dst || !src) return;
    g_active->blit(dst, dst_rect, src, src_rect);
}

void ghal_present(ghal_surface_t* s, const ghal_rect_t* rect) {
    if (!g_active || !s) return;
    g_active->present(s, rect);
}
