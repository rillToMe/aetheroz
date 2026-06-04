section .text
global mouse_isr_stub
extern mouse_handler

mouse_isr_stub:
    pushad              ; Simpan status register saat ini
    call mouse_handler  ; Panggil fungsi C yang baru kita buat di mouse.c
    popad               ; Kembalikan register
    iretd               ; Kembali ke program yang sedang berjalan