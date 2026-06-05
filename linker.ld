/* ============================================================
   Kyuzen OS — Linker Script (x86-64 Higher Half, Limine Protocol)
   
   Limine menempatkan kernel di higher half virtual address space.
   Kernel harus di-link ke FFFFFFFF80000000 ke atas.
   
   Referensi: https://github.com/limine-bootloader/limine/blob/trunk/PROTOCOL.md
   ============================================================ */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)

ENTRY(kernel_main)

/* Higher-half kernel base address (standar Limine) */
KERNEL_VMA = 0xFFFFFFFF80000000;

SECTIONS
{
    /* Posisi fisik yang akan diminta ke Limine */
    . = KERNEL_VMA;

    /* ---- Kode Utama ---- */
    .text : ALIGN(4K)
    {
        *(.text .text.*)
    }

    /* ---- Read-only data ---- */
    .rodata : ALIGN(4K)
    {
        *(.rodata .rodata.*)
    }

    /* ---- Data Limine Requests (WAJIB agar request terdeteksi) ---- */
    .requests : ALIGN(4K)
    {
        KEEP(*(.requests_start_marker))
        KEEP(*(.requests))
        KEEP(*(.requests_end_marker))
    }

    /* ---- Data terinisialisasi ---- */
    .data : ALIGN(4K)
    {
        *(.data .data.*)
    }

    /* ---- Data tidak terinisialisasi + stack ---- */
    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss .bss.*)
    }

    /* Buang section debug dan sisa Multiboot (tidak relevan) */
    /DISCARD/ :
    {
        *(.note .note.*)
        *(.eh_frame)
        *(.multiboot)
    }
}