#include "userlib.h"
#define FONT8x16_IMPLEMENTATION
#include "font8x16.h"

int win_id = -1;
uint32_t* my_canvas = 0;
int win_w = 400, win_h = 300;

// Variabel Pelacak File yang Sedang Diklik
int selected_file = -1; 

void draw_rect_local(int start_x, int start_y, int width, int height, uint32_t color) {
    uint32_t solid_color = color | 0xFF000000; 
    for(int y = start_y; y < start_y + height; y++) {
        for(int x = start_x; x < start_x + width; x++) {
            if(x >= 0 && x < win_w && y >= 0 && y < win_h) {
                my_canvas[(y * win_w) + x] = solid_color;
            }
        }
    }
}

void draw_char_local(char c, int x, int y, uint32_t color) {
    if (c < 0 || c > 127) return;
    const unsigned char* bitmap = font8x16[(int)c];
    uint32_t solid_color = color | 0xFF000000;

    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            if (bitmap[row] & (0x80 >> col)) {
                int px = x + col;
                int py = y + row;
                if (px >= 0 && px < win_w && py >= 0 && py < win_h) {
                    my_canvas[(py * win_w) + px] = solid_color;
                }
            }
        }
    }
}

void draw_string_local(const char* str, int x, int y, uint32_t color) {
    int cx = x, cy = y;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') { cy += 16; cx = x; }
        else { draw_char_local(str[i], cx, cy, color); cx += 8; }
    }
}

// --- FUNGSI RENDER UI (Dipanggil setiap kali ada perubahan) ---
void render_ui(file_info_t* files, int total_files) {
    // 1. Gambar Dasar Jendela
    draw_rect_local(0, 0, win_w, win_h, 0xFFFFFF);     
    draw_rect_local(0, 0, win_w, 30, 0x111111);        
    draw_rect_local(win_w - 40, 0, 40, 30, 0xE53935);  

    draw_string_local("FILE MANAGER", 10, 7, 0xFFFFFF);
    draw_string_local("X", win_w - 24, 7, 0xFFFFFF);

    draw_string_local("Isi Penyimpanan KZFS:", 15, 45, 0x000000);
    
    // 2. Render Daftar File dengan Deteksi Sorotan
    int start_y = 70;
    for (int i = 0; i < total_files; i++) {
        if (i == selected_file) {
            // Blok sorotan tajam dan tegas untuk file yang dipilih
            draw_rect_local(10, start_y - 2, win_w - 20, 20, 0x111111);
            draw_string_local(">", 15, start_y, 0xFFFFFF); 
            draw_string_local(files[i].filename, 30, start_y, 0xFFFFFF);
        } else {
            draw_string_local(">", 15, start_y, 0x555555);
            draw_string_local(files[i].filename, 30, start_y, 0x000000);
        }
        start_y += 20;
    }

    // 3. Status Bar Minimalis di Bawah
    draw_rect_local(0, win_h - 30, win_w, 30, 0xEEEEEE);
    if (selected_file != -1) {
        draw_string_local("Terpilih: ", 10, win_h - 22, 0x555555);
        draw_string_local(files[selected_file].filename, 90, win_h - 22, 0x000000); 

        // --- TAMBAHKAN TOMBOL BUKA (Cyan CMYK: 0x00AEEF) ---
        draw_rect_local(win_w - 70, win_h - 26, 60, 22, 0x00AEEF);
        draw_string_local("BUKA", win_w - 55, win_h - 23, 0xFFFFFF);
        // ---------------------------------------------------
    }

    // 4. Tumpahkan Kanvas Lokal ke Kernel
    sys_update_window(win_id, my_canvas);
}

