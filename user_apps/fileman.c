#include "userlib.h"
#define FONT8x16_IMPLEMENTATION
#include "font8x16.h" // Impor pustaka font ke User Space!

int win_id = -1;
uint32_t* my_canvas = 0;
int win_w = 400, win_h = 300;

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

// --- MESIN TEKS RING 3 ---
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
// -------------------------

void main() {
    win_id = sys_create_window(200, 150, win_w, win_h);
    if (win_id < 0) return;

    my_canvas = (uint32_t*) sys_alloc(win_w * win_h * 4);

    // 1. Gambar Dasar UI
    draw_rect_local(0, 0, win_w, win_h, 0xFFFFFF);     // Background Putih
    draw_rect_local(0, 0, win_w, 30, 0x111111);        // Title Bar Hitam
    draw_rect_local(win_w - 40, 0, 40, 30, 0xE53935);  // Tombol Tutup

    // 2. Teks Title Bar (Putih)
    draw_string_local("FILE MANAGER", 10, 7, 0xFFFFFF);
    draw_string_local("X", win_w - 24, 7, 0xFFFFFF);

    // 3. Tarik Data File dari Hard Disk!
    file_info_t files[16];
    int total_files = sys_get_file_list(files, 16);

    // 4. Render Daftar File (Teks Hitam Tajam)
    draw_string_local("Isi Penyimpanan KZFS:", 15, 45, 0x000000);
    
    int start_y = 70;
    for (int i = 0; i < total_files; i++) {
        draw_string_local(">", 15, start_y, 0x555555); // Bullet point minimalis
        draw_string_local(files[i].filename, 30, start_y, 0x000000);
        start_y += 20; // Jarak antar item
    }

    // 5. Kirim Kanvas ke Layar
    sys_update_window(win_id, my_canvas);

    // 6. Event Loop
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
                int absolute_btn_x_start = 200 + (win_w - 40);
                int absolute_btn_x_end = 200 + win_w;
                int absolute_btn_y_start = 150 + 0;
                int absolute_btn_y_end = 150 + 30;

                if (mouse_x >= absolute_btn_x_start && mouse_x <= absolute_btn_x_end &&
                    mouse_y >= absolute_btn_y_start && mouse_y <= absolute_btn_y_end) {
                    break; 
                }
            }

            if (event.type == 1 && event.param1 == 27) {
                break;
            }
        }
        sys_yield(); 
    }
    
    // Pembersihan Memori
    sys_destroy_window(win_id);
    sys_free(my_canvas);
}