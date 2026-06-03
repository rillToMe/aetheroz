#include "paging.h"

uint32_t page_directory[1024] __attribute__((aligned(4096)));
uint32_t first_page_table[1024] __attribute__((aligned(4096)));
uint32_t second_page_table[1024] __attribute__((aligned(4096))); 
uint32_t third_page_table[1024] __attribute__((aligned(4096)));

void init_paging(void) {
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 2; 
    }

    for (int i = 0; i < 1024; i++) first_page_table[i] = (i * 4096) | 7;         // 0 - 4MB (Kernel)
    for (int i = 0; i < 1024; i++) second_page_table[i] = ((i + 1024) * 4096) | 7; // 4 - 8MB (Heap)
    for (int i = 0; i < 1024; i++) third_page_table[i] = ((i + 2048) * 4096) | 7;  // 8 - 12MB (Zona Aplikasi)

    // Direktori utamanya juga harus flag 7!
    page_directory[0] = ((uint32_t)first_page_table) | 7;
    page_directory[1] = ((uint32_t)second_page_table) | 7; 
    page_directory[2] = ((uint32_t)third_page_table) | 7; 

    __asm__ volatile("mov %0, %%cr3" :: "r"(page_directory));
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
}