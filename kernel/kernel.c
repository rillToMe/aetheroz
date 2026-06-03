#include <stdint.h>
#include "fs.h"
#include "tty.h"
#include "pmm.h"
#include "paging.h"
#include "heap.h"
#include "string.h"
#include "ata.h"
#include "kyuzenfs.h"
#include "zen.h"
#include "task.h"
#include "timer.h"

extern void init_gdt();
extern void init_idt();
extern void pic_remap();
extern fs_node_t tty_node;
extern void set_kernel_stack(uint32_t stack);

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

// PROGRAM KEDUA: Berjalan di latar belakang tanpa henti
void background_task() {
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
    int counter = 0;
    char spinner[] = {'|', '/', '-', '\\'};
    
    while(1) {
        // Tulis animasi baling-baling langsung ke pojok kanan atas layar
        vga[78] = (uint16_t)spinner[counter % 4] | (0x0E << 8); // Warna kuning
        counter++;
        
        // Jeda waktu supaya baling-baling tidak muter seperti helikopter rusak
        for(volatile int i = 0; i < 500000; i++); 
        
        // Oper kembali CPU ke Shell!
        yield(); 
    }
}

// --- PROGRAM RING 3 (USER SPACE) ---
void my_first_app() {
    char* msg = "\n[USER SPACE] Berhasil! Saya berjalan tanpa hak akses Kernel!\n";
    
    // Panggil Syscall 1 (Print)
    __asm__ volatile(
        "mov $1, %%eax \n"
        "mov %0, %%ebx \n"
        "int $0x80     \n"
        : : "r"(msg) : "%eax", "%ebx"
    );
    
    // Aplikasi User Space belum punya Syscall "Exit" (Tutup Program).
    // Jadi untuk sementara, kita kurung dia di loop tak terhingga.
    while(1) {
        // Jangan taruh hlt di sini! Ring 3 dilarang memakai instruksi hlt.
    }
}

