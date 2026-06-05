%include "arch/x86/isr_macro.inc"
section .text
global keyboard_isr_stub
extern keyboard_handler

keyboard_isr_stub:
    push 0
    push 33         ; IRQ 1 (Master PIC → INT 0x21 = 33)
    PUSHA64
    call keyboard_handler  ; keyboard_handler() tidak punya parameter
    POPA64
    add rsp, 16
    iretq