void main() {
    win_id = sys_create_window(200, 150, win_w, win_h);
    if (win_id < 0) return;

    my_canvas = (uint32_t*) sys_alloc(win_w * win_h * 4);

    file_info_t files[16];
    int total_files = sys_get_file_list(files, 16);

    // Render layar untuk pertama kalinya
    render_ui(files, total_files);

    kyuzen_event_t event;
    while (1) {
        if (sys_get_event(&event)) {
            static int mouse_x = 0;
            static int mouse_y = 0;
            
            if (event.type == 2) {
                mouse_x = event.param1;
                mouse_y = event.param2;
            }

            if (event.type == 3 && event.param1 == 0 && event.param2 == 1) {
                
                // 1. DETEKSI KLIK: Tombol Merah (Close)
                int btn_x_start = 200 + (win_w - 40);
                int btn_x_end = 200 + win_w;
                int btn_y_start = 150 + 0;
                int btn_y_end = 150 + 30;

                if (mouse_x >= btn_x_start && mouse_x <= btn_x_end &&
                    mouse_y >= btn_y_start && mouse_y <= btn_y_end) {
                    break; 
                }

                // 2. DETEKSI KLIK: Daftar File
                int list_start_y = 150 + 70; 
                int list_end_y = list_start_y + (total_files * 20);
                int list_start_x = 200 + 10;
                int list_end_x = 200 + win_w - 10;

                if (mouse_x >= list_start_x && mouse_x <= list_end_x &&
                    mouse_y >= list_start_y && mouse_y < list_end_y) {
                    
                    int clicked_index = (mouse_y - list_start_y) / 20;
                    if (clicked_index >= 0 && clicked_index < total_files) {
                        selected_file = clicked_index;
                        render_ui(files, total_files); 
                    }
                } // <-- TUTUP KURUNG DAFTAR FILE HARUS DI SINI!

                // 3. DETEKSI KLIK: TOMBOL BUKA (CYAN)
                if (selected_file != -1) {
                    int buka_x_start = 200 + (win_w - 70);
                    int buka_x_end = 200 + win_w - 10;
                    int buka_y_start = 150 + (win_h - 26);
                    int buka_y_end = 150 + (win_h - 4);

                    if (mouse_x >= buka_x_start && mouse_x <= buka_x_end &&
                        mouse_y >= buka_y_start && mouse_y <= buka_y_end) {
                        
                        char* target_file = files[selected_file].filename;
                        
                        // Cek ekstensi file
                        int len = 0; while(target_file[len]) len++;
                        
                        if (len > 4 && target_file[len-4] == '.' && target_file[len-3] == 'e' && 
                            target_file[len-2] == 'l' && target_file[len-1] == 'f') {
                            
                            // --- LOGIKA BUKA .ELF ---
                            sys_destroy_window(win_id);
                            sys_free(my_canvas);
                            
                            uint32_t app_entry = sys_load_elf(target_file);
                            if (app_entry != 0) {
                                void (*run_app)() = (void (*)())app_entry;
                                run_app();
                            }
                            return; // Akhiri eksekusi File Manager
                        } 
                        else if (len > 4 && target_file[len-4] == '.' && target_file[len-3] == 'p' && 
                                 target_file[len-2] == 'n' && target_file[len-1] == 'g') {
                            
                            // --- LOGIKA BUKA .PNG (JEMBATAN KE VIEWER.ELF) ---
                            // 1. Tulis nama file ke 'view.tmp'
                            if (sys_file_exists("view.tmp")) fs_delete("view.tmp");
                            sys_create_file("view.tmp", target_file, len);
                            
                            // 2. Hancurkan GUI File Manager sebelum berpindah
                            sys_destroy_window(win_id);
                            sys_free(my_canvas);
                            
                            // 3. Panggil Aplikasi Viewer!
                            uint32_t app_entry = sys_load_elf("viewer.elf");
                            if (app_entry != 0) {
                                void (*run_app)() = (void (*)())app_entry;
                                run_app();
                            }
                            return; // Akhiri eksekusi File Manager
                        } 
                        else {
                            // Untuk file format lain (.sys, .txt, dll)
                            print("\n[Fileman] Format belum didukung untuk GUI: ");
                            print(target_file);
                            print("\n");
                        }
                    }
                } // Akhir dari Tombol Buka
                
            } // Akhir dari event Type 3 (Click)

            if (event.type == 1 && event.param1 == 27) {
                break;
            }
        }
        sys_yield(); 
    }
    
    sys_destroy_window(win_id);
    sys_free(my_canvas);
}