#include "paging.h"

// ===================================================================
// PAGE DIRECTORY & TABEL STATIS (Pemetaan 64 MB Pertama!)
// ===================================================================
uint32_t page_directory[1024] __attribute__((aligned(4096)));

// KITA GANTI 5 VARIABEL MANUAL MENJADI 1 ARRAY 2D RAKSASA!
// 16 Tabel x 4MB = 64 MB Area Aman untuk Kernel & Heap
uint32_t kernel_page_tables[32][1024] __attribute__((aligned(4096)));

// Tabel khusus Framebuffer GPU
// Tabel khusus Framebuffer GPU (KITA PERBESAR JADI 4 TABEL = 16 MB!)
uint32_t fb_page_tables[4][1024] __attribute__((aligned(4096)));

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
    for (int t = 0; t < 32; t++) { // <-- UBAH BATAS LOOP JADI 32
        for (int i = 0; i < 1024; i++) {
            kernel_page_tables[t][i] = ((t * 0x400000) + (i * 4096)) | 7;
        }
        page_directory[t] = ((uint32_t)kernel_page_tables[t]) | 7;
    }

    // --- PEMETAAN FRAMEBUFFER VRAM ---
    if (fb_phys_addr != 0) {
        uint32_t fb_dir_index = fb_phys_addr >> 22;
        uint32_t fb_base      = fb_phys_addr & 0xFFC00000;

        // Petakan 4 blok (16 Megabyte) berturut-turut untuk VRAM Monitor!
        for (int t = 0; t < 4; t++) {
            for (int i = 0; i < 1024; i++) {
                fb_page_tables[t][i] = (fb_base + (t * 0x400000) + (i * 4096)) | 7;
            }
            // Daftarkan ke Page Directory secara berurutan
            page_directory[fb_dir_index + t] = ((uint32_t)fb_page_tables[t]) | 7;
        }
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

// Import fungsi PMM
extern void* pmm_alloc_page(void);
extern void* memset(void* s, int c, uint32_t n);

// ===================================================================
// VMM: Mengikat Alamat Virtual ke Alamat Fisik (Physical Frame)
// ===================================================================
void vmm_map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags) {
    uint32_t pd_index = vaddr >> 22;
    uint32_t pt_index = (vaddr >> 12) & 0x03FF;

    if (!(page_directory[pd_index] & 1)) {
        uint32_t* new_pt = (uint32_t*)pmm_alloc_page(); 
        memset(new_pt, 0, 4096);
        page_directory[pd_index] = ((uint32_t)new_pt) | 7; 
        
        // ---> TAMBAHKAN BARIS INI: RELOAD CR3 CACHE! <---
        // Wajib dilakukan agar CPU sadar ada rute Page Table yang baru!
        __asm__ volatile("mov %0, %%cr3" :: "r"(page_directory) : "memory");
    }

    uint32_t* pt = (uint32_t*)(page_directory[pd_index] & 0xFFFFF000);
    pt[pt_index] = (paddr & 0xFFFFF000) | flags;
    __asm__ volatile("invlpg (%0)" ::"r" (vaddr) : "memory");
}
// ===================================================================
// VMM: Alokator Otomatis (Dipanggil oleh Heap!)
// ===================================================================
int vmm_alloc_page(uint32_t vaddr, uint32_t flags) {
    void* paddr = pmm_alloc_page(); // Minta RAM fisik beneran ke hardware
    if (!paddr) return 0;           // RAM fisik habis total!
    
    vmm_map_page(vaddr, (uint32_t)paddr, flags); // Rakit ilusi virtualnya
    return 1;
}