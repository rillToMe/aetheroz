#include "paging.h"

uint32_t page_directory[1024] __attribute__((aligned(4096)));
uint32_t first_page_table[1024] __attribute__((aligned(4096)));
uint32_t second_page_table[1024] __attribute__((aligned(4096))); 
uint32_t third_page_table[1024] __attribute__((aligned(4096)));

// TABEL BARU: Khusus untuk menampung memori Framebuffer GPU!
uint32_t fb_page_table[1024] __attribute__((aligned(4096)));

void init_paging(uint32_t fb_phys_addr) {
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 2; 
    }

    for (int i = 0; i < 1024; i++) first_page_table[i] = (i * 4096) | 7;         // 0 - 4MB (Kernel)
    for (int i = 0; i < 1024; i++) second_page_table[i] = ((i + 1024) * 4096) | 7; // 4 - 8MB (Heap)
    for (int i = 0; i < 1024; i++) third_page_table[i] = ((i + 2048) * 4096) | 7;  // 8 - 12MB (Aplikasi)

    page_directory[0] = ((uint32_t)first_page_table) | 7;
    page_directory[1] = ((uint32_t)second_page_table) | 7; 
    page_directory[2] = ((uint32_t)third_page_table) | 7; 

    // --- PEMETAAN FRAMEBUFFER VRAM ---
    if (fb_phys_addr != 0) {
        // 1. Hitung Indeks Direktori (Blok 4MB ke berapa VRAM ini berada?)
        uint32_t fb_dir_index = fb_phys_addr >> 22;
        
        // 2. Bulatkan alamat ke kelipatan 4MB ke bawah agar sejajar (Aligned)
        uint32_t fb_base = fb_phys_addr & 0xFFC00000; 
        
        // 3. Daftarkan 4MB penuh (1024 entri * 4KB) mulai dari alamat base tersebut
        for (int i = 0; i < 1024; i++) {
            fb_page_table[i] = (fb_base + (i * 4096)) | 7;
        }
        
        // 4. Masukkan ke Buku Peta Utama (Page Directory)
        page_directory[fb_dir_index] = ((uint32_t)fb_page_table) | 7;
    }
    // ---------------------------------

    __asm__ volatile("mov %0, %%cr3" :: "r"(page_directory));
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
}