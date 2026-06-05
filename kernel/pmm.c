#include "pmm.h"
#include "limine.h"

// Array penyimpan status RAM (0 = bebas, 1 = dipakai)
static uint8_t pmm_bitmap[PMM_BITMAP_SIZE];

// ---> PINDAHKAN VARIABEL INI KE ATAS SINI <---
uint32_t real_total_ram = 256 * 1024 * 1024;

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

void pmm_init_dynamic(void* memmap_entries, uint64_t entry_count) {
    // 1. KUNCI SEMUA RAM (Tingkat Keamanan Maksimal)
    for (uint32_t i = 0; i < PMM_BITMAP_SIZE; i++) {
        pmm_bitmap[i] = 0xFF;
    }

    struct limine_memmap_entry **entries = (struct limine_memmap_entry **)memmap_entries;
    uint64_t highest_addr = 0;

    // 2. BACA PETA LIMINE (Bebaskan bit hanya untuk RAM yang USABLE)
    for (uint64_t i = 0; i < entry_count; i++) {
        struct limine_memmap_entry *entry = entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            uint64_t start = entry->base;
            uint64_t end = start + entry->length;

            if (end > highest_addr) highest_addr = end;

            // Batasi mentok 4GB untuk OS 32-bit
            if (start >= 0x100000000ULL) continue;
            if (end > 0x100000000ULL) end = 0x100000000ULL;

            for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
                bitmap_clear((uint32_t)(addr / PAGE_SIZE));
            }
        }
    }

    // 3. KUNCI KEMBALI 72 MB PERTAMA
    uint32_t pages_to_lock = 0x4800000 / PAGE_SIZE;
    for (uint32_t i = 0; i < pages_to_lock; i++) {
        bitmap_set(i);
    }

    // Limine membuat kita tidak perlu lagi menebak total RAM!
    real_total_ram = (uint32_t)highest_addr;
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

void pmm_set_total_ram(uint32_t size) {
    real_total_ram = size;
}

// Timpa pmm_get_total_ram yang sebelumnya dengan ini:
uint32_t pmm_get_total_ram(void) {
    return real_total_ram;
}