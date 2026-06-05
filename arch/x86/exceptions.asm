%include "arch/x86/isr_macro.inc"
section .text
extern exception_handler

; --- Macro untuk Exception TANPA Error Code dari CPU ---
%macro ISR_NOERRCODE 1
global isr%1_stub
isr%1_stub:
    push 0          ; Push dummy error code agar seragam
    push %1         ; Push nomor interupsi
    PUSHA64
    mov rdi, rsp    ; Lempar pointer struct registers ke RDI (Argumen 1 C)
    call exception_handler
    POPA64
    add rsp, 16     ; Bersihkan nomor int dan error code
    iretq
%endmacro

; --- Macro untuk Exception DENGAN Error Code dari CPU ---
%macro ISR_ERRCODE 1
global isr%1_stub
isr%1_stub:
    ; CPU sudah mem-push error_code secara otomatis
    push %1         ; Push nomor interupsi
    PUSHA64
    mov rdi, rsp    ; Lempar pointer struct registers ke RDI (Argumen 1 C)
    call exception_handler
    POPA64
    add rsp, 16     ; Bersihkan nomor int dan error code
    iretq
%endmacro

; Buat gerbang exception secara massal!
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13