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

// ==============================================================
// 🛡️ USER SPACE (RING 3) STANDARD LIBRARY 🛡️
// ==============================================================

void print(char* text) {
    __asm__ volatile("mov $1, %%eax \n mov %0, %%ebx \n int $0x80" : : "r"(text) : "%eax", "%ebx");
}

void clear_screen() {
    __asm__ volatile("mov $2, %%eax \n int $0x80" : : : "%eax");
}

uint32_t read_keyboard(char* buffer, uint32_t size) {
    uint32_t bytes_read;
    __asm__ volatile(
        "mov $4, %%eax \n mov %1, %%ebx \n mov %2, %%ecx \n int $0x80 \n mov %%eax, %0 \n"
        : "=r"(bytes_read) : "r"(buffer), "r"(size) : "%eax", "%ebx", "%ecx"
    );
    return bytes_read;
}

void sys_yield() {
    __asm__ volatile("mov $5, %%eax \n int $0x80" : : : "%eax");
}

// --- PEMBUNGKUS FILE SYSTEM BARU ---
void fs_format() { __asm__ volatile("mov $6, %%eax \n int $0x80" : : : "%eax"); }
void fs_list() { __asm__ volatile("mov $7, %%eax \n int $0x80" : : : "%eax"); }
void fs_read(char* filename) { __asm__ volatile("mov $8, %%eax \n mov %0, %%ebx \n int $0x80" : : "r"(filename) : "%eax", "%ebx"); }
void fs_delete(char* filename) { __asm__ volatile("mov $9, %%eax \n mov %0, %%ebx \n int $0x80" : : "r"(filename) : "%eax", "%ebx"); }

// --- PEMBUNGKUS MEMORI & FS LANJUTAN UNTUK ZEN ---

void* sys_alloc(uint32_t size) {
    uint32_t ret;
    // "a"(11) isi EAX dengan 11. "b"(size) isi EBX dengan size.
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(11), "b"(size));
    return (void*)ret;
}

void sys_free(void* ptr) {
    __asm__ volatile("int $0x80" : : "a"(12), "b"(ptr));
}

void* sys_realloc(void* ptr, uint32_t old_size, uint32_t new_size) {
    uint32_t ret;
    // Clang akan secara cerdas mengatur EAX=13, EBX=ptr, ECX=old_size, EDX=new_size
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(13), "b"(ptr), "c"(old_size), "d"(new_size));
    return (void*)ret;
}

int sys_file_exists(char* filename) {
    uint32_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(14), "b"(filename));
    return ret;
}

uint32_t sys_file_size(char* filename) {
    uint32_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(15), "b"(filename));
    return ret;
}

int sys_read_file_to_buffer(char* filename, char* buffer) {
    uint32_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(16), "b"(filename), "c"(buffer));
    return ret;
}

int sys_create_file(char* filename, char* data, uint32_t size) {
    uint32_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(17), "b"(filename), "c"(data), "d"(size));
    return ret;
}

// --- PEMBUNGKUS SYSTEM STATS BARU ---
uint32_t sys_uptime() {
    uint32_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(18)); return ret;
}
uint32_t sys_total_ram() {
    uint32_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(19)); return ret;
}
uint32_t sys_used_ram() {
    uint32_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(20)); return ret;
}

// Fungsi manual untuk mencetak angka Integer (Karena kita tidak punya printf)
void print_num(uint32_t num) {
    if (num == 0) { print("0"); return; }
    char buf[16];
    int i = 14;
    buf[15] = '\0';
    while (num > 0 && i >= 0) {
        buf[i--] = (num % 10) + '0';
        num /= 10;
    }
    print(&buf[i + 1]);
}

// --- APLIKASI KYUZEN FETCH ---
void kyuzen_fetch() {
    print("\n");
    print("       /\\        OS   : Kyuzen OS (Ring 3)\n");
    print("      /  \\       Arch : x86 32-bit (Protected Mode)\n");
    print("     /____\\      CPU  : Intel Core i5-12450HX (Simulated)\n");
    print("    /      \\     GPU  : NVIDIA GeForce RTX 3060 (Simulated)\n");
    print("   /        \\    Shell: kyuzen-shell\n");
    
    // Konversi dari Byte ke Megabyte (Bagi 1024, lalu 1024 lagi)
    uint32_t used_mb = sys_used_ram() / 1024 / 1024;
    uint32_t total_mb = sys_total_ram() / 1024 / 1024;
    
    print("  /__________\\   RAM  : ");
    print_num(used_mb); print(" MB / "); print_num(total_mb); print(" MB\n");
    
    print("                 Up   : ");
    print_num(sys_uptime()); print(" detik\n\n");
}


extern int strcmp(const char *s1, const char *s2); 
extern void zen_main(char* filename); // Impor zen_main langsung ke Ring 3!

