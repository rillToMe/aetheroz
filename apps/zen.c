#include "zen.h"
#include "fs.h"
#include "tty.h"
#include "kyuzenfs.h"
#include "heap.h"
#include "string.h"

extern fs_node_t tty_node;
extern uint32_t keyboard_read(uint8_t *buffer, uint32_t size);
extern uint32_t string_length(const char* str);

void zen_main(char* filename) {
    // 1. Bersihkan layar & pasang Header ala Nano
    tty_clear();
    char* header = "--- ZEN EDITOR (Tekan ESC untuk Simpan & Keluar) ---\n";
    write_fs(&tty_node, 0, string_length(header), (uint8_t*)header);

    // 2. Sewa RAM 4KB untuk menampung teks sementara
    char* text_buffer = (char*)kmalloc(4096);
    memset(text_buffer, 0, 4096);
    uint32_t cursor = 0;

    // 3. Load file lama jika sudah ada
    if (kfs_exists(filename)) {
        kfs_read_to_buffer(filename, text_buffer);
        cursor = string_length(text_buffer);
        // Tampilkan isi file lama ke layar agar bisa diedit
        write_fs(&tty_node, 0, cursor, (uint8_t*)text_buffer);
    }

    // 4. EDITOR LOOP (Ambil alih kontrol dari Shell)
    uint8_t key[1];
    while(1) {
        if (keyboard_read(key, 1) > 0) {
            char c = key[0];

            if (c == 27) { // 27 adalah kode ASCII untuk tombol ESCAPE
                break;     // Keluar dari Editor
            }
            else if (c == '\b') {
                if (cursor > 0) {
                    cursor--;
                    text_buffer[cursor] = '\0';
                    write_fs(&tty_node, 0, 1, (uint8_t*)"\b"); 
                }
            }
            else if (c == '\n') {
                if (cursor < 4095) {
                    text_buffer[cursor] = '\n';
                    cursor++;
                    write_fs(&tty_node, 0, 1, (uint8_t*)"\n");
                }
            }
            else {
                if (cursor < 4095) {
                    text_buffer[cursor] = c;
                    cursor++;
                    write_fs(&tty_node, 0, 1, (uint8_t*)&c);
                }
            }
        }
        __asm__ volatile("hlt"); // Hemat CPU
    }

    // 5. PROSES PENYIMPANAN
    tty_clear();
    char* msg_save = "Menyimpan file ke Hard Disk...\n";
    write_fs(&tty_node, 0, string_length(msg_save), (uint8_t*)msg_save);

    // Kalau file sudah ada, hapus rantai sektor lamanya
    if (kfs_exists(filename)) {
        kfs_delete_file(filename);
    }
    
    // Tulis ulang sebagai file baru dengan isi yang sudah di-update
    kfs_create_file(filename, text_buffer);

    kfree(text_buffer);
    
    // Kembalikan layar bersih untuk Shell
    tty_clear();
}