section .text
global timer_isr_stub
extern timer_handler

timer_isr_stub:
    pushad               ; Simpan status CPU
    call timer_handler   ; Panggil logika di C
    popad                ; Kembalikan status CPU
    iretd                ; Kembali ke proses utama