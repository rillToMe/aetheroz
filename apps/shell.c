#include "shell.h"
#include "userlib.h"
#include "zen.h"
#include "timer.h"   // timer_sleep_ms() — hardware-agnostic sleep
#include "task.h"

#include <stddef.h>
#include <stdint.h>

// sys_ping: weak fallback definition di sini agar link selalu berhasil.
// Jika apps/userlib.o (versi strong) juga di-link, definisi ini diabaikan.
// Syscall 41: RBX = host string pointer, return RTT ms atau -1.
__attribute__((weak))
int sys_ping(const char *host) {
    int64_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(41ULL), "b"((uint64_t)host));
    return (int)ret;
}

// Global: RSP yang disimpan SEBELUM shell memanggil app via CALL.
// Digunakan oleh sys_exec (syscall 33) untuk me-reset stack sehingga
// app baru bisa `ret` kembali ke shell dengan benar.
uint64_t g_shell_return_rsp = 0;

// extern void draw_png_image(const char* filename, int start_x, int start_y);

void kyuzen_fetch() {
    // Siapkan memori kosong untuk menampung nama CPU
    char cpu_name[49];
    get_cpu_string(cpu_name);

    print("\n");
    print("       /\\        OS   : Kyuzen OS (Ring 3)\n");
    print("      /  \\       Arch : x86_64 (Long Mode)\n");
    
    // Cetak nama CPU aslinya ke layar!
    print("     /____\\      CPU  : "); 
    print(cpu_name); 
    print("\n");
    
    print("    /      \\     GPU  : VGA Compatible (Text Mode)\n");
    print("   /        \\    Shell: kyuzen-shell\n");
    
    uint32_t used_mb = sys_used_ram() / 1024 / 1024;
    uint32_t total_mb = sys_total_ram() / 1024 / 1024;

    print("  /__________\\   RAM  : ");
    print_num(used_mb); print(" MB / "); print_num(total_mb); print(" MB\n\n");
}

void str_append(char* dest, const char* src) {
    while (*dest) dest++;
    while (*src) *dest++ = *src++;
    *dest = '\0';
}

void num_to_str(uint32_t num, char* str) {
    if (num == 0) { str[0] = '0'; str[1] = '\0'; return; }
    int i = 0; char temp[16];
    while (num > 0) { temp[i++] = (num % 10) + '0'; num /= 10; }
    int j = 0;
    while (i > 0) { str[j++] = temp[--i]; }
    str[j] = '\0';
}

int parse_uint(const char* str, uint32_t* out) {
    if (str == NULL || out == NULL) return 0;

    while (*str == ' ') str++;
    if (*str == '\0') return 0;

    uint32_t value = 0;
    while (*str != '\0') {
        if (*str < '0' || *str > '9') return 0;
        value = value * 10 + (uint32_t)(*str - '0');
        str++;
    }

    *out = value;
    return 1;
}

