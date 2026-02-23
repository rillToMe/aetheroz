ENTRY(kernel_main)

PHDRS
{
    text PT_LOAD FLAGS(5);   /* R + X */
    data PT_LOAD FLAGS(6);   /* R + W */
}

SECTIONS
{
    . = 0xffffffff80000000;
    __kernel_start = .;

    .text : ALIGN(4K)
    {
        *(.text*)
    } :text

    .limine_requests : ALIGN(8)
    {
        KEEP(*(.limine_requests))
    } :text

    .rodata : ALIGN(4K)
    {
        *(.rodata*)
    } :text

    .data : ALIGN(4K)
    {
        *(.data*)
    } :data

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss*)
    } :data
    __kernel_end = .;
}
