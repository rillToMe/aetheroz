%include "arch/x86/isr_macro.inc"
section .text
global isr128_stub
extern syscall_handler

isr128_stub:
    push 0          ; dummy error code
    push 128        ; int_num
    PUSHA64
    
    mov rdi, rsp    ; Arg 1: struct registers*
    call syscall_handler
    
    ; Trik 64-bit: Fungsi C mengembalikan data di RAX.
    ; Kita timpa RAX lama yang kita simpan di tumpukan stack.
    ; Posisi RAX lama tepat berada 112 byte dari dasar stack saat ini.
    mov [rsp + 112], rax    
    
    POPA64          ; Kembalikan semua, aplikasi akan dapat nilai RAX yang baru!
    add rsp, 16
    iretq