; ============================================================
; arch/x86/timer_isr.asm — Preemptive Timer ISR (IRQ0), Kyuzen OS
;
; FLOW:
;   IRQ0 fires → CPU auto-push SS/RSP/RFLAGS/CS/RIP
;             → push error_code(0) + int_num(32)
;             → PUSHA64 (simpan semua GP register)
;             → mov rdi, rsp  (kirim frame ke timer_handler)
;             → call timer_handler  (returns: RSP task berikutnya di RAX)
;             → mov rsp, rax  ← CONTEXT SWITCH: ganti stack ke task berikutnya!
;             → POPA64 (restore GP register dari frame task baru)
;             → add rsp, 16   (skip int_num + error_code)
;             → iretq          (CPU restore RIP, CS, RFLAGS, RSP, SS dari frame)
;
; KUNCI: timer_handler() mengembalikan registers_t* (RSP baru).
;        Jika tidak ada switch, ia mengembalikan RSP yang sama (r = r).
;        Jika ada switch, ia mengembalikan RSP task berikutnya.
; ============================================================

%include "arch/x86/isr_macro.inc"

section .text
global timer_isr_stub
extern timer_handler    ; registers_t* timer_handler(registers_t* r)

timer_isr_stub:
    push 0              ; error_code = 0 (timer tidak punya error code)
    push 32             ; int_num = 32 (IRQ0 = INT 0x20)
    PUSHA64             ; Simpan semua 15 GP register (lihat layout di isr_macro.inc)

    ; RSP sekarang menunjuk ke r15 di ISR frame = pointer ke registers_t
    mov rdi, rsp        ; Arg 1 (rdi) = registers_t* current_regs

    call timer_handler  ; Panggil C handler, return value di RAX = registers_t* baru

    ; ── CONTEXT SWITCH ──
    ; RAX = RSP task berikutnya (mungkin sama dengan RSP saat ini jika tidak ada switch)
    ; Ini adalah "kunci" dari preemptive scheduling!
    mov rsp, rax        ; Ganti RSP ke frame task berikutnya

    POPA64              ; Restore GP register dari frame task baru
    add rsp, 16         ; Buang int_num (8 byte) + error_code (8 byte)
    iretq               ; CPU restore RIP, CS, RFLAGS, RSP, SS → lanjutkan task!