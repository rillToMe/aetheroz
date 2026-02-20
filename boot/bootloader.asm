BITS 16
ORG 0x7C00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov [BOOT_DRIVE], dl

    ; print message
    mov si, msg
.print:
    lodsb
    or al, al
    jz load_kernel
    mov ah, 0x0E
    int 0x10
    jmp .print

load_kernel:
    mov ax, 0x1000
    mov es, ax
    xor bx, bx        ; BX = 0
    mov dh, [kernel_sectors]
    mov dl, [BOOT_DRIVE]
    call disk_load


    cli
    call enable_a20
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp CODE_SEG:protected_mode

BITS 32
protected_mode:
    cli

    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    mov esp, 0x90000

    mov eax, [0x10000]
    mov [0xB8000], eax
    mov al, [debug_pause]
    test al, al
    jnz debug_halt

    mov eax, 0x10000
    jmp eax

debug_halt:
    jmp debug_halt

enable_a20:
    in al, 0x92
    or al, 2
    out 0x92, al
    ret

BITS 16
disk_load:
    pusha
    mov ah, 0x02
    mov al, dh
    mov ch, 0x00
    mov dh, 0x00
    mov cl, 0x02
    int 0x13
    jc disk_error
    popa
    ret

disk_error:
    hlt
    jmp disk_error

%include "arch/x86/gdt.asm"



msg db "AetherOS booting...", 0
BOOT_DRIVE db 0
kernel_sectors db 2
debug_pause db 0

times 510-($-$$) db 0
dw 0xAA55
