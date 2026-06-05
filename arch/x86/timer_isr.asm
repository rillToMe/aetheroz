%include "arch/x86/isr_macro.inc"
section .text
global timer_isr_stub
extern timer_handler

timer_isr_stub:
    push 0
    push 32         ; IRQ 0
    PUSHA64
    call timer_handler  ; timer_handler() tidak punya parameter
    POPA64
    add rsp, 16
    iretq