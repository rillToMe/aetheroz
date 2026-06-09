%include "arch/x86/isr_macro.inc"

section .text
global lapic_timer_isr_stub
extern lapic_timer_handler

lapic_timer_isr_stub:
    push 0
    push 240
    PUSHA64

    mov rdi, rsp
    call lapic_timer_handler
    mov rsp, rax

    POPA64
    add rsp, 16
    iretq
