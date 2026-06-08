%include "arch/x86/isr_macro.inc"

section .text
global lapic_timer_isr_stub
extern lapic_timer_handler

lapic_timer_isr_stub:
    push 0
    push 240
    PUSHA64

    call lapic_timer_handler

    POPA64
    add rsp, 16
    iretq
