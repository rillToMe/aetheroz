section .text
global isr8_stub  ; Double Fault
global isr13_stub ; General Protection Fault (Sering terjadi saat error GDT)

extern print_hex

isr8_stub:
    cli
    hlt ; Berhenti selamanya jika Double Fault

isr13_stub:
    cli
    push dword 13 ; Kirim angka 13 (0xD) ke fungsi print_hex
    call print_hex
    add esp, 4
    hlt ; Berhenti selamanya