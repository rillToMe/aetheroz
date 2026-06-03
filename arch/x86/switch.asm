section .text
global switch_task

; void switch_task(uint32_t *old_esp, uint32_t new_esp);
switch_task:
    ; 1. Simpan semua isi "otak" CPU dari program yang sedang berjalan
    pusha                   

    ; 2. Ambil parameter dari bahasa C (Pointer lama & baru)
    mov eax, [esp + 36]     ; [esp+36] adalah posisi variabel 'old_esp'
    mov ebx, [esp + 40]     ; [esp+40] adalah posisi variabel 'new_esp'

    ; 3. Simpan state memori lama, dan SUNTIKKAN state memori baru!
    mov [eax], esp          ; Simpan ESP lama
    mov esp, ebx            ; Ganti otak CPU dengan memori program kedua!

    ; 4. Pulihkan "otak" CPU untuk program kedua
    popa                    
    ret