#include <stdint.h>
#include "fs.h"
#include "tty.h"
#include "pmm.h"
#include "paging.h"
#include "heap.h"
#include "string.h"

extern void init_gdt();
extern void init_idt();
extern void pic_remap();
extern fs_node_t tty_node;

// Fungsi kecil pengganti strlen() bawaan C
uint32_t string_length(const char* str) {
    uint32_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

void print_hex(uint32_t num) {
    char hex_str[11] = "0x00000000";
    char hex_chars[] = "0123456789ABCDEF";
    for (int i = 9; i >= 2; i--) {
        hex_str[i] = hex_chars[num & 0xF];
        num >>= 4;
    }
    write_fs(&tty_node, 0, 10, (uint8_t*)hex_str);
    write_fs(&tty_node, 0, 1, (uint8_t*)"\n");
}

void kernel_main(void) {
    // 1. Inisialisasi hardware layar sebagai "file" bernama tty0
    fs_node_t* tty0 = init_tty();
    
    // 2. Inisialisasi memori & interupsi CPU
    init_gdt();
    init_idt();

    pmm_init();
    init_paging();
    init_heap();

    char* msg1 = "========================================\n";
    write_fs(tty0, 0, string_length(msg1), (uint8_t*)msg1);
    char* msg2 = "   Kyuzen OS - Memory Manager Online    \n";
    write_fs(tty0, 0, string_length(msg2), (uint8_t*)msg2);

    char* msg_pg = "   Kyuzen OS - Paging MMU Enabled       \n"; // Pesan tambahan
    write_fs(tty0, 0, string_length(msg_pg), (uint8_t*)msg_pg);

    write_fs(tty0, 0, string_length(msg1), (uint8_t*)msg1);
    
    // --- UJI COBA ALOKASI RAM ---
    char* dynamic_text = (char*)kmalloc(50);
    if (dynamic_text != NULL) {
        // Salin teks ke memori dinamis yang baru dialokasikan
        memcpy(dynamic_text, "[OK] kmalloc() berhasil! Teks ini dari Heap.\n", 46);
        
        // Cetak ke layar
        write_fs(tty0, 0, string_length(dynamic_text), (uint8_t*)dynamic_text);
        
        // Jangan lupa dikembalikan!
        kfree(dynamic_text);
    }
    
    char* msg3 = "\nSistem File Unix-style berjalan! Ketik sesuatu...\n";
    write_fs(tty0, 0, string_length(msg3), (uint8_t*)msg3);

    pic_remap();
    __asm__ volatile("sti");
    
    uint32_t* alamat_terlarang = (uint32_t*)0x10000000;
    *alamat_terlarang = 0xDEADBEEF; // CPU akan panik saat membaca baris ini!
    
    // --- BASIC SHELL LOOP ---
    uint8_t key_buffer[1];
    uint32_t current_line_length = 0; // Pelacak jumlah huruf yang sedang diketik user
    
    while (1) {
        uint32_t bytes_read = read_fs(tty0, 0, 1, key_buffer);
        
        if (bytes_read > 0) {
            char c = key_buffer[0];
            
            if (c == '\b') {
                // Kalau user menekan Backspace, cek dulu:
                // Apakah user sudah mengetik sesuatu? Kalau belum, abaikan.
                if (current_line_length > 0) {
                    write_fs(tty0, 0, 1, key_buffer); // Pantulkan backspace ke layar
                    current_line_length--;            // Kurangi hitungan huruf
                }
            } 
            else if (c == '\n') {
                // Kalau menekan Enter, pantulkan ke layar, dan reset hitungan huruf
                write_fs(tty0, 0, 1, key_buffer);
                current_line_length = 0;
            } 
            else {
                // Karakter biasa: Pantulkan ke layar dan tambah hitungan huruf
                write_fs(tty0, 0, 1, key_buffer);
                current_line_length++;
            }
        }
        
        __asm__ volatile("hlt"); 
    }
}