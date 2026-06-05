#include "paging.h"

// ===================================================================
// PAGE DIRECTORY & TABEL STATIS (Pemetaan 64 MB Pertama!)
// ===================================================================
uint32_t page_directory[1024] __attribute__((aligned(4096)));

// KITA GANTI 5 VARIABEL MANUAL MENJADI 1 ARRAY 2D RAKSASA!
// 16 Tabel x 4MB = 64 MB Area Aman untuk Kernel & Heap
uint32_t kernel_page_tables[16][1024] __attribute__((aligned(4096))); 

// Tabel khusus Framebuffer GPU
uint32_t fb_page_table[1024] __attribute__((aligned(4096)));

// ===================================================================
// POOL TABEL HALAMAN DINAMIS
// ===================================================================
static uint32_t dynamic_tables[MAX_DYNAMIC_TABLES][1024] __attribute__((aligned(4096)));
static uint8_t  dynamic_table_used[MAX_DYNAMIC_TABLES];   // 0 = kosong, 1 = terpakai

// ===================================================================
// INISIALISASI PAGING (Dipanggil sekali saat boot)
// ===================================================================
void init_paging(uint32_t fb_phys_addr) {
    // Tandai semua entri sebagai Not Present dulu
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 2; // Bit 1 = R/W, Bit 0 = Not Present
    }

    // Inisialisasi pool dinamis
    for (int i = 0; i < MAX_DYNAMIC_TABLES; i++) {
        dynamic_table_used[i] = 0;
    }

    // =========================================================
    // PETAKAN 64 MB PERTAMA RAM SECARA OTOMATIS!
    // =========================================================
    for (int t = 0; t < 16; t++) {
        for (int i = 0; i < 1024; i++) {
            // (t * 0x400000) adalah offset per 4MB. Flag | 7 (User, R/W, Present)
            kernel_page_tables[t][i] = ((t * 0x400000) + (i * 4096)) | 7;
        }
        page_directory[t] = ((uint32_t)kernel_page_tables[t]) | 7;
    }

    // --- PEMETAAN FRAMEBUFFER VRAM ---
    if (fb_phys_addr != 0) {
        uint32_t fb_dir_index = fb_phys_addr >> 22;
        uint32_t fb_base      = fb_phys_addr & 0xFFC00000;

        for (int i = 0; i < 1024; i++) {
            fb_page_table[i] = (fb_base + (i * 4096)) | 7;
        }
        page_directory[fb_dir_index] = ((uint32_t)fb_page_table) | 7;
    }
    // ---------------------------------

    // Aktifkan paging
    __asm__ volatile("mov %0, %%cr3" :: "r"(page_directory));
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
}

// ===================================================================
// paging_map_region() — Petakan blok 4MB yang mencakup 'vaddr'
//
// Cara kerja:
//   1. Hitung indeks Page Directory dari alamat virtual (bit 31-22)
//   2. Jika sudah dipetakan (Present bit = 1), return langsung (idempoten)
//   3. Ambil slot kosong dari pool dynamic_tables[]
//   4. Isi 1024 entri dengan identity mapping untuk blok 4MB tersebut
//   5. Daftarkan ke page_directory[] dan flush TLB via CR3 reload
// ===================================================================
int paging_map_region(uint32_t vaddr) {
    uint32_t dir_index = vaddr >> 22; // Bit 31-22 = indeks Page Directory

    // Sudah dipetakan? Return langsung (idempoten)
    if (page_directory[dir_index] & 1) return 1;

    // Cari slot kosong dari pool
    for (int i = 0; i < MAX_DYNAMIC_TABLES; i++) {
        if (!dynamic_table_used[i]) {
            dynamic_table_used[i] = 1;

            // Hitung alamat fisik base untuk blok 4MB ini
            uint32_t base = dir_index << 22; // dir_index * 0x400000

            // Isi 1024 page entries (masing-masing 4KB = 4MB total)
            // Flag | 7: Present | R/W | User-accessible
            for (int j = 0; j < 1024; j++) {
                dynamic_tables[i][j] = (base + (uint32_t)(j * 4096)) | 7;
            }

            // Daftarkan ke Page Directory
            page_directory[dir_index] = ((uint32_t)dynamic_tables[i]) | 7;

            // Flush TLB: Reload CR3 agar CPU membaca pemetaan baru
            __asm__ volatile("mov %0, %%cr3" :: "r"(page_directory) : "memory");

            return 1; // Sukses
        }
    }

    return 0; // Pool habis (8 tabel x 4MB = 32MB sudah terpakai)
}

// ===================================================================
// paging_is_mapped() — Cek apakah alamat sudah dipetakan
// Mengembalikan 1 jika Present bit aktif, 0 jika belum dipetakan
// ===================================================================
int paging_is_mapped(uint32_t addr) {
    uint32_t dir_index = addr >> 22;
    return (page_directory[dir_index] & 1) ? 1 : 0;
}
