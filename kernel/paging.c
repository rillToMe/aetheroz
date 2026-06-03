#include "paging.h"

uint32_t page_directory[1024] __attribute__((aligned(4096)));
uint32_t first_page_table[1024] __attribute__((aligned(4096)));
uint32_t second_page_table[1024] __attribute__((aligned(4096))); 

void init_paging(void) {
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 2; 
    }

    // UBAH FLAG KE 7: Mengizinkan aplikasi Ring 3 mengakses RAM
    for (int i = 0; i < 1024; i++) {
        first_page_table[i] = (i * 4096) | 7;
    }

    for (int i = 0; i < 1024; i++) {
        second_page_table[i] = ((i + 1024) * 4096) | 7;
    }

    // Direktori utamanya juga harus flag 7!
    page_directory[0] = ((uint32_t)first_page_table) | 7;
    page_directory[1] = ((uint32_t)second_page_table) | 7; 

    __asm__ volatile("mov %0, %%cr3" :: "r"(page_directory));
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
}