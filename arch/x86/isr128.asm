section .text
global isr128_stub
extern syscall_handler

isr128_stub:
    pushad               ; Simpan semua register (EAX, ECX, EDX, EBX, dll)
    
    push esp             ; Kirim pointer stack (yang berisi semua register tadi) ke bahasa C
    call syscall_handler ; Panggil fungsi C kita
    add esp, 4           ; Buang pointer dari stack setelah fungsi C selesai
    
    ; SANGAT PENTING: Fungsi C mengembalikan nilai di register EAX.
    ; Kita harus menimpa EAX lama yang tersimpan di stack (posisi esp+28)
    ; supaya saat 'popad', CPU memuat nilai EAX yang baru dari Kernel!
    mov [esp+28], eax    

    popad                ; Pulihkan semua register
    iretd                ; Kembali ke aplikasi