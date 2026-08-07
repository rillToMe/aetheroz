// ============================================================
// VirtIO-GPU Backend (graphics/backend/virtio_gpu.c)
//
// Menerjemahkan ghal_backend_ops_t ke command VirtIO-GPU lewat GPU
// Driver Core. Phase 2A: bring-up + resource + scanout + present
// (target: satu warna solid full-screen di QEMU).
//
// struct ghal_surface (opaque) untuk backend ini:
//   resource_id + daftar backing pages (host RAM) + geometry.
// ============================================================

#include "ghal.h"
#include "virtio_gpu_dev.h"
#include "virtio_gpu_cmd.h"
#include "gpu_alloc.h"
#include "heap.h"
#include <string.h>
#include <stddef.h>

// Format VirtIO untuk XRGB8888 kita: byte [B,G,R,X] = X8R8G8B8_UNORM.

struct ghal_surface {
    uint32_t    resource_id;
    uint32_t    width;
    uint32_t    height;
    ghal_format_t format;
    gpu_page_t* pages;         // backing pages (contiguous logical list)
    uint32_t    num_pages;
    uint32_t*   backing_virt;  // = pages[0].virt (linear)
    uint64_t    backing_phys;  // = pages[0].phys (untuk attach)
    uint8_t     scanout_set;   // 1 = sudah SET_SCANOUT
};

static int g_virtio_active = 0;

// --- helper: kirim command & periksa response OK_NODATA ---
static int vgpu_send_ok(const void* cmd, uint32_t cmd_len) {
    uint32_t resp[8] = {0};   // ctrl_hdr response
    if (virtio_gpu_dev_command(cmd, cmd_len, resp, sizeof(resp)) != 0) return -1;
    if (resp[0] != VIRTIO_GPU_RESP_OK_NODATA) return -1;
    return 0;
}

static int virtio_init(void) {
    if (g_virtio_active) return 0;
    if (virtio_gpu_dev_probe() != 0) return -1;

    // GET_DISPLAY_INFO → simpan resolusi.
    virtio_gpu_ctrl_hdr_t cmd;
    virtio_gpu_cmd_get_display_info(&cmd);
    virtio_gpu_resp_display_info_t resp;
    memset(&resp, 0, sizeof(resp));
    if (virtio_gpu_dev_command(&cmd, sizeof(cmd), &resp, sizeof(resp)) != 0) return -1;
    if (resp.hdr.type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO) return -1;
    if (resp.pmodes[0].enabled == 0) return -1;
    g_vgpu.scanout_width  = resp.pmodes[0].rect.width;
    g_vgpu.scanout_height = resp.pmodes[0].rect.height;

    g_virtio_active = 1;
    return 0;
}

static void virtio_shutdown(void) { g_virtio_active = 0; }

void virtio_backend_get_size(uint32_t* w, uint32_t* h) {
    if (w) *w = g_vgpu.scanout_width;
    if (h) *h = g_vgpu.scanout_height;
}

