#include "shell.h"
#include "userlib.h"
#include "zen.h"
#include <stddef.h>

extern void draw_png_image(const char* filename, int start_x, int start_y);

void kyuzen_fetch() {
    // Siapkan memori kosong untuk menampung nama CPU
    char cpu_name[49];
    get_cpu_string(cpu_name);

    print("\n");
    print("       /\\        OS   : Kyuzen OS (Ring 3)\n");
    print("      /  \\       Arch : x86 32-bit (Protected Mode)\n");
    
    // Cetak nama CPU aslinya ke layar!
    print("     /____\\      CPU  : "); 
    print(cpu_name); 
    print("\n");
    
    print("    /      \\     GPU  : VGA Compatible (Text Mode)\n");
    print("   /        \\    Shell: kyuzen-shell\n");
    
    uint32_t used_mb = sys_used_ram() / 1024 / 1024;
    uint32_t total_mb = sys_total_ram() / 1024 / 1024;
    
    print("  /__________\\   RAM  : ");
    print_num(used_mb); print(" MB / "); print_num(total_mb); print(" MB\n");
    print("                 Up   : ");
    print_num(sys_uptime()); print(" detik\n\n");
}

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

                    // --- DAFTAR PERINTAH ---
                    if (strcmp(command, "help") == 0) {
                        print("Perintah User Space:\n- help   : Info ini\n- clear  : Bersihkan layar\n- echo   : Cetak teks\n- format : Format disk ke KZFS\n- ls     : Daftar file\n- zen    : Buka teks editor\n- baca   : Baca isi file\n- hapus  : Hapus file\n- fetch  : Tampilkan spek OS\n- view   : Tampilkan gambar PNG\n install_app : Instal app.bin\n- run    : Jalankan .bin\n- - jam    : Lihat waktu sekarang\n");
                    } 
                    else if (strcmp(command, "clear") == 0) { clear_screen(); }
                    else if (strcmp(command, "echo") == 0) {
                        if (argument != NULL) { print(argument); print("\n"); } 
                        else { print("Penggunaan: echo [teks_bebas]\n"); }
                    }
                    else if (strcmp(command, "format") == 0) { fs_format(); }
                    else if (strcmp(command, "ls") == 0) { fs_list(); }
                    else if (strcmp(command, "zen") == 0) {
                        if (argument != NULL) { zen_main(argument); clear_screen(); } 
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
                    // --- TAMBAHKAN PERINTAH VIEW DI SINI ---
                    else if (strcmp(command, "view") == 0) {
                        if (argument != NULL) {
                            if (sys_file_exists(argument)) {
                                print("Menggambar PNG ke layar...\n");
                                // Panggil fungsi dari viewer.c, letakkan di koordinat X: 200, Y: 100
                                draw_png_image(argument, 200, 100);
                            } else {
                                print("Error: File gambar tidak ditemukan!\n");
                            }
                        } 
                        else { 
                            print("Penggunaan: view [nama_file.png]\n"); 
                        }
                    }
                    // ---------------------------------------
                    else if (strcmp(command, "install_app") == 0) {
                        char dummy_bin[] = {
                            0xB8, 0x01, 0x00, 0x00, 0x00, 0xBB, 0x0D, 0x00, 0x80, 0x00, 0xCD, 0x80, 0xC3,
                            'H', 'a', 'l', 'o', ' ', 'd', 'a', 'r', 'i', ' ', 'B', 'I', 'N', 'A', 'R', 'Y', '!', '\n', '\0'
                        };
                        sys_create_file("app.bin", dummy_bin, 33);
                        print("Aplikasi app.bin berhasil di-install ke Hard Disk!\n");
                    }
                    else if (strcmp(command, "fetch") == 0) { kyuzen_fetch(); 
                    }
                    else if (strcmp(command, "jam") == 0) {
                        uint32_t waktu[6];
                        sys_get_time(waktu);

                        print("Waktu Dunia Nyata (WIB) : ");
                        
                        // Format Tanggal: YYYY-MM-DD
                        print_num(waktu[0]); print("-");
                        if(waktu[1] < 10) print("0"); print_num(waktu[1]); print("-");
                        if(waktu[2] < 10) print("0"); print_num(waktu[2]); print(" ");
                        
                        // Format Jam: HH:MM:SS
                        if(waktu[3] < 10) print("0"); print_num(waktu[3]); print(":");
                        if(waktu[4] < 10) print("0"); print_num(waktu[4]); print(":");
                        if(waktu[5] < 10) print("0"); print_num(waktu[5]); print("\n");
                    }
                    else { 
                        // 1. Siapkan wadah teks untuk menyisipkan ".elf"
                        char elf_filename[32];
                        int i = 0;
                        
                        // Salin nama perintah yang diketik user
                        while (command[i] != '\0' && i < 27) {
                            elf_filename[i] = command[i];
                            i++;
                        }
                        
                        // Tambahkan ekstensi ".elf" secara diam-diam
                        elf_filename[i++] = '.';
                        elf_filename[i++] = 'e';
                        elf_filename[i++] = 'l';
                        elf_filename[i++] = 'f';
                        elf_filename[i] = '\0';

                        // 2. Minta Kernel mencari dan memuat file tersebut
                        uint32_t app_entry = sys_load_elf(elf_filename);
                        
                        if (app_entry != 0) {
                            clear_screen();
                            // 3. Jika ketemu, LOMPAT dan jalankan aplikasinya!
                            void (*run_app)() = (void (*)())app_entry;
                            run_app(); 
                                       
                            // Opsional: Beri jarak baris setelah aplikasi selesai
                            print("\n"); 

                        } else {
                            // 4. Jika file memang tidak ada, tampilkan error aslinya
                            print("Perintah tidak dikenali: ");
                            print(command);
                            print("\n");
                        }
                    }
                    // -------------------------------------------
                }
                cmd_index = 0;
                print(prompt);
            }
            else if (c == '\b') {
                if (cmd_index > 0) { print("\b"); cmd_index--; }
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