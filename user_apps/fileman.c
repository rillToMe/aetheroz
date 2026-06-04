#include "userlib.h"

// Fungsi pembantu untuk menggambar kotak dengan super cepat menggunakan mesin "Blitting"
void draw_rect_fast(int start_x, int start_y, int width, int height, uint32_t color) {
    uint32_t* buffer = (uint32_t*) sys_alloc(width * height * sizeof(uint32_t));
    if (!buffer) return;

    for(int i = 0; i < width * height; i++) {
        buffer[i] = color | 0xFF000000; // Format 0xAARRGGBB (Alpha Penuh)
    }
    
    sys_draw_image(start_x, start_y, width, height, buffer);
    sys_free(buffer);
}

void main() {
    print("Membuka Kyuzen File Manager...\n");

    int win_x = 100, win_y = 100;
    int win_w = 400, win_h = 300;

    // 1. Gambar Jendela Utama (Background Putih)
    draw_rect_fast(win_x, win_y, win_w, win_h, 0xFFFFFF);
    
    // 2. Gambar Title Bar (Biru Gelap)
    draw_rect_fast(win_x, win_y, win_w, 30, 0x0055AA);
    sys_draw_string("Kyuzen OS - File Manager", win_x + 10, win_y + 8, 0xFFFFFF);

    // 3. Ambil daftar file dari Kernel (Menggunakan Jembatan Syscall 24)
    file_info_t files[16];
    int total_files = sys_get_file_list(files, 16);

    // 4. Gambar daftar file ke dalam Jendela!
    int start_item_y = win_y + 50;
    for(int i = 0; i < total_files; i++) {
        int item_y = start_item_y + (i * 30);

        // Gambar ikon (Kotak Kuning untuk folder, Kotak Abu-abu untuk file)
        uint32_t icon_color = files[i].is_folder ? 0xFFCC00 : 0xAAAAAA;
        draw_rect_fast(win_x + 20, item_y, 16, 16, icon_color);

        // Tulis nama file
        sys_draw_string(files[i].filename, win_x + 45, item_y, 0x000000);
    }

    // 5. EVENT LOOP (Jantung Aplikasi GUI yang sebenarnya)
    char key[2];
    while (1) {
        // Cek apakah ada tombol keyboard yang ditekan
        if (read_keyboard(key, 1) > 0) {
            // Jika tombol ESC (Kode ASCII: 27) ditekan, hancurkan loop!
            if (key[0] == 27) {
                break; 
            }
        }
        
        // PENTING: Beri napas ke CPU agar Kursor Mouse tetap bisa bergerak mulus!
        sys_yield(); 
    }
    
    // Aplikasi Selesai
    print("Menutup File Manager...\n");
}