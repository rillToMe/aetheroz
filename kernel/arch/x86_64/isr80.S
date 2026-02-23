.global isr80_stub
.extern syscall_dispatch

isr80_stub:
    push %rbp
    mov %rsp, %rbp

    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    cld
    mov %rdx, %r10
    mov %rax, %rdi
    mov %rbx, %rsi
    mov %rcx, %rdx
    mov %r10, %rcx
    mov %rsp, %r12
    and $-16, %rsp
    call syscall_dispatch
    mov %r12, %rsp
    mov %rax, -8(%rbp)

    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax

    leave
    iretq
