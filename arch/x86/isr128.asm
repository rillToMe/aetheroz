section .text
global isr128_stub
extern syscall_handler

; =======================================================================
; isr128_stub — System Call Handler (int 0x80)
; =======================================================================
; PENTING: Jangan tambahkan `sti` di sini!
; Alasannya:
;   - Timer ISR bisa menyela di tengah syscall_handler
;   - Timer memanggil compositor_flush() + yield()
;   - Ini bisa merusak stack frame syscall → Invalid Opcode (INT 6) / Triple Fault
;   - ATA driver sudah punya cli/sti guard sendiri di ata_read/write_sector
;
; CPU secara otomatis mematikan interrupt saat masuk ISR (EFLAGS.IF = 0).
; `iretd` akan memulihkan EFLAGS (termasuk IF) saat kembali ke user space.
; Jadi interrupt OTOMATIS aktif kembali di Ring 3 setelah iretd.
; =======================================================================

isr128_stub:
    pushad               ; Simpan semua register (EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI)
    
    push esp             ; Kirim pointer stack (register dump) ke fungsi C
    call syscall_handler ; Panggil syscall handler di C
    add esp, 4           ; Bersihkan argumen
    
    ; PENTING: Fungsi C mengembalikan return value di EAX.
    ; Timpa EAX lama di stack (posisi esp+28 setelah pushad)
    ; agar saat popad, aplikasi mendapat return value syscall di EAX.
    mov [esp+28], eax    

    popad                ; Pulihkan semua register
    iretd                ; Kembali ke aplikasi (juga memulihkan EFLAGS = interrupt ON)
