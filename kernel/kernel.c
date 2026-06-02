#include <stdint.h>
#include "fs.h"
#include "tty.h"

extern void init_gdt();
extern void init_idt();
extern void pic_remap();

// Fungsi kecil pengganti strlen() bawaan C
uint32_t string_length(const char* str) {
    uint32_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

void kernel_main(void) {
    // 1. Inisialisasi hardware layar sebagai "file" bernama tty0
    fs_node_t* tty0 = init_tty();
    
    // 2. Inisialisasi memori & interupsi CPU
    init_gdt();
    init_idt();
    
    // 3. Uji coba VFS! Alih-alih memanggil hardware layar langsung, 
    // kita memanggil fungsi sistem file abstrak.
    
    char* msg1 = "========================================\n";
    write_fs(tty0, 0, string_length(msg1), (uint8_t*)msg1);
    
    char* msg2 = "   Kyuzen OS - VFS TTY0 Initialized     \n";
    write_fs(tty0, 0, string_length(msg2), (uint8_t*)msg2);
    
    char* msg3 = "========================================\n\n";
    write_fs(tty0, 0, string_length(msg3), (uint8_t*)msg3);
    
    char* msg4 = "[OK] /dev/tty0 is ready (Character Device).\n";
    write_fs(tty0, 0, string_length(msg4), (uint8_t*)msg4);
    
    char* msg5 = "[OK] GDT & IDT Online.\n";
    write_fs(tty0, 0, string_length(msg5), (uint8_t*)msg5);
    
    char* msg6 = "\nSistem File Unix-style berjalan! Ketik sesuatu...\n";
    write_fs(tty0, 0, string_length(msg6), (uint8_t*)msg6);

    // Aktifkan keyboard interrupt
    pic_remap();
    __asm__ volatile("sti");
}