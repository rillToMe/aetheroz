// ============================================================
// Generic split virtqueue (drivers/graphics/hw/virtqueue.c)
//
// v1 model: synchronous (submit → notify → wait), single-context.
// Karena submit selalu diikuti wait sebelum submit berikutnya, hanya ada
// SATU chain aktif pada satu waktu. Freelist dipelihara sederhana:
// free_head menunjuk descriptor kosong berikutnya; num_free = sisa.
// ============================================================

#include "virtqueue.h"
#include "gpu_alloc.h"
#include <stddef.h>

int virtq_init(virtq_t* vq, uint16_t queue_index, uint16_t queue_size,
               volatile virtio_pci_common_cfg_t* common,
               volatile uint16_t* notify_base, uint32_t notify_off_multiplier) {
    if (vq == NULL || common == NULL || queue_size == 0) return -1;

    // Select queue & baca ukuran max dari device.
    common->queue_select = queue_index;
    __asm__ volatile("" ::: "memory");
    uint16_t dev_size = common->queue_size;
    if (dev_size == 0) return -1;            // queue tidak ada
    if (queue_size > dev_size) queue_size = dev_size;

    gpu_page_t desc_pg, avail_pg, used_pg;
    if (gpu_alloc_page(&desc_pg) != 0) return -1;
    if (gpu_alloc_page(&avail_pg) != 0) { gpu_free_pages(&desc_pg, 1); return -1; }
    if (gpu_alloc_page(&used_pg) != 0)  { gpu_free_pages(&desc_pg, 1); gpu_free_pages(&avail_pg, 1); return -1; }

    vq->queue_index  = queue_index;
    vq->queue_size   = queue_size;
    vq->desc         = (virtq_desc_t*)desc_pg.virt;
    vq->avail        = (virtq_avail_t*)avail_pg.virt;
    vq->used         = (virtq_used_t*)used_pg.virt;
    vq->desc_phys    = desc_pg.phys;
    vq->avail_phys   = avail_pg.phys;
    vq->used_phys    = used_pg.phys;
    vq->free_head    = 0;
    vq->num_free     = queue_size;
    vq->last_used_idx = 0;

    // Bangun freelist: desc[i].next = i+1.
    for (int i = 0; i < queue_size; i++) {
        vq->desc[i].addr  = 0;
        vq->desc[i].len   = 0;
        vq->desc[i].flags = 0;
        vq->desc[i].next  = (uint16_t)((i + 1) % queue_size);
    }

    vq->avail->flags = 0;
    vq->avail->idx   = 0;
    vq->used->flags  = 0;
    vq->used->idx    = 0;

    // Tulis physical addr ke common_cfg (64-bit write — aligned di x86-64).
    common->queue_desc  = vq->desc_phys;
    common->queue_driver = vq->avail_phys;
    common->queue_device = vq->used_phys;
    __asm__ volatile("" ::: "memory");

    common->queue_enable = 1;
    __asm__ volatile("" ::: "memory");

    uint16_t notify_off = common->queue_notify_off;
    vq->notify_addr = notify_base + (uint32_t)notify_off * notify_off_multiplier;

    return 0;
}

// Submit chain sepanjang n_bufs, mengambil descriptor berurutan dari
// freelist (head, head+1, ..., head+n-1). Return head index atau -1.
int virtq_submit(virtq_t* vq, uint64_t* addrs, uint32_t* lens,
                 uint16_t* flags, int n_bufs) {
    if (vq == NULL || n_bufs <= 0) return -1;
    if ((uint16_t)n_bufs > vq->num_free) return -1;

    uint16_t head = vq->free_head;
    for (int i = 0; i < n_bufs; i++) {
        uint16_t idx = (uint16_t)((head + i) % vq->queue_size);
        vq->desc[idx].addr  = addrs[i];
        vq->desc[idx].len   = lens[i];
        vq->desc[idx].flags = flags[i];
        vq->desc[idx].next  = (uint16_t)((idx + 1) % vq->queue_size);
        if (i < n_bufs - 1) vq->desc[idx].flags |= VIRTQ_DESC_F_NEXT;
    }
    // head+n-1 adalah descriptor terakhir; next-nya tetap berurutan.

    // Update freelist: num_free berkurang; free_head maju n.
    vq->free_head = (uint16_t)((head + n_bufs) % vq->queue_size);
    vq->num_free -= (uint16_t)n_bufs;
    vq->last_count = (uint16_t)n_bufs;

    // Tambahkan head ke avail ring.
    uint16_t slot = (uint16_t)(vq->avail->idx % vq->queue_size);
    vq->avail->ring[slot] = head;
    __asm__ volatile("" ::: "memory");
    vq->avail->idx++;

    return (int)head;
}

void virtq_notify(virtq_t* vq) {
    if (vq == NULL || vq->notify_addr == NULL) return;
    __asm__ volatile("" ::: "memory");
    *vq->notify_addr = vq->queue_index;
}

int virtq_wait(virtq_t* vq, int head, uint32_t* out_written_len) {
    if (vq == NULL || head < 0) return -1;
    uint32_t iter = 0;
    for (;;) {
        uint16_t used_idx = vq->used->idx;
        if (vq->last_used_idx != used_idx) {
            // Proses semua used element baru, cari head kita.
            while (vq->last_used_idx != used_idx) {
                virtq_used_elem_t* ue =
                    &vq->used->ring[vq->last_used_idx % vq->queue_size];
                if (ue->id == (uint32_t)head) {
                    if (out_written_len) *out_written_len = ue->len;
                    vq->last_used_idx++;
                    // Kembalikan descriptor chain (head..head+last_count-1)
                    // ke freelist. v1: submit→wait berpasangan, 1 chain aktif.
                    vq->num_free += vq->last_count;
                    if (vq->num_free > vq->queue_size) vq->num_free = vq->queue_size;
                    vq->free_head = 0;   // ring dipakai ulang dari awal
                    return 0;
                }
                vq->last_used_idx++;
            }
        }
        if (++iter > VIRTQ_POLL_MAX_ITER) return -2;   // -ETIMEDOUT
        __asm__ volatile("pause");
    }
}
