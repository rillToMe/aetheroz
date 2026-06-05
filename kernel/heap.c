#include "heap.h"
#include "string.h"
#include "paging.h" // Untuk memanggil vmm_alloc_page()

// KITA TENTUKAN ZONA HEAP VIRTUAL (Mulai dari 256 MB di angkasa!)
// Zona ini terisolasi dan tidak akan tertabrak oleh Kernel (0-64MB) atau User Apps
#define HEAP_START_VADDR 0x10000000 
static uint32_t current_heap_end = HEAP_START_VADDR;

heap_block_t* heap_head = NULL; // Kepala rantai memori

// 1. Inisialisasi: Sekarang sangat bersih, tidak memakan RAM sama sekali!
void init_heap(void) {
    heap_head = NULL; 
}

// ===================================================================
// FUNGSI INTI: MEMPERLUAS HEAP SECARA DINAMIS MENGGUNAKAN VMM!
// ===================================================================
static heap_block_t* expand_heap(uint32_t required_size) {
    // Hitung total kebutuhan (data + label metadata)
    uint32_t total_size = required_size + sizeof(heap_block_t);
    
    // Berapa blok 4KB (Page) yang kita butuhkan?
    uint32_t pages_needed = total_size / 4096;
    if (total_size % 4096 != 0) pages_needed++; // Bulatkan ke atas

    uint32_t start_expansion_addr = current_heap_end;

    // Panggil VMM untuk menurunkan RAM fisik dari langit ke virtual address!
    for (uint32_t i = 0; i < pages_needed; i++) {
        // UBAH KE FLAG 7: Present (1) | R/W (2) | User (4). Agar aplikasi GUI Ring 3 bisa pakai Heap!
        if (!vmm_alloc_page(current_heap_end, 7)) {
            return NULL; // Fatal Error: RAM fisik komputer beneran habis
        }
        current_heap_end += 4096; // Geser batas ujung heap
    }

    // Jadikan halaman memori yang baru turun ini sebagai satu blok bebas raksasa
    heap_block_t* new_block = (heap_block_t*)start_expansion_addr;
    new_block->size = (pages_needed * 4096) - sizeof(heap_block_t);
    new_block->is_free = 1;
    new_block->next = NULL;

    // Sambungkan blok baru ini ke ujung linked list kita
    if (heap_head == NULL) {
        heap_head = new_block;
    } else {
        heap_block_t* curr = heap_head;
        while (curr->next != NULL) curr = curr->next;
        curr->next = new_block;
    }

    return new_block;
}

// 2. Kmalloc: Cari blok kosong, jika habis? Minta VMM untuk ekspansi!
void* kmalloc(uint32_t size) {
    if (size == 0) return NULL;

    heap_block_t* current = heap_head;
    
    // Tahap 1: Susuri rantai memori mencari kotak lama yang pas (First-Fit)
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            
            // Splitting: Potong blok jika kebesaran
            if (current->size > size + sizeof(heap_block_t) + 1) {
                heap_block_t* new_block = (heap_block_t*)((uint8_t*)current + sizeof(heap_block_t) + size);
                new_block->is_free = 1;
                new_block->size = current->size - size - sizeof(heap_block_t);
                new_block->next = current->next;
                
                current->next = new_block;
                current->size = size;
            }
            
            current->is_free = 0; // Tandai terpakai
            return (void*)((uint8_t*)current + sizeof(heap_block_t));
        }
        current = current->next;
    }
    
    // Tahap 2: GAGAL MENEMUKAN KOTAK! Saatnya EKSPANSI HEAP!
    heap_block_t* fresh_block = expand_heap(size);
    if (fresh_block == NULL) return NULL; // Panik: RAM fisik habis
    
    // Panggil ulang kmalloc. Kali ini pasti berhasil karena kita baru memetakan RAM baru.
    return kmalloc(size); 
}

// 3. Kfree: Bebaskan dan Leburkan (Coalesce)
void kfree(void* ptr) {
    if (ptr == NULL) return;
    
    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));
    block->is_free = 1; 

    // COALESCING: Leburkan blok-blok kosong yang bersebelahan
    heap_block_t* current = heap_head;
    while (current != NULL) {
        if (current->is_free && current->next != NULL && current->next->is_free) {
            current->size += current->next->size + sizeof(heap_block_t);
            current->next = current->next->next; 
        } else {
            current = current->next; 
        }
    }
}

// 4. Krealloc: Ubah ukuran memori
void* krealloc(void* ptr, uint32_t old_size, uint32_t new_size) {
    if (new_size == 0) { kfree(ptr); return NULL; }
    if (ptr == NULL) return kmalloc(new_size);

    void* new_ptr = kmalloc(new_size);
    if (new_ptr == NULL) return NULL;

    // Salin seukuran yang paling kecil antara lama dan baru agar aman
    memcpy(new_ptr, ptr, (old_size < new_size) ? old_size : new_size);
    kfree(ptr);
    return new_ptr;
}