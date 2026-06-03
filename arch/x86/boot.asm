MBALIGN  equ  1 << 0            ; Align loaded modules on page boundaries
MEMINFO  equ  1 << 1            ; Provide memory map
FLAGS    equ  MBALIGN | MEMINFO ; Multiboot 'flag' field
MAGIC    equ  0x1BADB002        ; 'magic number' let bootloader find the header
CHECKSUM equ -(MAGIC + FLAGS)   ; Checksum of above, to prove we are multiboot

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom:
resb 16384 ; Reserve 16 KiB untuk stack
stack_top:

section .text
global _start
extern kernel_main

_start:
    ; Setup stack pointer ke top of stack
    mov esp, stack_top

    ; --- TAMBAHKAN DUA BARIS INI ---
    ; Dorong argumen ke stack agar bisa dibaca oleh fungsi C (kernel_main)
    push ebx  ; Argumen 2: Pointer ke Laporan Multiboot
    push eax  ; Argumen 1: Magic Number (0x2BADB002)
    
    ; Pindah ke kode C kita!
    call kernel_main

    ; Kalau kernel_main selesai (seharusnya tidak), matikan interrupt dan halt system
    cli
.hang:
    hlt
    jmp .hang