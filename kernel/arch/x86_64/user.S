.global enter_user_mode

enter_user_mode:
    mov $0x23, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    pushq $0x23
    pushq %rsi
    pushfq
    pop %rax
    pushq %rax
    pushq $0x1b
    pushq %rdi
    iretq
