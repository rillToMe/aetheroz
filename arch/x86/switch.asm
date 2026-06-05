section .text
global switch_task

; void switch_task(uint64_t *old_rsp, uint64_t new_rsp);
; Parameter: rdi = old_rsp (pointer ke RSP lama), rsi = new_rsp (RSP baru)
switch_task:
    ; 1. Simpan semua isi "otak" CPU dari program yang sedang berjalan
    ;    (pusha tidak ada di 64-bit — push manual callee-saved registers)
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; 2. Parameter sudah ada di rdi & rsi (System V AMD64 ABI)
    ;    rdi = pointer ke old_rsp, rsi = new_rsp

    ; 3. Simpan state memori lama, dan SUNTIKKAN state memori baru!
    mov [rdi], rsp          ; Simpan RSP lama ke *old_rsp
    mov rsp, rsi            ; Ganti RSP dengan stack program kedua!

    ; 4. Pulihkan "otak" CPU untuk program kedua
    ;    (popa tidak ada di 64-bit — pop manual callee-saved registers)
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret