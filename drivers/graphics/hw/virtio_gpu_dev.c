// ============================================================
// VirtIO-GPU Driver Core (drivers/graphics/hw/virtio_gpu_dev.c)
//
// PCI → MMIO → negotiation → virtqueue → command execution.
// Akses MMIO via hhdm_offset (HHDM me-map physical space). Semua
// akses register lewat pointer volatile.
// ============================================================

#include "virtio_gpu_dev.h"
#include "pci.h"          // pci_read_word
#include "io.h"
#include "gpu_alloc.h"
#include <string.h>

extern uint64_t hhdm_offset;

virtio_gpu_dev_t g_vgpu;

static void serial_log(const char* s) {
    extern void serial_print(const char* s);
    serial_print(s);
}

// --- PCI config space 32-bit read (lewat pci_read_word) ---
static uint32_t pci_cfg_read32(uint16_t bus, uint16_t slot, uint16_t func, uint8_t off) {
    uint32_t lo = (uint32_t)pci_read_word((uint8_t)bus, (uint8_t)slot, (uint8_t)func, off);
    uint32_t hi = (uint32_t)pci_read_word((uint8_t)bus, (uint8_t)slot, (uint8_t)func, (uint8_t)(off + 2));
    return lo | (hi << 16);
}

// --- PCI config space BYTE read (offset apapun, benar untuk offset odd) ---
// pci_read_word hanya bekerja untuk offset even; untuk byte offset odd kita
// ambil word di offset&~1 lalu pilih byte berdasarkan offset&1.
static uint8_t pci_cfg_read8(uint16_t bus, uint16_t slot, uint16_t func, uint8_t off) {
    uint16_t w = pci_read_word((uint8_t)bus, (uint8_t)slot, (uint8_t)func, (uint8_t)(off & (uint8_t)~1));
    return (off & 1) ? (uint8_t)(w >> 8) : (uint8_t)(w & 0xFF);
}

// --- Scan PCI bus untuk device VirtIO-GPU (0x1AF4 / 0x1050) ---
static int pci_find_virtio_gpu(void) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint16_t slot = 0; slot < 32; slot++) {
            for (uint16_t func = 0; func < 8; func++) {
                uint16_t vid = pci_read_word((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0);
                if (vid == 0xFFFF) { if (func == 0) break; continue; }
                uint16_t did = pci_read_word((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 2);
                if (vid == VIRTIO_PCI_VENDOR_ID && did == VIRTIO_PCI_DEVICE_GPU) {
                    // Validasi class code = 0x03 (Display).
                    uint16_t class_info = pci_read_word((uint8_t)bus, (uint8_t)slot, (uint8_t)func, 0x0A);
                    uint8_t class_code = (class_info >> 8) & 0xFF;
                    if (class_code != 0x03) continue;
                    g_vgpu.bus = bus; g_vgpu.slot = slot; g_vgpu.func = func;
                    return 1;
                }
                if (func == 0) break;   // single-function optimization
            }
        }
    }
    return 0;
}

// --- PCI capability list parsing (cap_vndr == 0x09 = VirtIO) ---
// Layout virtio_pci_cap:
//   +0 cap_vndr, +1 cap_next, +2 cap_len, +3 cfg_type, +4 bar,
//   +5..+7 padding[3], +8 offset(32), +12 length(32)
// virtio_pci_notify_cap menambah notify_off_multiplier(32) di +16.
static int parse_capabilities(void) {
    uint8_t cap_ptr = pci_cfg_read8(g_vgpu.bus, g_vgpu.slot, g_vgpu.func, 0x34);
    int found_common = 0, found_notify = 0, found_isr = 0, found_dev = 0;

    while (cap_ptr != 0) {
        uint8_t cap_vndr = pci_cfg_read8(g_vgpu.bus, g_vgpu.slot, g_vgpu.func, cap_ptr);
        uint8_t cap_next = pci_cfg_read8(g_vgpu.bus, g_vgpu.slot, g_vgpu.func, (uint8_t)(cap_ptr + 1));
        if (cap_vndr != 0x09) { cap_ptr = cap_next; continue; }

        uint8_t cfg_type = pci_cfg_read8(g_vgpu.bus, g_vgpu.slot, g_vgpu.func, (uint8_t)(cap_ptr + 3));
        uint8_t bar = pci_cfg_read8(g_vgpu.bus, g_vgpu.slot, g_vgpu.func, (uint8_t)(cap_ptr + 4));
        uint32_t bar_off = pci_cfg_read32(g_vgpu.bus, g_vgpu.slot, g_vgpu.func, (uint8_t)(cap_ptr + 8));

        // Baca BAR phys (32-bit) dari config space offset 0x10 + bar*4.
        uint32_t bar_phys = pci_cfg_read32(g_vgpu.bus, g_vgpu.slot, g_vgpu.func, (uint8_t)(0x10 + bar * 4));
        bar_phys &= ~0xF;   // clear flags (memory space)

        switch (cfg_type) {
            case VIRTIO_PCI_CAP_COMMON_CFG:
                g_vgpu.common_cfg_phys = bar_phys;
                g_vgpu.common_cfg_off  = bar_off;
                found_common = 1;
                break;
            case VIRTIO_PCI_CAP_NOTIFY_CFG:
                g_vgpu.notify_base_phys = bar_phys;
                g_vgpu.notify_base_off  = bar_off;
                // notify_off_multiplier ada di +16 (virtio_pci_notify_cap).
                g_vgpu.notify_off_multiplier =
                    pci_cfg_read32(g_vgpu.bus, g_vgpu.slot, g_vgpu.func, (uint8_t)(cap_ptr + 16));
                if (g_vgpu.notify_off_multiplier == 0) g_vgpu.notify_off_multiplier = 1;
                found_notify = 1;
                break;
            case VIRTIO_PCI_CAP_ISR_CFG:
                found_isr = 1;   // tidak dipakai v1 (polling)
                break;
            case VIRTIO_PCI_CAP_DEVICE_CFG:
                g_vgpu.device_cfg_phys = bar_phys;
                g_vgpu.device_cfg_off  = bar_off;
                found_dev = 1;
                break;
            default:
                break;
        }
        cap_ptr = cap_next;
    }
    return (found_common && found_notify && found_isr && found_dev) ? 0 : -1;
}

