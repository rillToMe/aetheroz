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
    int mouse_x = 0, mouse_y = 0;  // Cache posisi mouse terkini
    while (1) {
        if (sys_get_event(&event)) {
            if (event.type == EVENT_MOUSE_MOVE) {
                mouse_x = event.param1;
                mouse_y = event.param2;
            }

            // EVENT_MOUSE_CLICK: param1=0 (kiri), param2=1 (ditekan), param3=mouse_x
            // mouse_y diambil dari cache MOVE (selalu valid karena mouse gerak sebelum klik)
            if (event.type == EVENT_MOUSE_CLICK && event.param1 == 0 && event.param2 == 1) {
                // Sinkronkan mouse_x dari click event jika ada
                if (event.param3 != 0) mouse_x = event.param3;

                // Window ada di layar posisi (win_off_x=200, win_off_y=150)
                int win_off_x = 200, win_off_y = 150;

                // Koordinat RELATIF terhadap window
                int rel_x = mouse_x - win_off_x;
                int rel_y = mouse_y - win_off_y;

                // 1. DETEKSI KLIK: Tombol Close (X) — pojok kanan atas
                if (rel_x >= (win_w - 40) && rel_x <= win_w &&
                    rel_y >= 0 && rel_y <= 30) {
                    break;
                }

                // 2. DETEKSI KLIK: Daftar File
                int list_x0 = 10, list_x1 = win_w - 10;
                int list_y0 = 70, list_y1 = list_y0 + (total_files * 20);

                if (rel_x >= list_x0 && rel_x <= list_x1 &&
                    rel_y >= list_y0 && rel_y < list_y1) {
                    int clicked_index = (rel_y - list_y0) / 20;
                    if (clicked_index >= 0 && clicked_index < total_files) {
                        selected_file = clicked_index;
                        render_ui(files, total_files);
                    }
                }

                // 3. DETEKSI KLIK: TOMBOL BUKA (CYAN) — koordinat rel terhadap window
                if (selected_file != -1) {
                    int buka_x0 = win_w - 70, buka_x1 = win_w - 10;
                    int buka_y0 = win_h - 26, buka_y1 = win_h - 4;

                    if (rel_x >= buka_x0 && rel_x <= buka_x1 &&
                        rel_y >= buka_y0 && rel_y <= buka_y1) {
                        
                        char* target_file = files[selected_file].filename;
                        
                        // Cek ekstensi file
                        int len = 0; while(target_file[len]) len++;
                        
                        if (len > 4 && target_file[len-4] == '.' && target_file[len-3] == 'e' && 
                            target_file[len-2] == 'l' && target_file[len-1] == 'f') {
                            
                            // --- LOGIKA BUKA .ELF ---
                            sys_destroy_window(win_id);
                            sys_free(my_canvas);

                            uint64_t app_entry = sys_load_elf(target_file);
                            if (app_entry != 0) {
                                void (*run_app)(void) = (void (*)(void))app_entry;
                                run_app();
                            }
                            return;
                        } 
                        else if (len > 4 && target_file[len-4] == '.' && target_file[len-3] == 'p' && 
                                 target_file[len-2] == 'n' && target_file[len-1] == 'g') {
                            
                            // --- LOGIKA BUKA .PNG → VIEWER.ELF ---
                            if (sys_file_exists("view.tmp")) fs_delete("view.tmp");
                            sys_create_file("view.tmp", target_file, len);

                            sys_destroy_window(win_id);
                            sys_free(my_canvas);

                            uint64_t app_entry2 = sys_load_elf("viewer.elf");
                            if (app_entry2 != 0) {
                                void (*run_app2)(void) = (void (*)(void))app_entry2;
                                run_app2();
                            }
                            return;
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