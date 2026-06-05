#include "userlib.h"
#define FONT8x16_IMPLEMENTATION
#include "font8x16.h"

// --- KONFIGURASI STB_IMAGE ---
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#define STBI_NO_STDIO
#define STBI_NO_THREAD_LOCALS
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_ASSERT(x)
#define STBI_MALLOC(sz)                       sys_alloc(sz)
#define STBI_FREE(p)                          sys_free(p)
#define STBI_REALLOC_SIZED(p, old_sz, new_sz) sys_realloc(p, old_sz, new_sz)
#include "stb_image.h"

int win_id = -1;
uint32_t* my_canvas = 0;
int win_w = 0, win_h = 0;

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
                int px = x + col, py = y + row;
                if (px >= 0 && px < win_w && py >= 0 && py < win_h) my_canvas[(py * win_w) + px] = solid_color;
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

void main() {
    print("[Viewer] START\n");

    // ==========================================================
    // 1. Baca Jembatan File (view.tmp) untuk tahu PNG mana yang diklik
    // ==========================================================
    if (!sys_file_exists("view.tmp")) { print("[Viewer] view.tmp tidak ada!\n"); return; }

    char target_png[64];
    uint32_t tmp_size = sys_file_size("view.tmp");
    if (tmp_size == 0 || tmp_size >= 63) return; // Nama file tidak valid

    sys_read_file_to_buffer("view.tmp", target_png);
    target_png[tmp_size] = '\0'; // Kunci string

    print("[Viewer] Target: "); print(target_png); print("\n");

    // ==========================================================
    // 2. Baca file PNG dari disk ke RAM
    // ==========================================================
    uint32_t img_filesize = sys_file_size(target_png);
    print("[Viewer] PNG size: "); print_num(img_filesize); print(" bytes\n");
    if (img_filesize == 0) {
        print("[Viewer] File tidak ditemukan: ");
        print(target_png); print("\n");
        return;
    }
    
    uint8_t* file_buffer = (uint8_t*) sys_alloc(img_filesize);
    if (!file_buffer) {
        print("[Viewer] GAGAL: Heap tidak cukup untuk buffer PNG!\n");
        return;
    }
    print("[Viewer] Buffer OK. Membaca file dari disk...\n");
    sys_read_file_to_buffer(target_png, (char*)file_buffer);

    // ==========================================================
    // 3. Dekode PNG menggunakan mesin STB_IMAGE
    // ==========================================================
    print("[Viewer] Mulai decode PNG...\n");
    int img_w, img_h, channels;
    uint8_t *img_data = stbi_load_from_memory(file_buffer, img_filesize, &img_w, &img_h, &channels, 4);
    print("[Viewer] Decode selesai!\n");
    sys_free(file_buffer); // Buang file mentah setelah decode

    if (!img_data) {
        print("[Viewer] GAGAL: Bukan file PNG atau decode error.\n");
        return;
    }

    // ==========================================================
    // 4. Kalkulasi Ukuran Jendela Dinamis
    // ==========================================================
    win_w = img_w;
    win_h = img_h + 30; // +30 untuk title bar
    
    // Failsafe: Jendela tidak boleh lebih sempit dari teks title bar
    if (win_w < 200) win_w = 200;

    // ==========================================================
    // 5. Buat Jendela + Alokasi Canvas
    // NULL check di sini mencegah freeze jika memory/slot habis!
    // ==========================================================
    win_id = sys_create_window(250, 100, win_w, win_h);
    if (win_id < 0) {
        print("[Viewer] GAGAL: Tidak ada slot jendela yang tersedia!\n");
        sys_free(img_data);
        return;
    }

    my_canvas = (uint32_t*) sys_alloc(win_w * win_h * 4);
    if (!my_canvas) {
        print("[Viewer] GAGAL: Heap tidak cukup untuk canvas jendela!\n");
        sys_destroy_window(win_id);
        sys_free(img_data);
        return;
    }

    // ==========================================================
    // 6. Gambar UI Dasar Jendela
    // ==========================================================
    draw_rect_local(0, 0, win_w, win_h, 0x181818);    // Kanvas Dark Gray 
    draw_rect_local(0, 0, win_w, 30, 0x111111);       // Title Bar Hitam
    draw_rect_local(win_w - 40, 0, 40, 30, 0xE53935); // Tombol Merah (Close)
    draw_string_local(target_png, 10, 7, 0xFFFFFF);   // Tulis nama file
    draw_string_local("X", win_w - 24, 7, 0xFFFFFF);

    // ==========================================================
    // 7. Tumpahkan Pixel Gambar ke Kanvas (Mulai dari Y=30 / bawah title bar)
    // ==========================================================
    int index = 0;
    int offset_x = (win_w - img_w) / 2; // Posisi gambar di tengah
    for (int y = 0; y < img_h; y++) {
        for (int x = 0; x < img_w; x++) {
            uint8_t r = img_data[index++];
            uint8_t g = img_data[index++];
            uint8_t b = img_data[index++];
            uint8_t a = img_data[index++];

            if (a > 0) { // Hanya gambar bagian yang tidak transparan
                uint32_t color = 0xFF000000 | (r << 16) | (g << 8) | b;
                my_canvas[((y + 30) * win_w) + (x + offset_x)] = color;
            }
        }
    }

    // Buang img_data SEGERA setelah pixel disalin ke canvas
    // (Bebaskan memory yang paling besar sesegera mungkin)
    sys_free(img_data);
    img_data = 0;
    
    // Kirim canvas ke Compositor
    sys_update_window(win_id, my_canvas);

    // ==========================================================
    // 8. Event Loop — Menunggu klik tombol Close atau ESC
    // ==========================================================
    kyuzen_event_t event;
    int mouse_x = 0, mouse_y = 0;  // Cache posisi mouse
    while (1) {
        if (sys_get_event(&event)) {
            if (event.type == EVENT_MOUSE_MOVE) {
                mouse_x = event.param1;
                mouse_y = event.param2;
            }

            // Klik kiri: param1=0, param2=1, param3=mouse_x saat klik
            if (event.type == EVENT_MOUSE_CLICK && event.param1 == 0 && event.param2 == 1) {
                if (event.param3 != 0) mouse_x = event.param3;
                // Window di posisi (250, 100) — konversi ke koordinat relatif
                int win_off_x = 250, win_off_y = 100;
                int rel_x = mouse_x - win_off_x;
                int rel_y = mouse_y - win_off_y;

                // Tombol Close (X): pojok kanan atas, lebar 40px, tinggi 30px
                if (rel_x >= (win_w - 40) && rel_x <= win_w &&
                    rel_y >= 0 && rel_y <= 30) {
                    break;
                }
            }

            if (event.type == EVENT_KEY_PRESS && event.param1 == 27) break; // ESC
        }
        sys_yield();
    }
    
    // ==========================================================
    // 9. Pembersihan: Hancurkan jendela, bebaskan canvas
    // ==========================================================
    sys_destroy_window(win_id);
    sys_free(my_canvas);
    my_canvas = 0;
    
    // Kembali ke File Manager
    sys_exec("fileman.elf"); // OS free viewer RAM + load fileman + jump
}
