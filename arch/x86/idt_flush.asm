section .text
global idt_flush

idt_flush:
    mov eax, [esp+4]  ; Ambil pointer IDT dari parameter fungsi C
    lidt [eax]        ; Load IDT ke dalam prosesor
    ret               ; Kembali ke kode C