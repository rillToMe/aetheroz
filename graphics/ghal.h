#ifndef GHAL_H
#define GHAL_H

// ============================================================
// KyuzenOS Graphics HAL — kontrak publik (graphics/ghal.h)
//
// Layer ini adalah satu-satunya cara compositor/window manager
// berkomunikasi dengan GPU. Backend (software / virtio-gpu / ...)
// mengimplementasikan ghal_backend_ops_t; HAL memilih backend aktif
// saat ghal_init() dan me-route semua panggilan ke vtable tersebut.
//
// Coding rules (roadmap §5):
//   - ghal_* tidak pernah menyentuh MMIO/register GPU.
//   - compositor tidak pernah memanggil backend secara langsung.
//   - ghal_surface_t adalah OPAQUE: tiap backend mendefinisikan
//     struct konkretnya sendiri di file .c-nya.
// ============================================================

#include <stdint.h>

// Format piksel. Backend WAJIB handle semua ini, atau menolak
// surface_create dengan error eksplisit (tidak boleh silent fallback).
typedef enum {
    GHAL_FMT_XRGB8888 = 0,   // format native framebuffer software
    GHAL_FMT_ARGB8888 = 1,   // dengan alpha channel (dipakai untuk cursor)
} ghal_format_t;

typedef struct ghal_surface ghal_surface_t;   // opaque — internal per-backend

typedef struct {
    uint32_t x, y, w, h;
} ghal_rect_t;

// Capability flags — compositor query TANPA tahu backend aktif.
#define GHAL_CAP_HW_CURSOR      (1u << 0)   // hardware cursor plane
#define GHAL_CAP_ASYNC_PRESENT  (1u << 1)   // present non-blocking (fence-based)
#define GHAL_CAP_PARTIAL_FLUSH  (1u << 2)   // resource_flush per-rect

typedef struct {
    const char* name;                        // "software" / "virtio-gpu"
    uint32_t    capabilities;                // bitmask GHAL_CAP_*

    int  (*init)(void);                      // 0 sukses, <0 gagal
    void (*shutdown)(void);

    ghal_surface_t* (*surface_create)(uint32_t w, uint32_t h, ghal_format_t fmt);
    void            (*surface_destroy)(ghal_surface_t* s);

    // Upload dari system memory (canvas app) ke surface backend.
    // WAJIB ada di semua backend — software backend = memcpy.
    void (*surface_upload)(ghal_surface_t* s, const uint32_t* src,
                           uint32_t src_pitch, ghal_rect_t rect);

    void (*fill_rect)(ghal_surface_t* dst, ghal_rect_t rect, uint32_t argb);
    void (*blit)(ghal_surface_t* dst, ghal_rect_t dst_rect,
                 ghal_surface_t* src, ghal_rect_t src_rect);

    // Present: surface jadi scanout aktif. rect==NULL = full flush.
    void (*present)(ghal_surface_t* s, const ghal_rect_t* rect);

    // Hardware cursor (opsional — cek GHAL_CAP_HW_CURSOR sebelum panggil).
    // Backend tanpa cap ini WAJIB set ke NULL, bukan no-op silent.
    void (*cursor_update)(ghal_surface_t* cursor_img, int hot_x, int hot_y);
    void (*cursor_move)(int x, int y);
} ghal_backend_ops_t;

// --- API publik dipanggil compositor ---
int  ghal_init(void);                        // pilih & init backend
void ghal_shutdown(void);
const char* ghal_active_backend_name(void);
uint32_t    ghal_capabilities(void);

ghal_surface_t* ghal_surface_create(uint32_t w, uint32_t h, ghal_format_t fmt);
void            ghal_surface_destroy(ghal_surface_t* s);
void ghal_surface_upload(ghal_surface_t* s, const uint32_t* src,
                         uint32_t src_pitch, ghal_rect_t rect);
void ghal_fill_rect(ghal_surface_t* dst, ghal_rect_t rect, uint32_t argb);
void ghal_blit(ghal_surface_t* dst, ghal_rect_t dst_rect,
               ghal_surface_t* src, ghal_rect_t src_rect);
void ghal_present(ghal_surface_t* s, const ghal_rect_t* rect);

// Diagnostics: pesan error statis dari operasi terakhir yang gagal.
const char* ghal_last_error(void);

// Ukuran scanout yang dipilih backend aktif (resolusi output). Software backend
// memakai ukuran framebuffer Limine; virtio-gpu memakai pmodes[0]. Compositor
// memakai ini untuk menentukan ukuran main surface.
void ghal_scanout_size(uint32_t* w, uint32_t* h);

// Beri tahu HAL/backend software di mana framebuffer hardware berada.
// WAJIB dipanggil sebelum ghal_init() supaya software backend bisa present.
// (virtio-gpu backend mengabaikan ini — dia punya scanout sendiri.)
void ghal_set_framebuffer(uint32_t* fb, uint32_t width, uint32_t height,
                          uint32_t pitch_bytes);

// Internal: daftarkan backend. Dipanggil dari graphics/select.c sebelum
// ghal_init(). Backend pertama yang init sukses menjadi aktif.
int ghal_register_backend(const ghal_backend_ops_t* ops);

#endif // GHAL_H
