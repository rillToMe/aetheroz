#include "heap.h"
#include "string.h"
#include "paging.h"  // Untuk memanggil vmm_alloc_page()
#include "spinlock.h"
#include <stddef.h>  // size_t

#define HEAP_MAGIC 0xDEADC0DE

extern void kernel_panic(const char* title, const char* desc, uint64_t code);

// Zona Heap Virtual — HARUS canonical (bit47=1, bits63-48=0xFFFF)
// dan di luar HHDM Limine (0xFFFF800000000000 + RAM size, biasanya < 0xFFFF810000000000)
//
// 0xFFFF000000000000 → NON-CANONICAL (bit47=0, bits63-48=0xFFFF) → GPF!
// 0xFFFF900000000000 → CANONICAL (bit47=1) + jauh dari HHDM dan kernel (PML4[288])
#define HEAP_START_VADDR 0xFFFF900000000000ULL

static uint64_t current_heap_end = HEAP_START_VADDR;

heap_block_t* heap_head = NULL;
static spinlock_t heap_lock = SPINLOCK_INIT;

// 1. Inisialisasi
void init_heap(void) {
    heap_head = NULL;
}

// ===================================================================
// FUNGSI INTI: MEMPERLUAS HEAP SECARA DINAMIS MENGGUNAKAN VMM!
// ===================================================================
static heap_block_t* expand_heap(size_t required_size) {
    size_t total_size    = required_size + sizeof(heap_block_t);
    uint64_t pages_needed = total_size / 4096;
    if (total_size % 4096 != 0) pages_needed++;

    uint64_t start_expansion_addr = current_heap_end;

    for (uint64_t i = 0; i < pages_needed; i++) {
        if (!vmm_alloc_page(current_heap_end, 7)) {
            return NULL; // RAM fisik habis
        }
        current_heap_end += 4096;
    }

    // Jadikan halaman baru sebagai blok bebas
    heap_block_t* new_block = (heap_block_t*)start_expansion_addr;
    new_block->magic   = HEAP_MAGIC;
    new_block->size    = (size_t)(pages_needed * 4096) - sizeof(heap_block_t);
    new_block->is_free = 1;
    new_block->next    = NULL;

    if (heap_head == NULL) {
        heap_head = new_block;
    } else {
        heap_block_t* curr = heap_head;
        while (curr->next != NULL) curr = curr->next;
        curr->next = new_block;
    }

    return new_block;
}

// 2. Kmalloc: First-Fit + dynamic expand
void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    uint64_t flags = spinlock_lock_irqsave(&heap_lock);

    heap_block_t* current = heap_head;

    while (current != NULL) {
        if (current->magic != HEAP_MAGIC) {
            kernel_panic("HEAP CORRUPTION",
                         "heap_block_t magic mismatch - header corrupt",
                         (uint64_t)current);
        }
        if (current->is_free && current->size >= size) {
            // Splitting: potong jika blok terlalu besar
            if (current->size > size + sizeof(heap_block_t) + 1) {
                heap_block_t* new_block = (heap_block_t*)((uint8_t*)current + sizeof(heap_block_t) + size);
                new_block->magic   = HEAP_MAGIC;
                new_block->is_free = 1;
                new_block->size    = current->size - size - sizeof(heap_block_t);
                new_block->next    = current->next;

                current->next = new_block;
                current->size = size;
            }
            current->is_free = 0;
            void* result = (void*)((uint8_t*)current + sizeof(heap_block_t));
            spinlock_unlock_irqrestore(&heap_lock, flags);
            return result;
        }
        current = current->next;
    }

    // Tidak ada blok cocok — ekspansi heap
    heap_block_t* fresh_block = expand_heap(size);
    if (fresh_block == NULL) {
        spinlock_unlock_irqrestore(&heap_lock, flags);
        return NULL;
    }
    spinlock_unlock_irqrestore(&heap_lock, flags);
    return kmalloc(size);
}

// 3. Kfree: Bebaskan + Coalesce
void kfree(void* ptr) {
    if (ptr == NULL) return;

    uint64_t flags = spinlock_lock_irqsave(&heap_lock);

    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));
    if (block->magic != HEAP_MAGIC) {
        kernel_panic("HEAP CORRUPTION",
                     "heap_block_t magic mismatch - header corrupt",
                     (uint64_t)block);
    }
    block->is_free = 1;

    // Coalescing: lebur blok kosong yang bersebelahan
    heap_block_t* current = heap_head;
    while (current != NULL) {
        if (current->magic != HEAP_MAGIC) {
            kernel_panic("HEAP CORRUPTION",
                         "heap_block_t magic mismatch - header corrupt",
                         (uint64_t)current);
        }
        if (current->is_free && current->next != NULL && current->next->is_free) {
            current->size += current->next->size + sizeof(heap_block_t);
            current->next  = current->next->next;
        } else {
            current = current->next;
        }
    }

    spinlock_unlock_irqrestore(&heap_lock, flags);
}

// 4. Krealloc: Ubah ukuran
void* krealloc(void* ptr, size_t old_size, size_t new_size) {
    if (new_size == 0) { kfree(ptr); return NULL; }
    if (ptr == NULL)   return kmalloc(new_size);

    void* new_ptr = kmalloc(new_size);
    if (new_ptr == NULL) return NULL;

    memcpy(new_ptr, ptr, (old_size < new_size) ? old_size : new_size);
    kfree(ptr);
    return new_ptr;
}