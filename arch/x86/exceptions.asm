; =======================================================================
; exceptions.asm — Handler ASM untuk semua CPU Exceptions
; =======================================================================
; PENTING: Beberapa exception mendorong error code ke stack, beberapa tidak.
; Kita normalisasi dengan mendorong dummy 0 untuk yang tidak punya error code,
; agar semua handler C menerima argumen yang konsisten.
;
; Stack saat handler C dipanggil (dari atas ke bawah):
;   [esp+0]  = error_code (asli atau dummy 0)
;   [esp+4]  = eip (dari CPU)
;   [esp+8]  = cs (dari CPU)
;   [esp+12] = eflags (dari CPU)
;   ... (esp/ss jika ring change)
;
; Register dump dikirim via struct yang kita push sebelum call ke C.
; =======================================================================

section .text

; --- DEKLARASI GLOBAL (Untuk dipasang di IDT oleh idt.c) ---
global isr0_stub    ; Divide by Zero
global isr1_stub    ; Debug
global isr2_stub    ; NMI
global isr3_stub    ; Breakpoint
global isr4_stub    ; Overflow
global isr5_stub    ; Bound Range
global isr6_stub    ; Invalid Opcode
global isr7_stub    ; Device Not Available
global isr8_stub    ; Double Fault (punya error code)
global isr10_stub   ; Invalid TSS (punya error code)
global isr11_stub   ; Segment Not Present (punya error code)
global isr12_stub   ; Stack Fault (punya error code)
global isr13_stub   ; General Protection Fault (punya error code)

; --- DEKLARASI FUNGSI C YANG AKAN DIPANGGIL ---
extern exception_handler    ; Satu handler C untuk semua exception

; =======================================================================
; MACRO: Exception tanpa error code — dorong dummy 0 agar stack konsisten
; =======================================================================
%macro ISR_NOERRCODE 1
isr%1_stub:
    cli
    push dword 0        ; Dummy error code
    push dword %1       ; Nomor interrupt
    jmp exception_common_stub
%endmacro

; =======================================================================
; MACRO: Exception dengan error code — CPU sudah push error code
; =======================================================================
%macro ISR_ERRCODE 1
isr%1_stub:
    cli
    push dword %1       ; Nomor interrupt (error code sudah di stack)
    jmp exception_common_stub
%endmacro

; =======================================================================
; Instansiasi semua handler
; =======================================================================
ISR_NOERRCODE  0    ; Divide by Zero
ISR_NOERRCODE  1    ; Debug
ISR_NOERRCODE  2    ; NMI
ISR_NOERRCODE  3    ; Breakpoint
ISR_NOERRCODE  4    ; Overflow
ISR_NOERRCODE  5    ; Bound Range Exceeded
ISR_NOERRCODE  6    ; Invalid Opcode
ISR_NOERRCODE  7    ; Device Not Available
ISR_ERRCODE    8    ; Double Fault
ISR_ERRCODE   10    ; Invalid TSS
ISR_ERRCODE   11    ; Segment Not Present
ISR_ERRCODE   12    ; Stack-Segment Fault
ISR_ERRCODE   13    ; General Protection Fault

; =======================================================================
; STUB BERSAMA — Kumpulkan semua register, panggil exception_handler di C
; =======================================================================
exception_common_stub:
    ; Pada titik ini, stack berisi (dari atas ke bawah):
    ;   esp+0  = int_num
    ;   esp+4  = error_code (atau 0 dummy)  <- WAIT: urutan terbalik!
    ;
    ; KOREKSI: ISR_NOERRCODE push: dword 0 dulu, lalu int_num
    ;   esp+0  = int_num
    ;   esp+4  = error_code (dummy 0)
    ;   esp+8  = eip (dari CPU)
    ;   esp+12 = cs
    ;   esp+16 = eflags
    ;
    ; ISR_ERRCODE push: int_num saja (error_code sudah ada di bawahnya dari CPU)
    ;   esp+0  = int_num
    ;   esp+4  = error_code (dari CPU)
    ;   esp+8  = eip
    ;   esp+12 = cs
    ;   esp+16 = eflags

    ; Simpan semua register general-purpose
    pushad      ; push: edi, esi, ebp, esp, ebx, edx, ecx, eax

    ; Baca CR2 (fault address untuk page fault, berguna juga untuk debug lain)
    mov eax, cr2
    push eax    ; kirim fault_addr ke C

    ; Ambil nomor interrupt dan error code dari posisi stack sebelum pushad
    ; Setelah pushad (8 reg x 4 = 32 byte) + push eax (4 byte) = 36 byte offset
    ; esp+36 = int_num, esp+40 = error_code (atau dummy)
    mov eax, [esp + 36]   ; int_num
    push eax
    mov eax, [esp + 44]   ; error_code  (offset +4 karena kita sudah push int_num)
    push eax

    ; Panggil handler C kita dengan 3 argumen:
    ;   exception_handler(int_num, error_code, fault_addr_cr2)
    ; Stack: [esp] = error_code, [esp+4] = int_num, [esp+8] = cr2
    ; C calling convention: argumen dalam urutan terbalik
    ; Kita sudah push: cr2, int_num, error_code (dari bawah ke atas)
    ; Jadi di C: exception_handler(error_code, int_num, cr2_value)
    ;
    ; PERBAIKAN: Kita buat wrapper yang benar
    call exception_handler

    add esp, 12     ; Bersihkan 3 argumen yang kita push
    pop eax         ; Bersihkan push cr2

    popad           ; Kembalikan semua register
    add esp, 8      ; Bersihkan int_num dan error_code dari stack
    iretd