// --- MMIO map via HHDM ---
static void map_mmio(void) {
    if (g_vgpu.common_cfg_phys)
        g_vgpu.common = (volatile virtio_pci_common_cfg_t*)(g_vgpu.common_cfg_phys + hhdm_offset + g_vgpu.common_cfg_off);
    if (g_vgpu.notify_base_phys)
        g_vgpu.notify_base = (volatile uint16_t*)(g_vgpu.notify_base_phys + hhdm_offset + g_vgpu.notify_base_off);
    if (g_vgpu.device_cfg_phys)
        g_vgpu.device_cfg = (volatile virtio_gpu_config_t*)(g_vgpu.device_cfg_phys + hhdm_offset + g_vgpu.device_cfg_off);
}

// --- Feature negotiation ---
static int negotiate_features(void) {
    volatile virtio_pci_common_cfg_t* c = g_vgpu.common;
    if (c == NULL) return -1;

    // Reset.
    c->device_status = 0;
    __asm__ volatile("" ::: "memory");

    // ACKNOWLEDGE
    c->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __asm__ volatile("" ::: "memory");
    // DRIVER
    c->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __asm__ volatile("" ::: "memory");

    // Baca device features (window 0 dan 1).
    uint64_t dev_features = 0;
    c->device_feature_select = 0;
    __asm__ volatile("" ::: "memory");
    dev_features |= (uint64_t)c->device_feature;
    c->device_feature_select = 1;
    __asm__ volatile("" ::: "memory");
    dev_features |= ((uint64_t)c->device_feature) << 32;

    // Pilih fitur yang kita dukung: 2D only (tidak VIRGL, tidak BLOB).
    // EDID opsional. Guest features low 32-bit.
    uint32_t guest_features = 0;
    // (tidak ada yang wajib untuk 2D dasar)

    c->guest_feature_select = 0;
    __asm__ volatile("" ::: "memory");
    c->guest_feature = guest_features;
    __asm__ volatile("" ::: "memory");
    c->guest_feature_select = 1;
    __asm__ volatile("" ::: "memory");
    c->guest_feature = 0;
    __asm__ volatile("" ::: "memory");

    // FEATURES_OK
    c->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK;
    __asm__ volatile("" ::: "memory");

    // Re-read status — verifikasi FEATURES_OK masih ter-set.
    uint8_t st = c->device_status;
    if (!(st & VIRTIO_STATUS_FEATURES_OK)) {
        c->device_status = 0;   // reset
        return -1;
    }

    (void)dev_features;   // dev_features dipakai untuk log/debug opsional
    return 0;
}

// --- Setup kedua virtqueue ---
static int setup_queues(void) {
    int r = virtq_init(&g_vgpu.controlq, 0, 16, g_vgpu.common, g_vgpu.notify_base,
                       g_vgpu.notify_off_multiplier);
    if (r != 0) return -1;
    r = virtq_init(&g_vgpu.cursorq, 1, 16, g_vgpu.common, g_vgpu.notify_base,
                   g_vgpu.notify_off_multiplier);
    if (r != 0) return -1;
    return 0;
}

// --- DRIVER_OK ---
static int finalize_driver_ok(void) {
    g_vgpu.common->device_status =
        VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER |
        VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK;
    __asm__ volatile("" ::: "memory");
    // Verifikasi device tidak set NEEDS_RESET/FAILED.
    uint8_t st = g_vgpu.common->device_status;
    if (st & VIRTIO_STATUS_DEVICE_NEEDS_RESET) return -1;
    return 0;
}

