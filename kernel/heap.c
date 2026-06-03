#include "heap.h"
#include "pmm.h"

// Metadata untuk setiap potongan memori
typedef struct heap_block {
    size_t size;            // Ukuran potongan ini
    uint8_t is_free;        // 1 = Bebas, 0 = Terpakai
    struct heap_block* next; // Pointer ke potongan memori berikutnya
} heap_block_t;

// Kepala dari Linked List memori kita
static heap_block_t* heap_head = NULL;

void init_heap(void) {
    // Sewa 1 Page (4KB) dari PMM sebagai modal awal Heap kita
    heap_head = (heap_block_t*)pmm_alloc_page();
    
    // Potongan pertama ini ukurannya 4KB dikurangi ukuran metadatanya sendiri
    heap_head->size = PAGE_SIZE - sizeof(heap_block_t);
    heap_head->is_free = 1;
    heap_head->next = NULL;
}

// Fungsi pembungkus sakti kita!
void* kmalloc(size_t size) {
    heap_block_t* current = heap_head;
    
    while (current != NULL) {
        // Cari blok yang statusnya BEBAS dan ukurannya CUKUP
        if (current->is_free && current->size >= size) {
            
            // Kalau blok ini kebesaran, kita "belah" jadi dua (Split)
            // Syarat split: sisa ukurannya harus muat untuk nampung metadata baru + minimal 4 byte data
            if (current->size > size + sizeof(heap_block_t) + 4) {
                // Hitung alamat untuk blok baru (berada tepat setelah blok yg diminta user)
                heap_block_t* new_block = (heap_block_t*)((uint8_t*)current + sizeof(heap_block_t) + size);
                
                new_block->size = current->size - size - sizeof(heap_block_t);
                new_block->is_free = 1;
                new_block->next = current->next;
                
                current->size = size;
                current->next = new_block;
            }
            
            current->is_free = 0; // Tandai sedang dipakai
            
            // Kembalikan alamat ruang kosongnya (bukan alamat metadatanya)
            return (void*)((uint8_t*)current + sizeof(heap_block_t));
        }
        current = current->next;
    }
    
    // Kalau sampai sini berarti RAM Heap kita habis (Out of Memory)
    // Nanti kita bisa bikin logika agar dia otomatis minta Page baru ke PMM.
    return NULL; 
}

// Fungsi untuk mengembalikan memori
void kfree(void* ptr) {
    if (!ptr) return;
    
    // Mundur beberapa byte dari alamat yang dikasih user untuk menemukan metadatanya
    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));
    block->is_free = 1; // Bebaskan!
    
    // (Di OS canggih, biasanya ada fungsi tambahan di sini untuk menggabungkan 
    // dua blok bebas yang bersebelahan agar tidak terjadi fragmentasi/remukan memori).
}