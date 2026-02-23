.global context_switch
.global task_trampoline
.global task_start
task_trampoline:
    cld
    sti
    pop %rdi
    sub $8, %rsp
    call *%rdi
    add $8, %rsp
1:
    hlt
    jmp 1b
context_switch:
    push %rbp
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15
    mov %rsp, (%rdi)
    mov (%rsi), %rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    pop %rbp
    cld
    ret

task_start:
    cli
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    mov $0x3F8, %dx
    mov $'X', %al
    out %al, %dx
    mov %rdi, %rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax
    mov $0x3F8, %dx
    mov $'J', %al
    out %al, %dx
    mov %rsp, %r12
    mov $'i', %al
    out %al, %dx
    mov (%r12), %r10
    mov $16, %ecx
1:
    mov %r10, %rax
    shr $60, %rax
    and $0xF, %al
    cmp $9, %al
    jbe 2f
    add $7, %al
2:
    add $'0', %al
    out %al, %dx
    shl $4, %r10
    dec %ecx
    jnz 1b
    mov $'c', %al
    out %al, %dx
    mov 8(%r12), %r10
    mov $16, %ecx
3:
    mov %r10, %rax
    shr $60, %rax
    and $0xF, %al
    cmp $9, %al
    jbe 4f
    add $7, %al
4:
    add $'0', %al
    out %al, %dx
    shl $4, %r10
    dec %ecx
    jnz 3b
    mov $'f', %al
    out %al, %dx
    mov 16(%r12), %r10
    mov $16, %ecx
5:
    mov %r10, %rax
    shr $60, %rax
    and $0xF, %al
    cmp $9, %al
    jbe 6f
    add $7, %al
6:
    add $'0', %al
    out %al, %dx
    shl $4, %r10
    dec %ecx
    jnz 5b
    add $24, %rsp
    jmp task_trampoline