void user_shell() {
    char cmd_buffer[256];
    int cmd_index = 0;
    char key_buffer[2]; // <-- TAMBAHKAN BARIS INI UNTUK MENYELAMATKAN SHELL!
    
    // --- PROMPT DINAMIS BERDASARKAN USER ---
    char* prompt = "kyuzen> ";
    uint32_t uid = sys_get_uid(); // Tanya ke kernel siapa kita sekarang
    
    if (uid == 0) {
        prompt = "root@kyuzen> ";
    } else {
        prompt = "kyuzen@kyuzen> ";
    }
    // ---------------------------------------

    print("Selamat datang di Kyuzen OS.\n");
    print(prompt);

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
                        print("Perintah User Space:\n- help   : Info ini\n- clear  : Bersihkan layar\n- adduse  : Menambahkan User baru(khusus root)\n- logout  : Kembali ke halaman Login\n- echo   : Cetak teks\n- format : Format disk ke KZFS\n- ls     : Daftar file\n- zen    : Buka teks editor\n- baca   : Baca isi file\n- hapus  : Hapus file\n- fetch  : Tampilkan spek OS\n- sched  : Tampilkan status scheduler/CPU\n- refresh : Atur refresh rate (refresh 60 / 100 / 144)\n- shutdown   : Mematikan Os\n- Restart   : Merestart Os\n- Sleep   : Sleep Os\n- view   : Tampilkan gambar PNG\n- install_app : Instal app.bin\n- run    : Jalankan .bin\n- jam    : Lihat waktu sekarang\n- kalk   : Buka kalkulator\n- ping   : Ping host (ping google.com / ping 8.8.8.8)\n");
                    } 
                    else if (strcmp(command, "clear") == 0) { clear_screen(); }
                    else if (strcmp(command, "adduser") == 0) {
                        if (sys_get_uid() != 0) {
                            print("Error: Hanya 'root' yang diizinkan menambahkan user!\n");
                        } else if (argument == NULL) {
                            print("Penggunaan: adduser [nama_user_baru]\n");
                        } else {
                            print("Masukkan password untuk user baru: ");
                            char new_pass[32];
                            int p_idx = 0; char key_c;
                            
                            // Blocking event loop khusus untuk ngetik password
                            while (1) {
                                if (read_keyboard(&key_c, 1) > 0) {
                                    if (key_c == '\n') { new_pass[p_idx] = '\0'; print("\n"); break; }
                                    else if (key_c == '\b') { if (p_idx > 0) { print("\b"); p_idx--; } }
                                    else if (p_idx < 31) { new_pass[p_idx++] = key_c; print("*"); }
                                }
                                sys_yield();
                            }

                            // Sedot file users.sys yang lama ke RAM
                            uint32_t fsize = sys_file_size("users.sys");
                            char buffer[1024];
                            sys_read_file_to_buffer("users.sys", buffer);
                            buffer[fsize] = '\0';

                            // Hitung jumlah baris ('\n') untuk otomatis menentukan UID baru
                            int lines = 0;
                            for(int k=0; buffer[k]!='\0'; k++) { if (buffer[k] == '\n') lines++; }
                            uint32_t new_uid = 1000 + (lines - 1); // User pertama = 1000, kedua = 1001, dst.

                            // Susun baris teks baru
                            char uid_str[16];
                            num_to_str(new_uid, uid_str);
                            str_append(buffer, argument); 
                            str_append(buffer, ":");
                            str_append(buffer, new_pass);
                            str_append(buffer, ":");
                            str_append(buffer, uid_str);
                            str_append(buffer, "\n");

                            // Tulis Ulang Database!
                            int new_len = 0;
                            while(buffer[new_len]) new_len++;
                            
                            fs_delete("users.sys");
                            sys_create_file("users.sys", buffer, new_len);

                            print("Berhasil! User '"); print(argument); print("' berhasil ditambahkan.\n");
                        }
                    }
                    else if (strcmp(command, "logout") == 0) {
                        print("Keluar dari sesi...\n");
                        // Hancurkan loop shell agar fungsi berakhir!
                        break; 
                    }
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
                    // else if (strcmp(command, "view") == 0) {
                    //     if (argument != NULL) {
                    //         if (sys_file_exists(argument)) {
                    //             print("Menggambar PNG ke layar...\n");
                    //             // Panggil fungsi dari viewer.c, letakkan di koordinat X: 200, Y: 100
                    //             draw_png_image(argument, 200, 100);
                    //         } else {
                    //             print("Error: File gambar tidak ditemukan!\n");
                    //         }
                    //     } 
                    //     else { 
                    //         print("Penggunaan: view [nama_file.png]\n"); 
                    //     }
                    // }
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
                    else if (strcmp(command, "sched") == 0) {
                        scheduler_dump();
                    }
                    else if (strcmp(command, "refresh") == 0) {
                        if (argument == NULL) {
                            print("Refresh rate saat ini: ");
                            print_num(timer_get_refresh_rate());
                            print("Hz\nPilihan: 60, 100, 144\n");
                        } else {
                            uint32_t hz = 0;
                            if (!parse_uint(argument, &hz)) {
                                print("Penggunaan: refresh [60|100|144]\n");
                            } else if (timer_set_refresh_rate(hz) == 0) {
                                print("Refresh rate diubah ke ");
                                print_num(hz);
                                print("Hz\n");
                            } else {
                                print("Refresh rate tidak didukung. Pilihan: 60, 100, 144\n");
                            }
                        }
                    }
                    else if (strcmp(command, "shutdown") == 0) {
                        print("Mematikan Kyuzen OS...\n");
                        sys_shutdown();
                    }
                    // Perintah RESTART
                    else if (strcmp(command, "restart") == 0) {
                        print("Merestart Kyuzen OS...\n");
                        sys_reboot();
                    }
                    // Perintah SLEEP (Standby Mode)
                    else if (strcmp(command, "sleep") == 0) {
                        print("Sistem memasuki mode Sleep...\n");
                        print("Mata CPU ditutup. Tekan tombol apapun di keyboard untuk membangunkan.\n");
                        
                        // Jeda 2 detik agar user sempat membaca pesan
                        timer_sleep_ms(2000); // Jeda 2 detik


                        
                        clear_screen(); // Matikan/Bersihkan layar
                        
                        char dummy[2];
                        // Terus berputar sampai ada tombol keyboard yang ditekan
                        while (read_keyboard(dummy, 1) == 0) {
                            // Ini SANGAT PENTING: Sys_yield membuat beban CPU drop ke 0%!
                            // OS lu benar-benar "tidur" dan tidak membuang listrik/resource QEMU.
                            sys_yield(); 
                        }
                        
                        // Bangun!
                        clear_screen();
                        kyuzen_fetch(); // Tampilkan neofetch OS lu sebagai sapaan pagi
                    }
                    // Perintah PING — ICMP Echo Request via lwIP
                    else if (strcmp(command, "ping") == 0) {
                        if (argument == NULL) {
                            print("Penggunaan: ping [host]\n");
                            print("Contoh  : ping 10.0.2.2\n");
                            print("          ping 8.8.8.8\n");
                            print("          ping google.com\n");
                        } else {
                            // Output ping (Reply/Timeout/Statistik) dicetak oleh kernel
                            // via kprint() ke TTY — kita hanya perlu trigger syscall.
                            sys_ping(argument);
                        }
                    }
                    // else if (strcmp(command, "jam") == 0) {
                    //     // Launch clock.elf — jam digital real-time dengan GUI window
                    //     uint64_t entry = sys_load_elf("clock.elf");
                    //     if (entry != 0) {
                    //         void (*run)(void) = (void (*)(void))entry;
                    //         // Simpan RSP sebelum CALL agar sys_exec bisa reset stack
                    //         __asm__ volatile("mov %%rsp, %0" : "=m"(g_shell_return_rsp) :: "memory");
                    //         run();
                    //     } else {
                    //         print("[jam] Gagal memuat clock.elf dari disk.\n");
                    //     }
                    // }
                    // else if (strcmp(command, "kalk") == 0) {
                    //     // Launch calc.elf — kalkulator GUI
                    //     uint64_t entry = sys_load_elf("calc.elf");
                    //     if (entry != 0) {
                    //         void (*run)(void) = (void (*)(void))entry;
                    //         __asm__ volatile("mov %%rsp, %0" : "=m"(g_shell_return_rsp) :: "memory");
                    //         run();
                    //     } else {
                    //         print("[kalk] Gagal memuat calc.elf dari disk.\n");
                    //     }
                    // }
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
                        uint64_t app_entry = sys_load_elf(elf_filename);

                        if (app_entry != 0) {
                            clear_screen();
                            // 3. Lompat dan jalankan aplikasinya
                            void (*run_app)(void) = (void (*)(void))app_entry;
                            // Simpan RSP sebelum CALL agar sys_exec bisa reset stack
                            __asm__ volatile("mov %%rsp, %0" : "=m"(g_shell_return_rsp) :: "memory");
                            run_app();
                            // App returned (baik normal, atau via sys_exit longjmp)
                            print("\n");
                        } else {
                            // 4. File tidak ada
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