// --- LOGIKA LOMPATAN RING 3 ---
void switch_to_user_mode(void (*user_func)()) {
    
    // 1. Stack untuk aplikasi
    uint32_t user_stack = (uint32_t)kmalloc(4096) + 4096;

    // 2. STACK UNTUK KERNEL (PARASUT)!
    // Jika aplikasi memanggil int 0x80, CPU akan lompat ke stack ini!
    uint32_t kernel_landing_stack = (uint32_t)kmalloc(4096) + 4096;
    set_kernel_stack(kernel_landing_stack);

    // 3. Lakukan tipuan iret
    __asm__ volatile(
        "cli \n"                 
        "mov $0x23, %%ax \n" 
        "mov %%ax, %%ds \n"
        "mov %%ax, %%es \n"
        "mov %%ax, %%fs \n"
        "mov %%ax, %%gs \n"
        
        "pushl $0x23 \n"         
        "pushl %0 \n"            
        "pushfl \n"              
        "popl %%eax \n"
        "orl $0x200, %%eax \n"   
        "pushl %%eax \n"         
        "pushl $0x1B \n"         
        "pushl %1 \n"            
        
        "iret \n"                
        :
        : "r"(user_stack), "r"(user_func)
        : "%eax"
    );
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
    
    // --- UJI COBA HARD DISK & HEAP ---
    // 1. Sewa RAM 512 byte (ukuran 1 sektor) menggunakan Heap
    uint8_t* disk_buffer = (uint8_t*)kmalloc(512);
    
    if (disk_buffer != NULL) {
        // Bersihkan memori dulu supaya tidak ada teks sampah
        memset(disk_buffer, 0, 512); 
        
        // 2. Suruh Hard Disk membaca Sektor 0 dan masukkan datanya ke RAM
        ata_read_sector(0, disk_buffer);
        
        // 3. Cetak isi Hard Disk ke layar!
        write_fs(tty0, 0, string_length((char*)disk_buffer), disk_buffer);
        write_fs(tty0, 0, 1, (uint8_t*)"\n"); // Kasih enter
        
        // 4. Kembalikan RAM
        kfree(disk_buffer);
    }
    
    char* msg3 = "\nSistem File Unix-style berjalan! Ketik sesuatu...\n";
    write_fs(tty0, 0, string_length(msg3), (uint8_t*)msg3);

    pic_remap();
    __asm__ volatile("sti");

    kfs_init();

    tasking_init();
    init_timer(100);
    create_task(background_task);

    switch_to_user_mode(my_first_app);
    

    // --- INTERACTIVE SHELL LOOP ---
    char* prompt = "kyuzen> ";
    write_fs(tty0, 0, string_length(prompt), (uint8_t*)prompt);

    uint8_t key_buffer[1];
    char cmd_buffer[256];      // Tempat menyimpan teks yang diketik user
    uint32_t cmd_index = 0;    // Pelacak posisi huruf saat ini
    
    while (1) {
        uint32_t bytes_read = read_fs(tty0, 0, 1, key_buffer);
        
        if (bytes_read > 0) {
            char c = key_buffer[0];
            
            if (c == '\n') {
                // 1. User menekan Enter. Cetak enter ke layar.
                write_fs(tty0, 0, 1, (uint8_t*)"\n");
                
                // 2. Kunci string-nya dengan Null-Terminator agar menjadi teks C yang valid
                cmd_buffer[cmd_index] = '\0'; 
                
                // 3. LOGIKA EKSEKUSI PERINTAH (Command Parser)
                // 3. LOGIKA EKSEKUSI PERINTAH (Command Parser)
                if (cmd_index > 0) {
                    
                    // --- TRIK MEMBELAH STRING ---
                    char* command = cmd_buffer;
                    char* argument = NULL;

                    // Cari letak spasi pertama
                    for (uint32_t i = 0; i < cmd_index; i++) {
                        if (cmd_buffer[i] == ' ') {
                            cmd_buffer[i] = '\0';          // Potong string pertama (Command) di sini!
                            argument = &cmd_buffer[i + 1]; // Sisa string di sebelahnya jadi Argument
                            break;                         // Hentikan pencarian spasi
                        }
                    }

                    // --- DAFTAR PERINTAH ---
                    if (strcmp(command, "help") == 0) {
                        char* help_msg = "Perintah:\n- help   : Info ini\n- clear  : Bersihkan layar\n- echo   : Cetak teks\n- format : Format disk ke KZFS\n- ls     : Daftar file\n- buat   : Bikin file dummy\n- baca   : Baca isi file\n- hapus  : Hapus file\n";
                        write_fs(tty0, 0, string_length(help_msg), (uint8_t*)help_msg);
                    } 
                    else if (strcmp(command, "clear") == 0) {
                        tty_clear(); 
                    }
                    else if (strcmp(command, "echo") == 0) {
                        if (argument != NULL) {
                            write_fs(tty0, 0, string_length(argument), (uint8_t*)argument);
                            write_fs(tty0, 0, 1, (uint8_t*)"\n");
                        } else {
                            char* err_msg = "Penggunaan: echo [teks_bebas]\n";
                            write_fs(tty0, 0, string_length(err_msg), (uint8_t*)err_msg);
                        }
                    }
                    // -- COMMAND KYUZEN FS --
                    else if (strcmp(command, "format") == 0) {
                        kfs_format();
                    }
                    else if (strcmp(command, "ls") == 0) {
                        kfs_list_files();
                    }
                    else if (strcmp(command, "zen") == 0) {
                        if (argument != NULL) {
                            zen_main(argument); // Panggil aplikasinya!
                        } else {
                            char* err_msg = "Penggunaan: zen [nama_file]\n";
                            write_fs(tty0, 0, string_length(err_msg), (uint8_t*)err_msg);
                        }
                    }
                    else if (strcmp(command, "baca") == 0) {
                        if (argument != NULL) {
                            kfs_read_file(argument);
                        } else {
                            char* err_msg = "Penggunaan: baca [nama_file]\n";
                            write_fs(tty0, 0, string_length(err_msg), (uint8_t*)err_msg);
                        }
                    }
                    else if (strcmp(command, "hapus") == 0) {
                        if (argument != NULL) {
                            kfs_delete_file(argument);
                        } else {
                            char* err_msg = "Penggunaan: hapus [nama_file]\n";
                            write_fs(tty0, 0, string_length(err_msg), (uint8_t*)err_msg);
                        }
                    }
                    // else if (strcmp(command, "buatpanjang") == 0) {
                    //     if (argument != NULL) {
                    //         // Bikin string berukuran 1200 byte (Bakal makan 3 sektor: 512 + 512 + 176)
                    //         char* teks_raksasa = (char*)kmalloc(1300);
                    //         memset(teks_raksasa, 0, 1300);
                            
                    //         // Isi penuh dengan teks berulang-ulang!
                    //         for(int k=0; k < 40; k++) {
                    //             // memcpy dari string buatanmu. 40 x 30 karakter = 1200 karakter
                    //             memcpy(teks_raksasa + (k * 30), "Ini adalah teks Multi-Sector! ", 30);
                    //         }
                            
                    //         kfs_create_file(argument, teks_raksasa);
                    //         kfree(teks_raksasa);
                    //     } else {
                    //         char* err_msg = "Penggunaan: buatpanjang [nama_file]\n";
                    //         write_fs(tty0, 0, string_length(err_msg), (uint8_t*)err_msg);
                    //     }
                    // }
                    else {
                        char* err_msg = "Perintah tidak dikenali.\n";
                        write_fs(tty0, 0, string_length(err_msg), (uint8_t*)err_msg);
                    }
                }

                // 4. Reset ingatan buffer dan cetak prompt baru
                cmd_index = 0;
                write_fs(tty0, 0, string_length(prompt), (uint8_t*)prompt);
            } 
            else if (c == '\b') {
                // User menekan Backspace, hapus huruf dari layar dan kurangi index
                if (cmd_index > 0) {
                    write_fs(tty0, 0, 1, key_buffer); 
                    cmd_index--;
                }
            } 
            else {
                // Huruf biasa, simpan ke memori dan cetak ke layar
                if (cmd_index < 255) {
                    cmd_buffer[cmd_index] = c;
                    cmd_index++;
                    write_fs(tty0, 0, 1, key_buffer);
                }
            }
        }
        
        __asm__ volatile("hlt");
    }
}