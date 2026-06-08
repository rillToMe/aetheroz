bits 64

global smp_ap_entry
extern smp_ap_main

; Limine MP calls:
;   rdi = struct limine_mp_info *cpu
;
; x86_64 limine_mp_info layout:
;   +0  uint32_t processor_id
;   +4  uint32_t lapic_id
;   +8  uint64_t reserved
;   +16 void (*goto_address)(struct limine_mp_info *)
;   +24 uint64_t extra_argument
;
; extra_argument points to smp_cpu_state_t, whose first field is stack_top.
smp_ap_entry:
    cli

    mov rsi, [rdi + 24]     ; rsi = smp_cpu_state_t *state
    mov rsp, [rsi + 0]      ; switch to AP private stack
    and rsp, -16

    call smp_ap_main

.halt:
    cli
    hlt
    jmp .halt