// --- surface_create: buat resource + attach backing ---
static ghal_surface_t* virtio_surface_create(uint32_t w, uint32_t h, ghal_format_t fmt) {
    if (w == 0 || h == 0 || w > GHAL_MAX_DIM || h > GHAL_MAX_DIM) return NULL;
    uint64_t bytes = (uint64_t)w * h * 4;
    if (bytes == 0 || w != 0 && (bytes / 4 / w) != h) return NULL;
    uint32_t num_pages = (uint32_t)((bytes + 4095) / 4096);

    gpu_page_t* pages = (gpu_page_t*)kmalloc(sizeof(gpu_page_t) * num_pages);
    if (!pages) { extern void serial_print(const char* s); serial_print("[vgpu] surface: kmalloc pages failed\n"); return NULL; }
    uint32_t n = gpu_alloc_pages(num_pages, pages);
    if (n == 0) { extern void serial_print(const char* s); serial_print("[vgpu] surface: alloc pages 0\n"); kfree(pages); return NULL; }
    if (n < num_pages) { extern void serial_print(const char* s); serial_print("[vgpu] surface: partial pages\n"); gpu_free_pages(pages, n); kfree(pages); return NULL; }

    struct ghal_surface* s = (struct ghal_surface*)kmalloc(sizeof(*s));
    if (!s) { gpu_free_pages(pages, num_pages); kfree(pages); return NULL; }
    s->resource_id = virtio_gpu_next_resource_id();
    s->width = w; s->height = h; s->format = fmt;
    s->pages = pages; s->num_pages = num_pages;
    s->backing_virt = pages[0].virt;
    s->backing_phys = pages[0].phys;
    s->scanout_set = 0;

    // RESOURCE_CREATE_2D
    // Format: XRGB8888 kita = 0x00RRGGBB, little-endian memory byte order
    // B,G,R,X → VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM (nama virtio = urutan BYTE).
    // Memakai X8R8G8B8 (urutan X,R,G,B) membuat device membaca byte0 sebagai X
    // dan byte3 sebagai B → channel biru hilang (terbukti: gray 30,30,30
    // tampil 30,30,0).
    virtio_gpu_resource_create_2d_t c;
    virtio_gpu_cmd_resource_create_2d(&c, s->resource_id,
                                      VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM, w, h);
    if (vgpu_send_ok(&c, sizeof(c)) != 0) {
        gpu_free_pages(pages, num_pages); kfree(pages); kfree(s);
        return NULL;
    }

    // ATTACH_BACKING: satu entry per halaman.
    // Bangun command buffer dengan array entries.
    uint32_t nr = num_pages;
    size_t cmd_size = sizeof(virtio_gpu_resource_attach_backing_t) + nr * sizeof(virtio_gpu_mem_entry_t);
    virtio_gpu_resource_attach_backing_t* ac =
        (virtio_gpu_resource_attach_backing_t*)kmalloc(cmd_size);
    if (!ac) { virtio_gpu_cmd_resource_unref(&(virtio_gpu_resource_unref_t){0}, s->resource_id); gpu_free_pages(pages,num_pages); kfree(pages); kfree(s); return NULL; }
    virtio_gpu_cmd_attach_backing(ac, s->resource_id, nr);
    for (uint32_t i = 0; i < nr; i++) {
        ac->entries[i].addr = pages[i].phys;
        ac->entries[i].length = 4096;
        ac->entries[i].padding = 0;
    }
    int ok = vgpu_send_ok(ac, (uint32_t)cmd_size);
    kfree(ac);
    if (ok != 0) {
        // cleanup: unref resource
        virtio_gpu_resource_unref_t ur;
        virtio_gpu_cmd_resource_unref(&ur, s->resource_id);
        vgpu_send_ok(&ur, sizeof(ur));
        gpu_free_pages(pages, num_pages); kfree(pages); kfree(s);
        return NULL;
    }

    // SET_SCANOUT: surface full-screen (== resolusi scanout) jadi output.
    if (w == g_vgpu.scanout_width && h == g_vgpu.scanout_height) {
        virtio_gpu_set_scanout_t so;
        virtio_gpu_cmd_set_scanout(&so, 0, s->resource_id, 0, 0, w, h);
        vgpu_send_ok(&so, sizeof(so));
    }

    return s;
}

static void virtio_surface_destroy(ghal_surface_t* s) {
    if (!s) return;
    // DETACH_BACKING lalu UNREF.
    virtio_gpu_ctrl_hdr_t det;
    virtio_gpu_cmd_detach_backing(&det, s->resource_id);
    vgpu_send_ok(&det, sizeof(det));
    virtio_gpu_resource_unref_t ur;
    virtio_gpu_cmd_resource_unref(&ur, s->resource_id);
    vgpu_send_ok(&ur, sizeof(ur));
    gpu_free_pages(s->pages, s->num_pages);
    kfree(s->pages);
    kfree(s);
}

static void virtio_surface_upload(ghal_surface_t* s, const uint32_t* src,
                                  uint32_t src_pitch, ghal_rect_t rect) {
    if (!s || !src) return;
    if (rect.x >= s->width || rect.y >= s->height) return;
    uint32_t maxw = s->width - rect.x, maxh = s->height - rect.y;
    if (rect.w > maxw) rect.w = maxw;
    if (rect.h > maxh) rect.h = maxh;
    const uint32_t* srow = src + (uint64_t)rect.y * src_pitch + rect.x;
    uint32_t* drow = s->backing_virt + (uint64_t)rect.y * s->width + rect.x;
    for (uint32_t y = 0; y < rect.h; y++) {
        memcpy(drow, srow, rect.w * 4);
        srow += src_pitch;
        drow += s->width;
    }
}

