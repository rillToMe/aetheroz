MBALIGN  equ  1 << 0
MEMINFO  equ  1 << 1
VIDINFO  equ  1 << 2  ; <--- INI KUNCINYA! Minta Mode Grafis Resolusi Tinggi
FLAGS    equ  MBALIGN | MEMINFO | VIDINFO
MAGIC    equ  0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    ; AOUT Kludge (Kita isi nol karena kita pakai format ELF)
    dd 0, 0, 0, 0, 0
    ; --- PERMINTAAN RESOLUSI GRAFIS ---
    dd 0    ; Mode 0 = Linear Graphics (Piksel, bukan teks)
    dd 1024 ; Lebar layar (Width)
    dd 768  ; Tinggi layar (Height)
    dd 32   ; Kedalaman warna (BPP - 32 bit warna)

section .bss
align 16
stack_bottom:
resb 16384 ; 16 KB Stack
stack_top:

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top
    
    ; Lempar laporan dari Limine ke kernel_main (Ring 0)
    push ebx
    push eax
    
    call kernel_main

    cli
.hang:
    hlt
    jmp .hang