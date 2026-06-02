ENTRY(_start)

SECTIONS
{
    /* Mulai meletakkan section di 1 Megabyte */
    . = 1M;

    /* Taruh Multiboot Header persis di awal, WAJIB di-KEEP */
    .multiboot : ALIGN(4K)
    {
        KEEP(*(.multiboot))
    }

    /* Kode utama (Assembly & C) */
    .text : ALIGN(4K)
    {
        *(.text)
    }

    /* Read-only data */
    .rodata : ALIGN(4K)
    {
        *(.rodata)
    }

    /* Read-write data (initialized) */
    .data : ALIGN(4K)
    {
        *(.data)
    }

    /* Read-write data (uninitialized) dan stack */
    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
}