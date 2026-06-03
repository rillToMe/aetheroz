#include "heap.h"
#include "string.h"

// KITA SIAPKAN KOLAM MEMORI 1 MEGABYTE! (1024 * 1024 Bytes)
#define HEAP_SIZE (1024 * 1024)
uint8_t heap_memory[HEAP_SIZE]; 

heap_block_t* heap_head = NULL; // Kepala rantai memori

// 1. Inisialisasi: Sulap array kosong menjadi satu blok raksasa yang bebas
void init_heap(void) {
    heap_head = (heap_block_t*)heap_memory;
    heap_head->size = HEAP_SIZE - sizeof(heap_block_t); // Sisa ruang setelah dikurangi label metadata
    heap_head->is_free = 1;
    heap_head->next = NULL;
}

// 2. Kmalloc: Cari blok kosong, potong, dan berikan ke aplikasi
void* kmalloc(uint32_t size) {
    if (size == 0) return NULL;

    heap_block_t* current = heap_head;
    
    // Susuri rantai memori mencari yang pas
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            
            // Jika kotaknya kebesaran, kita POTONG dan sisanya jadikan kotak kosong baru! (Splitting)
            if (current->size > size + sizeof(heap_block_t) + 1) {
                heap_block_t* new_block = (heap_block_t*)((uint8_t*)current + sizeof(heap_block_t) + size);
                new_block->is_free = 1;
                new_block->size = current->size - size - sizeof(heap_block_t);
                new_block->next = current->next;
                
                current->next = new_block;
                current->size = size;
            }
            
            // Tandai terpakai
            current->is_free = 0;
            
            // Kembalikan alamat memori ASLI (setelah dilompati label metadatanya)
            return (void*)((uint8_t*)current + sizeof(heap_block_t));
        }
        current = current->next;
    }
    
    return NULL; // Memori 1 MB beneran habis!
}

// 3. Kfree: Kembalikan memori dan daur ulang!
void kfree(void* ptr) {
    if (ptr == NULL) return;
    
    // Mundur sedikit untuk membaca label metadatanya
    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));
    block->is_free = 1; // Bebaskan!

    // COALESCING: Peleburan Blok
    // Kita susuri dari awal, kalau ada dua blok KOSONG yang bersebelahan, kita HANCURKAN sekatnya!
    heap_block_t* current = heap_head;
    while (current != NULL) {
        if (current->is_free && current->next != NULL && current->next->is_free) {
            // Gabungkan ukurannya
            current->size += current->next->size + sizeof(heap_block_t);
            // Lompatkan rantainya (Blok yang di tengah hilang dilebur)
            current->next = current->next->next; 
        } else {
            // Hanya maju jika tidak ada peleburan, agar bisa mengecek blok berikutnya lagi
            current = current->next; 
        }
    }
}

void* krealloc(void* ptr, uint32_t old_size, uint32_t new_size) {
    // Kalau minta ukuran 0, sama saja dengan menghapus memori
    if (new_size == 0) { 
        kfree(ptr); 
        return NULL; 
    }
    
    // Kalau pointer sebelumnya kosong, sama saja dengan kmalloc baru
    if (ptr == NULL) {
        return kmalloc(new_size);
    }

    // 1. Sewa "rumah" baru yang lebih besar
    void* new_ptr = kmalloc(new_size);
    if (new_ptr == NULL) {
        return NULL; // Gagal, RAM beneran habis
    }

    // Bersihkan rumah baru agar terhindar dari sampah memori
    memset(new_ptr, 0, new_size);

    // 2. Pindahkan data dari "rumah" lama ke "rumah" baru
    memcpy(new_ptr, ptr, old_size);

    // 3. Jual/Bebaskan "rumah" lama agar bisa didaur ulang OS
    kfree(ptr);

    return new_ptr;
}