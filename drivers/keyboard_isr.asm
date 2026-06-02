section .text
global keyboard_isr_stub
extern keyboard_handler  ; Ini fungsi C yang akan kita buat

keyboard_isr_stub:
    pushad               ; Simpan semua status register CPU saat ini
    call keyboard_handler ; Panggil logika driver di bahasa C
    popad                ; Kembalikan status register CPU
    iretd                ; Interrupt Return (kembali ke aktivitas OS sebelumnya)