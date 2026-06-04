#include "userlib.h"

// Impor fungsi shell utama untuk dipanggil jika login sukses
extern void user_shell(); 

// Fungsi pembantu untuk membandingkan string (karena kita tidak pakai libc bawaan)
int str_match(const char* s1, const char* s2) {
    while (*s1 != '\0' && *s1 == *s2) {
        s1++; s2++;
    }
    return (*s1 == *s2);
}

void user_login() {
    char username[32];
    char password[32];
    char c;
    int u_idx, p_idx;

    while (1) {
        clear_screen();
        print("\n");
        print("  ==========================================\n");
        print("         SISTEM KEAMANAN KYUZEN OS\n");
        print("  ==========================================\n\n");

        print("  Username : ");
        u_idx = 0;
        while (1) {
            if (read_keyboard(&c, 1) > 0) {
                if (c == '\n') {
                    username[u_idx] = '\0';
                    break;
                } else if (c == '\b') {
                    if (u_idx > 0) { print("\b"); u_idx--; }
                } else if (u_idx < 31) {
                    username[u_idx++] = c;
                    char s[2] = {c, '\0'};
                    print(s); // Cetak huruf yang diketik
                }
            }
            sys_yield(); // Beri napas ke CPU
        }

        print("\n  Password : ");
        p_idx = 0;
        while (1) {
            if (read_keyboard(&c, 1) > 0) {
                if (c == '\n') {
                    password[p_idx] = '\0';
                    break;
                } else if (c == '\b') {
                    if (p_idx > 0) { print("\b"); p_idx--; }
                } else if (p_idx < 31) {
                    password[p_idx++] = c;
                    print("*"); // Sembunyikan dengan bintang!
                }
            }
            sys_yield();
        }

        print("\n\n  Mencocokkan data...\n");

        // --- HARDCODE DATABASE USER SEMENTARA ---
        int auth_ok = 0;
        if (str_match(username, "root") && str_match(password, "toor")) {
            auth_ok = 1;
            print("  [SUCCESS] Akses Diberikan: Superuser (Root)\n");
        } else if (str_match(username, "kyuzen") && str_match(password, "123")) {
            auth_ok = 1;
            print("  [SUCCESS] Akses Diberikan: Pengguna Standar\n");
        }

        if (auth_ok) {
            // Beri jeda sedikit agar user bisa membaca pesan sukses
            for(int i = 0; i < 500000; i++) sys_yield();
            
            clear_screen();
            user_shell(); // BUKA GERBANG SHELL!
            break;
        } else {
            print("  [DENIED] Akses Ditolak: Username atau Password salah!\n");
            // Jeda error sebelum mengulang loop
            for(int i = 0; i < 1000000; i++) sys_yield();
        }
    }
}