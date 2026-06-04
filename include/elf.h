#ifndef ELF_H
#define ELF_H

#include <stdint.h>

// ELF Magic Number ("\x7F E L F")
#define ELF_MAGIC 0x464C457F 

// --- ELF Header Utama ---
typedef struct {
    uint8_t  e_ident[16];   // Array berisi magic number dan info bit
    uint16_t e_type;        // 1 = Relocatable, 2 = Executable
    uint16_t e_machine;     // 3 = x86 (i386)
    uint32_t e_version;
    uint32_t e_entry;       // PENTING: Alamat memori untuk mulai mengeksekusi (Entry Point!)
    uint32_t e_phoff;       // Offset ke Program Header Table
    uint32_t e_shoff;       // Offset ke Section Header Table
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;   // Ukuran satu entri Program Header
    uint16_t e_phnum;       // Jumlah entri Program Header
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf32_ehdr_t;

// --- Program Header ---
// Mengatur bagaimana file ini di-load ke dalam memori
typedef struct {
    uint32_t p_type;        // 1 = Loadable segment (Penting!)
    uint32_t p_offset;      // Posisi data di dalam file
    uint32_t p_vaddr;       // Alamat Virtual tujuan di RAM
    uint32_t p_paddr;       // Alamat Fisik tujuan
    uint32_t p_filesz;      // Ukuran data di dalam file
    uint32_t p_memsz;       // Ukuran yang dibutuhkan di RAM (Bisa lebih besar dari filesz untuk .bss)
    uint32_t p_flags;       // Hak akses (Read, Write, Execute)
    uint32_t p_align;
} __attribute__((packed)) elf32_phdr_t;

#endif