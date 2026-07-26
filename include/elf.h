#ifndef ELF_H
#define ELF_H

#include <stdint.h>

// ELF Magic Number ("\x7F E L F")
#define ELF_MAGIC 0x464C457F

// ELF Class (e_ident[4])
#define ELFCLASS32 1
#define ELFCLASS64 2

// ======================================================================
// ELF64 Header — untuk user apps 64-bit (x86_64)
// ======================================================================
typedef struct {
    uint8_t  e_ident[16];   // Magic + class + data + version + OS/ABI
    uint16_t e_type;        // 2 = Executable
    uint16_t e_machine;     // 0x3E = x86_64
    uint32_t e_version;
    uint64_t e_entry;       // Entry point (64-bit!)
    uint64_t e_phoff;       // Offset ke Program Header Table
    uint64_t e_shoff;       // Offset ke Section Header Table
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf64_ehdr_t;

// ======================================================================
// ELF64 Program Header — describes each loadable segment
// ======================================================================
typedef struct {
    uint32_t p_type;        // 1 = PT_LOAD
    uint32_t p_flags;       // R/W/X flags (di ELF64 ada SEBELUM p_offset!)
    uint64_t p_offset;      // Posisi di file
    uint64_t p_vaddr;       // Alamat virtual tujuan
    uint64_t p_paddr;       // Alamat fisik (biasanya sama dengan p_vaddr)
    uint64_t p_filesz;      // Ukuran di file
    uint64_t p_memsz;       // Ukuran di RAM (>filesz untuk .bss)
    uint64_t p_align;
} __attribute__((packed)) elf64_phdr_t;

// Backward-compat aliases (ELF32 — tidak lagi dipakai untuk loading)
typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf32_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed)) elf32_phdr_t;

#define USER_STACK_SIZE (256 * 1024)

// Returns 64-bit entry point.
// *out_stack_top = 16-byte-aligned top of new user stack (set RSP here).
// *out_stack_base = raw kmalloc pointer (kfree this on exit/exec).
uint64_t elf_load_file(char* filename, uint64_t* out_stack_top, void** out_stack_base);

#endif