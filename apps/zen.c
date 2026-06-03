#include "zen.h"
#include "fs.h"
#include "tty.h"
#include "kyuzenfs.h"
#include "heap.h"
#include "string.h"
#include "task.h"

extern fs_node_t tty_node;
extern uint32_t keyboard_read(uint8_t *buffer, uint32_t size);
extern uint32_t string_length(const char* str);

void zen_main(char* filename) {
    tty_clear();
    char* header = "--- ZEN EDITOR (Tekan ESC untuk Simpan & Keluar) ---\n";
    write_fs(&tty_node, 0, string_length(header), (uint8_t*)header);

    // 1. Kapasitas awal HANYA 32 Byte! Super irit.
    uint32_t current_capacity = 32; 
    
    // Tapi, kalau filenya sudah ada di disk dan ukurannya besar, kita sesuaikan kapasitas awalnya
    if (kfs_exists(filename)) {
        uint32_t existing_size = kfs_get_file_size(filename);
        if (existing_size >= current_capacity) {
            current_capacity = existing_size + 32; // Kasih ruang napas ekstra
        }
    }

    // 2. Sewa memori dinamis
    char* text_buffer = (char*)kmalloc(current_capacity);
    if (text_buffer == NULL) return; // Proteksi BSoD
    memset(text_buffer, 0, current_capacity);
    
    uint32_t cursor = 0;

    // 3. Load file lama jika ada
    if (kfs_exists(filename)) {
        kfs_read_to_buffer(filename, text_buffer);
        cursor = string_length(text_buffer);
        write_fs(&tty_node, 0, cursor, (uint8_t*)text_buffer);
    }

    // 4. EDITOR LOOP DINAMIS
    uint8_t key[1];
    while(1) {
        if (keyboard_read(key, 1) > 0) {
            char c = key[0];

            if (c == 27) break; // Keluar (ESC)

            // Logika Backspace
            if (c == '\b') {
                if (cursor > 0) {
                    cursor--;
                    text_buffer[cursor] = '\0';
                    write_fs(&tty_node, 0, 1, (uint8_t*)"\b"); 
                }
                continue; // Lanjut ke putaran berikutnya
            }

            // ==========================================
            // LOGIKA ELASTISITAS MEMORI (AUTO-RESIZE)
            // ==========================================
            if (cursor >= current_capacity - 1) { 
                uint32_t new_capacity = current_capacity * 2; 
                char* new_buffer = (char*)krealloc(text_buffer, current_capacity, new_capacity);
                
                if (new_buffer == NULL) {
                    char* err = "\n[FATAL] RAM Habis, auto-expand gagal!\n";
                    write_fs(&tty_node, 0, string_length(err), (uint8_t*)err);
                    
                    // Jeda sejenak agar user bisa membaca pesan error-nya!
                    for (volatile int w = 0; w < 90000000; w++); 
                    
                    break; 
                }
                
                text_buffer = new_buffer;       
                current_capacity = new_capacity; 

                // Pesan debug ini harusnya sekarang bisa muncul dengan mulus!
                char* debug_msg = "\n[SYSTEM: Memori Zen kurang! Auto-Expand dipicu...]\n";
                write_fs(&tty_node, 0, string_length(debug_msg), (uint8_t*)debug_msg);
            }
            
            // Cetak Huruf / Enter ke memori dan layar
            if (c == '\n') {
                text_buffer[cursor] = '\n';
                cursor++;
                write_fs(&tty_node, 0, 1, (uint8_t*)"\n");
            } else {
                text_buffer[cursor] = c;
                cursor++;
                write_fs(&tty_node, 0, 1, (uint8_t*)&c);
            }
        }
        __asm__ volatile("hlt");
    }

    // 5. PROSES PENYIMPANAN
    tty_clear();
    char* msg_save = "Menyimpan file ke Hard Disk...\n";
    write_fs(&tty_node, 0, string_length(msg_save), (uint8_t*)msg_save);

    if (kfs_exists(filename)) { kfs_delete_file(filename); }
    kfs_create_file(filename, text_buffer);

    kfree(text_buffer); // Bersihkan memori dinamisnya
    
    tty_clear();
}