// --- APLIKASI SHELL RING 3 ---
void user_shell() {
    clear_screen();
    print("========================================\n");
    print("   Kyuzen OS - User Space Shell (Ring 3)\n");
    print("========================================\n");

    char* prompt = "kyuzen> ";
    print(prompt);

    char key_buffer[1];
    char cmd_buffer[256];      
    uint32_t cmd_index = 0;    
    
    while (1) {
        uint32_t bytes_read = read_keyboard(key_buffer, 1);
        
        if (bytes_read > 0) {
            char c = key_buffer[0];
            
            if (c == '\n') {
                print("\n");
                cmd_buffer[cmd_index] = '\0'; 
                
                if (cmd_index > 0) {
                    char* command = cmd_buffer;
                    char* argument = NULL;

                    for (uint32_t i = 0; i < cmd_index; i++) {
                        if (cmd_buffer[i] == ' ') {
                            cmd_buffer[i] = '\0';          
                            argument = &cmd_buffer[i + 1]; 
                            break;                         
                        }
                    }

                    // --- DAFTAR PERINTAH LENGKAP ---
                    if (strcmp(command, "help") == 0) {
                        print("Perintah User Space:\n- help   : Info ini\n- clear  : Bersihkan layar\n- echo   : Cetak teks\n- format : Format disk ke KZFS\n- ls     : Daftar file\n- zen    : Buka teks editor\n- baca   : Baca isi file\n- hapus  : Hapus file\n - fetch  : Tampilkan spek OS\n");
                    } 
                    else if (strcmp(command, "clear") == 0) {
                        clear_screen(); 
                    }
                    else if (strcmp(command, "echo") == 0) {
                        if (argument != NULL) { print(argument); print("\n"); } 
                        else { print("Penggunaan: echo [teks_bebas]\n"); }
                    }
                    else if (strcmp(command, "format") == 0) {
                        fs_format();
                    }
                    else if (strcmp(command, "ls") == 0) {
                        fs_list();
                    }
                    else if (strcmp(command, "zen") == 0) {
                        if (argument != NULL) { 
                            zen_main(argument); // Panggil sebagai fungsi User Space biasa!
                            clear_screen(); 
                        } 
                        else { print("Penggunaan: zen [nama_file]\n"); }
                    }
                    else if (strcmp(command, "baca") == 0) {
                        if (argument != NULL) { fs_read(argument); } 
                        else { print("Penggunaan: baca [nama_file]\n"); }
                    }
                    else if (strcmp(command, "hapus") == 0) {
                        if (argument != NULL) { fs_delete(argument); } 
                        else { print("Penggunaan: hapus [nama_file]\n"); }
                    }
                    else if (strcmp(command, "install_app") == 0) {
                        // Ini adalah kode mesin x86 rakitan murni. 
                        // Logikanya: Panggil Syscall 1 (Print), lalu Return ke Shell.
                        char dummy_bin[] = {
                            0xB8, 0x01, 0x00, 0x00, 0x00, // mov eax, 1 
                            0xBB, 0x0D, 0x00, 0x80, 0x00, // mov ebx, 0x80000D (Alamat Teks di RAM)
                            0xCD, 0x80,                   // int 0x80
                            0xC3,                         // ret (Kembali ke Shell)
                            'H', 'a', 'l', 'o', ' ', 'd', 'a', 'r', 'i', ' ', 'B', 'I', 'N', 'A', 'R', 'Y', '!', '\n', '\0'
                        };
                        sys_create_file("app.bin", dummy_bin, 33); // 33 adalah total ukuran byte-nya
                        print("Aplikasi app.bin berhasil di-install ke Hard Disk!\n");
                    }
                    else if (strcmp(command, "run") == 0) {
                        if (argument != NULL) {
                            if (sys_file_exists(argument)) {
                                print("Memuat aplikasi ke RAM 0x800000...\n");
                                
                                // 1. BACA RAW BINARY KE LAHAN APLIKASI
                                sys_read_file_to_buffer(argument, (char*)0x800000);
                                
                                // 2. SULAP ALAMAT MEMORI MENJADI FUNGSI!
                                void (*external_app)() = (void*)0x800000;
                                
                                // 3. EKSEKUSI! CPU melompat keluar dari Shell.
                                external_app();
                                
                                print("[Aplikasi Selesai Dieksekusi]\n");
                            } else {
                                print("Error: File tidak ditemukan.\n");
                            }
                        } else {
                            print("Penggunaan: run [nama_file.bin]\n");
                        }
                    }
                    else if (strcmp(command, "fetch") == 0) {
                        kyuzen_fetch();
                    }
                    else {
                        print("Perintah tidak dikenali.\n");
                    }
                }
                cmd_index = 0;
                print(prompt);
            } 
            else if (c == '\b') {
                if (cmd_index > 0) {
                    print("\b"); 
                    cmd_index--;
                }
            } 
            else {
                if (cmd_index < 255) {
                    cmd_buffer[cmd_index] = c;
                    cmd_index++;
                    char char_str[2] = {c, '\0'};
                    print(char_str);
                }
            }
        }
        sys_yield();
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

    switch_to_user_mode(user_shell);
    

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