static void virtio_fill_rect(ghal_surface_t* dst, ghal_rect_t rect, uint32_t argb) {
    if (!dst) return;
    if (rect.x >= dst->width || rect.y >= dst->height) return;
    uint32_t maxw = dst->width - rect.x, maxh = dst->height - rect.y;
    if (rect.w > maxw) rect.w = maxw;
    if (rect.h > maxh) rect.h = maxh;
    uint32_t color = argb & 0xFFFFFF;
    for (uint32_t y = 0; y < rect.h; y++) {
        uint32_t* row = dst->backing_virt + (uint64_t)(rect.y + y) * dst->width + rect.x;
        for (uint32_t x = 0; x < rect.w; x++) row[x] = color;
    }
}

static void virtio_blit(ghal_surface_t* dst, ghal_rect_t dst_rect,
                        ghal_surface_t* src, ghal_rect_t src_rect) {
    if (!dst || !src) return;
    if (dst_rect.w != src_rect.w || dst_rect.h != src_rect.h) return;   // v1 no scaling
    if (dst_rect.x >= dst->width || dst_rect.y >= dst->height) return;
    uint32_t w = dst_rect.w, h = dst_rect.h;
    if (w > dst->width - dst_rect.x) w = dst->width - dst_rect.x;
    if (h > dst->height - dst_rect.y) h = dst->height - dst_rect.y;
    if (src_rect.x >= src->width || src_rect.y >= src->height) return;
    const uint32_t* srow = src->backing_virt + (uint64_t)src_rect.y * src->width + src_rect.x;
    uint32_t* drow = dst->backing_virt + (uint64_t)dst_rect.y * dst->width + dst_rect.x;
    for (uint32_t y = 0; y < h; y++) {
        memcpy(drow, srow, w * 4);
        srow += src->width;
        drow += dst->width;
    }
}

static void virtio_present(ghal_surface_t* s, const ghal_rect_t* rect) {
    if (!s) return;
    ghal_rect_t r;
    if (rect) r = *rect;
    else { r.x = 0; r.y = 0; r.w = s->width; r.h = s->height; }

    // SET_SCANOUT sekali per resource (idempotent di device, tapi kita
    // lakukan sekali saja): pastikan resource ini yang jadi output aktif.
    if (!s->scanout_set && s->width == g_vgpu.scanout_width &&
        s->height == g_vgpu.scanout_height) {
        virtio_gpu_set_scanout_t so;
        virtio_gpu_cmd_set_scanout(&so, 0, s->resource_id, 0, 0, s->width, s->height);
        if (vgpu_send_ok(&so, sizeof(so)) == 0) s->scanout_set = 1;
    }

    // TRANSFER_TO_HOST_2D
    virtio_gpu_transfer_to_host_2d_t t;
    virtio_gpu_cmd_transfer_to_host(&t, s->resource_id, r.x, r.y, r.w, r.h);
    vgpu_send_ok(&t, sizeof(t));

    // RESOURCE_FLUSH
    virtio_gpu_resource_flush_t f;
    virtio_gpu_cmd_resource_flush(&f, s->resource_id, r.x, r.y, r.w, r.h);
    vgpu_send_ok(&f, sizeof(f));
}

const ghal_backend_ops_t virtio_gpu_backend_ops = {
    .name           = "virtio-gpu",
    .capabilities   = GHAL_CAP_PARTIAL_FLUSH,
    .init           = virtio_init,
    .shutdown       = virtio_shutdown,
    .surface_create = virtio_surface_create,
    .surface_destroy= virtio_surface_destroy,
    .surface_upload = virtio_surface_upload,
    .fill_rect      = virtio_fill_rect,
    .blit           = virtio_blit,
    .present        = virtio_present,
    .cursor_update  = NULL,   // Phase 2C
    .cursor_move    = NULL,
};
