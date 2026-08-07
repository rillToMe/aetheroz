#ifndef VIRTIO_GPU_DEV_H
#define VIRTIO_GPU_DEV_H

// ============================================================
// VirtIO-GPU Driver Core (drivers/graphics/hw/virtio_gpu_dev.h)
//
// Satu-satunya tempat yang boleh menyentuh MMIO/PCI VirtIO-GPU.
// Tugas: PCI bring-up, capability parsing, MMIO map, device status FSM,
// feature negotiation, virtqueue setup, dan eksekusi command ke device.
// TIDAK tahu konsep "surface"/"compositor" — hanya transport + command.
// ============================================================

#include <stdint.h>
#include "virtio_gpu_regs.h"
#include "virtqueue.h"

typedef struct {
    // PCI identity
    uint16_t bus, slot, func;

    // Capability physical locations (BAR phys + offset), di-map via HHDM.
    uint64_t common_cfg_phys;      // base BAR phys untuk COMMON_CFG
    uint32_t common_cfg_off;
    uint64_t notify_base_phys;     // base BAR phys untuk NOTIFY_CFG
    uint32_t notify_base_off;
    uint32_t notify_off_multiplier;
    uint64_t device_cfg_phys;
    uint32_t device_cfg_off;

    // MMIO virtual (HHDM translated)
    volatile virtio_pci_common_cfg_t* common;
    volatile uint16_t*  notify_base;
    volatile virtio_gpu_config_t* device_cfg;

    // Virtqueues
    virtq_t controlq;
    virtq_t cursorq;

    // Display info (dari GET_DISPLAY_INFO)
    uint32_t scanout_width;
    uint32_t scanout_height;
    uint32_t num_scanouts;

    // State
    int      initialized;
    int      negotiation_done;
} virtio_gpu_dev_t;

// Instance device tunggal (KyuzenOS tidak mendukung multi-GPU v1).
extern virtio_gpu_dev_t g_vgpu;

// Probe PCI & init device sampai DRIVER_OK. Return 0 sukses, <0 gagal.
// Dipanggil dari virtio_gpu_backend_ops.init().
int virtio_gpu_dev_probe(void);

// Kirim satu command virtio-gpu (buffer sudah berisi ctrl_hdr + payload).
// `out` = buffer response (harus ada ruang). Return 0 sukses, <0 error.
int virtio_gpu_dev_command(const void* cmd, uint32_t cmd_len,
                           void* out, uint32_t out_len);

// Helper: alokasi resource_id monotonic (tidak pernah di-reuse, §6.3).
uint32_t virtio_gpu_next_resource_id(void);

#endif // VIRTIO_GPU_DEV_H
