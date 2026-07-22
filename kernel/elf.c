#include "elf.h"
#include "kyuzenfs.h"
#include "heap.h"
#include "string.h"
#include "paging.h"

extern void kprint(const char* str);
extern void kprint_num(uint32_t num);

// =======================================================================
// elf_load_file() — Load ELF64 user-app ke RAM, kembalikan entry point
//
// CATATAN: User apps dikompilasi sebagai 64-bit ELF (e_machine = 0x3E = x86_64)
// ELF64 berbeda dari ELF32 dalam dua hal penting:
//   1. e_entry, e_phoff, e_shoff adalah uint64_t (bukan uint32_t)
//   2. Di Program Header, p_flags ada SEBELUM p_offset (bukan setelah!)
// Menggunakan struct ELF32 untuk ELF64 → semua field offset salah → crash!
// =======================================================================
uint64_t elf_load_file(char* filename) {
    uint32_t file_size = kfs_get_file_size(filename);
    if (file_size == 0) {
        kprint("[ELF] Error: File kosong atau tidak ditemukan!\n");
        return 0;
    }

    uint8_t* file_buffer = (uint8_t*)kmalloc(file_size);
    if (!file_buffer) {
        kprint("[ELF] Error: Heap habis!\n");
        return 0;
    }
    kfs_read_to_buffer(filename, (char*)file_buffer, file_size);

    // Validasi ELF Magic Number
    if (file_buffer[0] != 0x7F || file_buffer[1] != 'E' ||
        file_buffer[2] != 'L'  || file_buffer[3] != 'F') {
        kprint("[ELF] Error: Bukan file ELF!\n");
        kfree(file_buffer);
        return 0;
    }

    // Deteksi class: byte[4] = 1 (ELF32) atau 2 (ELF64)
    uint8_t elf_class = file_buffer[4];

    if (elf_class == ELFCLASS64) {
        // ===========================
        //  LOAD ELF64 (x86_64)
        // ===========================
        elf64_ehdr_t* hdr  = (elf64_ehdr_t*)file_buffer;
        elf64_phdr_t* phdr = (elf64_phdr_t*)(file_buffer + hdr->e_phoff);

        for (int i = 0; i < hdr->e_phnum; i++) {
            if (phdr[i].p_type != 1) continue; // Hanya PT_LOAD

            uint64_t seg_vaddr = phdr[i].p_vaddr;
            uint64_t seg_end   = seg_vaddr + phdr[i].p_memsz;

            // PRE-MAP: map semua 4KB pages yang dicakup segmen
            for (uint64_t page = seg_vaddr & ~0xFFFULL; page < seg_end; page += 4096) {
                if (!paging_is_mapped(page)) {
                    if (!vmm_alloc_page(page, 7)) {
                        kprint("[ELF64] FATAL: Tidak bisa map page 0x");
                        kprint_num((uint32_t)(page >> 32)); kprint_num((uint32_t)page);
                        kprint("\n");
                        kfree(file_buffer);
                        return 0;
                    }
                }
            }

            // COPY: salin data segmen ke virtual address tujuan
            memcpy((void*)seg_vaddr,
                   file_buffer + phdr[i].p_offset,
                   (uint32_t)phdr[i].p_filesz);

            // ZERO BSS: isi sisa ruang dengan 0
            if (phdr[i].p_memsz > phdr[i].p_filesz) {
                memset((uint8_t*)seg_vaddr + phdr[i].p_filesz,
                       0,
                       (uint32_t)(phdr[i].p_memsz - phdr[i].p_filesz));
            }
        }

        kfree(file_buffer);
        return hdr->e_entry;  // 64-bit entry point

    } else if (elf_class == ELFCLASS32) {
        // ===========================
        //  LOAD ELF32 (i386) — legacy
        // ===========================
        elf32_ehdr_t* hdr  = (elf32_ehdr_t*)file_buffer;
        elf32_phdr_t* phdr = (elf32_phdr_t*)(file_buffer + hdr->e_phoff);

        for (int i = 0; i < hdr->e_phnum; i++) {
            if (phdr[i].p_type != 1) continue;

            uint32_t seg_vaddr = phdr[i].p_vaddr;
            uint32_t seg_end   = seg_vaddr + phdr[i].p_memsz;

            uint32_t block = seg_vaddr & 0xFFC00000;
            while (block < seg_end) {
                if (!paging_map_region(block)) {
                    kprint("[ELF32] FATAL: Tidak bisa map region!\n");
                    kfree(file_buffer);
                    return 0;
                }
                block += 0x400000;
            }

            memcpy((void*)(uint64_t)seg_vaddr,
                   file_buffer + phdr[i].p_offset,
                   phdr[i].p_filesz);

            if (phdr[i].p_memsz > phdr[i].p_filesz) {
                memset((uint8_t*)(uint64_t)seg_vaddr + phdr[i].p_filesz,
                       0,
                       phdr[i].p_memsz - phdr[i].p_filesz);
            }
        }

        kfree(file_buffer);
        return (uint64_t)hdr->e_entry;
    }

    kprint("[ELF] Error: ELF class tidak dikenal!\n");
    kfree(file_buffer);
    return 0;
}