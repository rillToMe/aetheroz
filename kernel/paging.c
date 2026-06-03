#include "paging.h"

uint32_t page_directory[1024] __attribute__((aligned(4096)));
uint32_t first_page_table[1024] __attribute__((aligned(4096)));

// TAMBAHAN: Tabel memori kedua untuk area 4MB - 8MB
uint32_t second_page_table[1024] __attribute__((aligned(4096))); 

void init_paging(void) {
    // 1. Kosongkan semua (Not Present)
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 2; 
    }

    // 2. Tabel 1: Mapping 0MB sampai 4MB (Untuk Kernel, VGA, IDT)
    for (int i = 0; i < 1024; i++) {
        first_page_table[i] = (i * 4096) | 3;
    }

    // 3. Tabel 2: Mapping 4MB sampai 8MB (Tempat Heap kita dialokasikan oleh PMM!)
    for (int i = 0; i < 1024; i++) {
        // i + 1024 memastikan alamat fisik dilanjutkan dari 4MB
        second_page_table[i] = ((i + 1024) * 4096) | 3;
    }

    // 4. Masukkan kedua tabel ke direktori utama
    page_directory[0] = ((uint32_t)first_page_table) | 3;
    page_directory[1] = ((uint32_t)second_page_table) | 3; // Daftarkan tabel kedua

    // 5. Aktifkan Paging
    __asm__ volatile("mov %0, %%cr3" :: "r"(page_directory));
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
}