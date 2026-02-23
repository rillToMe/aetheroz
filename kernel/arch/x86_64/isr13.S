.global isr13_stub

isr13_stub:
    mov $0x3F8, %dx
    mov $'G', %al
    out %al, %dx
    mov %rsp, %r12
    mov $'0', %al
    out %al, %dx
    mov 0(%r12), %r10
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
    mov $'1', %al
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
    mov $'2', %al
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
    mov $'3', %al
    out %al, %dx
    mov 24(%r12), %r10
    mov $16, %ecx
7:
    mov %r10, %rax
    shr $60, %rax
    and $0xF, %al
    cmp $9, %al
    jbe 8f
    add $7, %al
8:
    add $'0', %al
    out %al, %dx
    shl $4, %r10
    dec %ecx
    jnz 7b
9:
    hlt
    jmp 9b
