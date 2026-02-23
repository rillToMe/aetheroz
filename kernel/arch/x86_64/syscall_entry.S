.global syscall_entry
.extern syscall_dispatch
.extern syscall_user_rsp
.extern syscall_kernel_rsp
.extern syscall_kernel_return_rip
.extern syscall_kernel_return_rsp
.extern syscall_return_to_kernel

syscall_entry:
    mov %rsp, syscall_user_rsp(%rip)
    mov syscall_kernel_rsp(%rip), %rsp
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
    mov %r10, %r8
    mov %rdx, %rcx
    mov %rsi, %rdx
    mov %rdi, %rsi
    mov %rax, %rdi
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

    cmpq $0, syscall_return_to_kernel(%rip)
    je 1f
    movq $0, syscall_return_to_kernel(%rip)
    mov syscall_kernel_return_rsp(%rip), %rsp
    jmp *syscall_kernel_return_rip(%rip)
1:
    mov syscall_user_rsp(%rip), %rsp
    sysretq
