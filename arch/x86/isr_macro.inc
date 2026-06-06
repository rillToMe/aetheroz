; ============================================================
; arch/x86/isr_macro.inc — PUSHA64 / POPA64 untuk Kyuzen OS
;
; STACK FRAME LAYOUT setelah PUSHA64 (RSP → atas stack):
;
;   [RSP+  0]  r15   ← RSP saat ini (pointer ke registers_t)
;   [RSP+  8]  r14
;   [RSP+ 16]  r13
;   [RSP+ 24]  r12
;   [RSP+ 32]  r11
;   [RSP+ 40]  r10
;   [RSP+ 48]  r9
;   [RSP+ 56]  r8
;   [RSP+ 64]  rdi
;   [RSP+ 72]  rsi
;   [RSP+ 80]  rbp
;   [RSP+ 88]  rdx
;   [RSP+ 96]  rcx
;   [RSP+104]  rbx
;   [RSP+112]  rax   ← isr128: mov [rsp+112], rax (return value)
;   [RSP+120]  int_num   (push INT_NUM by stub)
;   [RSP+128]  error_code (push 0 by stub, FIRST push = higher addr)
;   [RSP+136]  rip        (CPU auto-push on interrupt)
;   [RSP+144]  cs
;   [RSP+152]  rflags
;   [RSP+160]  rsp        (user/old RSP)
;   [RSP+168]  ss
;
; Ini cocok 1-to-1 dengan struct registers_t di task.h:
;   typedef struct { r15, r14, ..., rax, int_num, error_code,
;                    rip, cs, rflags, rsp, ss } registers_t;
;
; PENTING: Jangan ubah urutan PUSH/POP ini!
;          Seluruh ISR handler dan Preemptive Scheduler bergantung pada layout ini.
; ============================================================

%macro PUSHA64 0
    push rax        ; pushed first → highest address (RSP+112)
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15        ; pushed last → lowest address (RSP+0)
%endmacro

%macro POPA64 0
    pop r15         ; pop from RSP+0 first
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax         ; pop from RSP+112 last
%endmacro