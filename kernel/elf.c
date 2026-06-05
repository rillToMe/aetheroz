#include "elf.h"
#include "kyuzenfs.h"
#include "heap.h"
#include "string.h"

extern void kprint(const char* str);
// Fungsi andalan kita untuk memuat ELF dan mengembalikan alamat mulainya
uint32_t elf_load_file(char* filename) {
    uint32_t file_size = kfs_get_file_size(filename);
    if (file_size == 0) {
        kprint("ELF Error: File kosong/tidak ada!\n"); // Laporan detektif
        return 0;
    }

    uint8_t* file_buffer = (uint8_t*)kmalloc(file_size);
    kfs_read_to_buffer(filename, (char*)file_buffer);

    // --- RADAR FORENSIK BINER ---
    extern void kprint_num(uint32_t num);
    kprint("\n[DEBUG] 4 Byte Pertama File:\n");
    kprint("Byte 0: "); kprint_num(file_buffer[0]); kprint("\n");
    kprint("Byte 1: "); kprint_num(file_buffer[1]); kprint("\n");
    kprint("Byte 2: "); kprint_num(file_buffer[2]); kprint("\n");
    kprint("Byte 3: "); kprint_num(file_buffer[3]); kprint("\n");
    // ----------------------------

    elf32_ehdr_t* elf_hdr = (elf32_ehdr_t*)file_buffer;
    if (elf_hdr->e_ident[0] != 0x7F) {
        kprint("ELF Error: Magic Number tidak cocok!\n"); 
        kfree(file_buffer);
        return 0;
    }

    // 4. Bongkar Program Headers (Bagian mana yang kode, bagian mana yang variabel)
    // e_phoff adalah penunjuk lokasi tabel Program Header di dalam file
    elf32_phdr_t* phdr = (elf32_phdr_t*)(file_buffer + elf_hdr->e_phoff);

    for (int i = 0; i < elf_hdr->e_phnum; i++) {
        // Jika p_type == 1 (PT_LOAD), artinya blok ini harus disalin ke RAM utama
        if (phdr[i].p_type == 1) {
            
            // Pindahkan blok data ke Alamat Virtual (p_vaddr) yang diminta aplikasi
            memcpy((void*)phdr[i].p_vaddr, file_buffer + phdr[i].p_offset, phdr[i].p_filesz);

            // BSS Section: Jika memori yang diminta lebih besar dari data aslinya,
            // sisa ruangnya adalah variabel kosong yang harus diisi dengan angka Nol.
            if (phdr[i].p_memsz > phdr[i].p_filesz) {
                memset((uint8_t*)phdr[i].p_vaddr + phdr[i].p_filesz, 0, 
                       phdr[i].p_memsz - phdr[i].p_filesz);
            }
        }
    }

    // 5. Catat titik kumpul eksekusinya (Entry Point)
    uint32_t entry_point = elf_hdr->e_entry;

    // Bersihkan RAM sementara agar tidak bocor
    kfree(file_buffer);

    return entry_point;
}