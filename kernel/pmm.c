#include "pmm.h"

// Array penyimpan status RAM (0 = bebas, 1 = dipakai)
static uint8_t pmm_bitmap[PMM_BITMAP_SIZE];

// --- Fungsi Helper Bitwise ---
static inline void bitmap_set(uint32_t bit) {
    pmm_bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void bitmap_clear(uint32_t bit) {
    pmm_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline uint8_t bitmap_test(uint32_t bit) {
    return (pmm_bitmap[bit / 8] & (1 << (bit % 8)));
}

// --- Fungsi Utama PMM ---

void pmm_init(void) {
    // 1. Bersihkan semua bit (Anggap semua RAM kosong)
    for (uint32_t i = 0; i < PMM_BITMAP_SIZE; i++) {
        pmm_bitmap[i] = 0;
    }

    // 2. SANGAT PENTING: Lindungi memori tempat Kernel kita sendiri berada!
    // Kalau tidak, OS akan menimpa tubuhnya sendiri dan langsung crash.
    // Kita kunci 4 Megabyte pertama (0x0 sampai 0x400000).
    // Ini juga melindungi memori VGA dan IDT/GDT kita.
    uint32_t pages_to_lock = 0x400000 / PAGE_SIZE;
    for (uint32_t i = 0; i < pages_to_lock; i++) {
        bitmap_set(i);
    }
}

// Fungsi untuk meminta 1 blok RAM (4KB)
void* pmm_alloc_page(void) {
    // Cari bit pertama yang bernilai 0
    for (uint32_t i = 0; i < PMM_BITMAP_SIZE * 8; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i); // Tandai sebagai dipakai
            return (void*)(i * PAGE_SIZE); // Kembalikan alamat fisiknya
        }
    }
    return 0; // Kembalikan null jika RAM habis (Kernel Panic!)
}

// Fungsi untuk mengembalikan/melepas RAM
void pmm_free_page(void* ptr) {
    uint32_t addr = (uint32_t)ptr;
    uint32_t bit = addr / PAGE_SIZE;
    bitmap_clear(bit); // Tandai sebagai kosong lagi
}

// Hitung berapa banyak blok RAM (Page) yang bernilai 1 di Bitmap
uint32_t pmm_get_used_ram(void) {
    uint32_t used_pages = 0;
    for (uint32_t i = 0; i < PMM_BITMAP_SIZE * 8; i++) {
        if (bitmap_test(i)) {
            used_pages++;
        }
    }
    return used_pages * PAGE_SIZE; // Kembalikan dalam satuan Byte
}

uint32_t real_total_ram = 256 * 1024 * 1024; // Default 256 MB

void pmm_set_total_ram(uint32_t size) {
    real_total_ram = size;
}

// Timpa pmm_get_total_ram yang sebelumnya dengan ini:
uint32_t pmm_get_total_ram(void) {
    return real_total_ram;
}