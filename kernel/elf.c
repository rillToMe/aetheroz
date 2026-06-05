#include "elf.h"
#include "kyuzenfs.h"
#include "heap.h"
#include "string.h"
#include "paging.h"  // Untuk paging_map_region() dan paging_is_mapped()

extern void kprint(const char* str);
extern void kprint_num(uint32_t num);

// =======================================================================
// elf_load_file() — Muat file ELF ke RAM dan kembalikan entry point-nya
//
// ALUR KERJA:
//   1. Baca file ELF dari KyuzenFS ke buffer sementara
//   2. Validasi ELF magic number (0x7F 'E' 'L' 'F')
//   3. Untuk setiap PT_LOAD segment:
//      a. PRE-MAP: Petakan semua blok 4MB yang dibutuhkan segmen ke Page Table
//      b. COPY: Salin data dari file ke alamat virtual yang diminta (p_vaddr)
//      c. ZERO BSS: Isi sisa ruang (p_memsz > p_filesz) dengan nol
//   4. Kembalikan entry point (e_entry)
// =======================================================================
uint32_t elf_load_file(char* filename) {
    uint32_t file_size = kfs_get_file_size(filename);
    if (file_size == 0) {
        kprint("[ELF] Error: File kosong atau tidak ditemukan!\n");
        return 0;
    }

    uint8_t* file_buffer = (uint8_t*)kmalloc(file_size);
    if (!file_buffer) {
        kprint("[ELF] Error: Heap habis, tidak bisa alokasi buffer!\n");
        return 0;
    }
    kfs_read_to_buffer(filename, (char*)file_buffer);

    // Validasi ELF Magic Number: 0x7F 'E' 'L' 'F'
    elf32_ehdr_t* elf_hdr = (elf32_ehdr_t*)file_buffer;
    if (elf_hdr->e_ident[0] != 0x7F ||
        elf_hdr->e_ident[1] != 'E'  ||
        elf_hdr->e_ident[2] != 'L'  ||
        elf_hdr->e_ident[3] != 'F') {
        kprint("[ELF] Error: Magic Number tidak cocok (bukan file ELF)!\n");
        kfree(file_buffer);
        return 0;
    }

    // Bongkar tabel Program Header
    elf32_phdr_t* phdr = (elf32_phdr_t*)(file_buffer + elf_hdr->e_phoff);

    for (int i = 0; i < elf_hdr->e_phnum; i++) {
        // Hanya proses segment yang bertipe PT_LOAD (type == 1)
        if (phdr[i].p_type == 1) {
            uint32_t seg_vaddr = phdr[i].p_vaddr;
            uint32_t seg_end   = seg_vaddr + phdr[i].p_memsz;

            // ----------------------------------------------------------
            // PRE-MAP: Petakan semua blok 4MB yang dicakup segmen ini
            // Segmen bisa merentang lebih dari satu blok 4MB, jadi kita
            // iterasi dari batas bawah hingga batas atas (per 4MB).
            // ----------------------------------------------------------
            uint32_t block = seg_vaddr & 0xFFC00000; // Bulatkan ke bawah ke 4MB
            while (block < seg_end) {
                if (!paging_map_region(block)) {
                    kprint("[ELF] FATAL: Pool page table habis! Tidak bisa memetakan segmen.\n");
                    kprint("[ELF] Segmen p_vaddr = 0x"); kprint_num(seg_vaddr); kprint("\n");
                    kfree(file_buffer);
                    return 0;
                }
                block += 0x400000; // Lanjut ke blok 4MB berikutnya
            }
            // ----------------------------------------------------------

            // COPY: Pindahkan data segmen ke alamat virtual yang diminta
            memcpy((void*)seg_vaddr,
                   file_buffer + phdr[i].p_offset,
                   phdr[i].p_filesz);

            // ZERO BSS: Jika memsz > filesz, sisa ruang adalah .bss (harus diisi 0)
            if (phdr[i].p_memsz > phdr[i].p_filesz) {
                memset((uint8_t*)seg_vaddr + phdr[i].p_filesz,
                       0,
                       phdr[i].p_memsz - phdr[i].p_filesz);
            }
        }
    }

    // Catat dan kembalikan entry point
    uint32_t entry_point = elf_hdr->e_entry;

    // Bebaskan buffer sementara
    kfree(file_buffer);

    return entry_point;
}