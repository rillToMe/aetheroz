.global gdt_load

gdt_load:
    lgdt (%rdi)
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs
    pushq $0x08
    lea 1f(%rip), %rax
    pushq %rax
    lretq
1:
    ret
