section .text
global gdt_flush

gdt_flush:
    mov eax, [esp+4]
    lgdt [eax]

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Pastikan yang di-push mutlak berukuran 32-bit
    push dword 0x08
    push dword flush_cs
    retf

flush_cs:
    ret

global tss_flush

tss_flush:
    ; Index ke-5 di GDT x 8 byte = 40 (0x28)
    mov ax, 0x28  
    ltr ax          ; Load Task Register dengan segment TSS kita!
    ret