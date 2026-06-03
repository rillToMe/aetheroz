section .text
global isr14_stub
extern page_fault_handler

isr14_stub:
    pushad                  ; Simpan semua register CPU
    call page_fault_handler ; Panggil fungsi layar biru di C
    popad                   ; (Opsional) Kembalikan register
    add esp, 4              ; PENTING: Buang Error Code (4 byte) dari stack
    iretd