int virtio_gpu_dev_probe(void) {
    if (g_vgpu.initialized) return 0;
    memset(&g_vgpu, 0, sizeof(g_vgpu));

    // 2A.1
    if (!pci_find_virtio_gpu()) {
        serial_log("[vgpu] device not found\n");
        return -1;
    }
    if (parse_capabilities() != 0) {
        serial_log("[vgpu] required capabilities missing\n");
        return -1;
    }

    // 2A.2 MMIO map
    map_mmio();
    if (g_vgpu.common == NULL || g_vgpu.notify_base == NULL || g_vgpu.device_cfg == NULL) {
        serial_log("[vgpu] MMIO map failed\n");
        return -1;
    }

    // 2A.3 negotiation
    if (negotiate_features() != 0) {
        serial_log("[vgpu] feature negotiation failed\n");
        return -1;
    }

    // 2A.4 virtqueue
    if (setup_queues() != 0) {
        serial_log("[vgpu] virtqueue setup failed\n");
        return -1;
    }

    // 2A.3 DRIVER_OK
    if (finalize_driver_ok() != 0) {
        serial_log("[vgpu] DRIVER_OK rejected\n");
        return -1;
    }

    g_vgpu.num_scanouts = g_vgpu.device_cfg ? g_vgpu.device_cfg->num_scanouts : 1;
    g_vgpu.initialized = 1;
    g_vgpu.negotiation_done = 1;
    serial_log("[vgpu] device ready\n");
    return 0;
}

static uint32_t g_resource_counter = 1;   // 0 reserved

uint32_t virtio_gpu_next_resource_id(void) {
    return g_resource_counter++;
}

// --- Kirim command: bangun descriptor chain (cmd buffer, resp buffer),
// submit, notify, wait. Command & response live di halaman physical agar
// device (DMA) bisa akses. ---
int virtio_gpu_dev_command(const void* cmd, uint32_t cmd_len,
                           void* out, uint32_t out_len) {
    if (!g_vgpu.initialized) return -1;
    if (cmd_len == 0) return -1;

    // Command buffer mungkin lebih besar dari satu halaman (mis. ATTACH_BACKING
    // dengan banyak entries). Alokasikan cukup halaman & kirim sebagai chain.
    uint32_t cmd_pages = (cmd_len + 4095) / 4096;
    gpu_page_t cmd_pg[16];
    if (cmd_pages > 16) return -1;   // batas aman: 16 halaman command
    for (uint32_t i = 0; i < cmd_pages; i++) {
        if (gpu_alloc_page(&cmd_pg[i]) != 0) {
            gpu_free_pages(cmd_pg, i);
            return -1;
        }
    }
    memcpy(cmd_pg[0].virt, cmd, cmd_len);

    gpu_page_t resp_pg = {0,0};
    if (out && out_len) {
        if (gpu_alloc_page(&resp_pg) != 0) { gpu_free_pages(cmd_pg, cmd_pages); return -1; }
        memset(resp_pg.virt, 0, 4096);
    }

    // Bangun descriptor chain: tiap halaman command + 1 halaman response.
    uint64_t addrs[17];
    uint32_t lens[17];
    uint16_t flags[17];
    int nb = 0;
    for (uint32_t i = 0; i < cmd_pages; i++) {
        addrs[nb] = cmd_pg[i].phys;
        lens[nb]  = (i == cmd_pages - 1) ? (cmd_len - i * 4096) : 4096;
        flags[nb] = 0;
        nb++;
    }
    if (out && out_len) {
        addrs[nb] = resp_pg.phys;
        lens[nb]  = out_len;
        flags[nb] = VIRTQ_DESC_F_WRITE;
        nb++;
    }

    int head = virtq_submit(&g_vgpu.controlq, addrs, lens, flags, nb);
    if (head < 0) {
        gpu_free_pages(cmd_pg, cmd_pages); if (resp_pg.phys) gpu_free_pages(&resp_pg, 1); return -1;
    }
    virtq_notify(&g_vgpu.controlq);
    uint32_t written = 0;
    int r = virtq_wait(&g_vgpu.controlq, head, &written);
    if (r != 0) {
        gpu_free_pages(cmd_pg, cmd_pages);
        if (resp_pg.phys) gpu_free_pages(&resp_pg, 1);
        return -1;
    }

    if (out && out_len && resp_pg.phys) {
        uint32_t copy = written < out_len ? written : out_len;
        memcpy(out, resp_pg.virt, copy);
    }

    gpu_free_pages(cmd_pg, cmd_pages);
    if (resp_pg.phys) gpu_free_pages(&resp_pg, 1);
    return 0;
}
