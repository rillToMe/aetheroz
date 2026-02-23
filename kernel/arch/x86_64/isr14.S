.global isr14_stub
.extern serial_write
.extern serial_write_hex64

isr14_stub:
    push %rax
    push %rcx
    push %rdx
    mov %rsp, %r12
    and $-16, %rsp
    mov $'P', %edi
    call serial_write
    mov 24(%r12), %rdi
    call serial_write_hex64
    mov %r12, %rsp
1:
    hlt
    jmp 1b
