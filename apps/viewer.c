#include "userlib.h"

// Deklarasi fungsi gambar (jika belum ada di userlib.h)
extern void sys_draw_pixel(int x, int y, uint32_t color);

// --- KONFIGURASI STB_IMAGE UNTUK KYUZEN OS ---
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_THREAD_LOCALS
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_ASSERT(x)

// Arahkan alokasi memori ke syscall Kyuzen OS
#define STBI_MALLOC(sz)                       sys_alloc(sz)
#define STBI_FREE(p)                          sys_free(p)
// Gunakan versi SIZED agar cocok dengan (ptr, old_size, new_size) milikmu
#define STBI_REALLOC_SIZED(p, old_sz, new_sz) sys_realloc(p, old_sz, new_sz)

#include "stb_image.h"

// --- FUNGSI UNTUK MEMUAT DAN MENGGAMBAR PNG ---
void draw_png_image(const char* filename, int start_x, int start_y) {
    // 1. Ambil ukuran presisi file dari KyuzenFS
    uint32_t exact_size = sys_file_size((char*)filename);
    
    if (exact_size == 0) {
        print("Error: File kosong atau tidak ada di disk!\n");
        return;
    }

    // 2. Alokasikan RAM HANYA sebesar ukuran file (contoh: 13 KB)
    uint8_t* file_buffer = (uint8_t*) sys_alloc(exact_size);
    
    if (!file_buffer) {
        print("Error: Kehabisan memori saat menyiapkan buffer!\n");
        return;
    }

    // 3. Baca isi file PNG mentah ke RAM
    int read_size = sys_read_file_to_buffer((char*)filename, (char*)file_buffer); 
    
    if (read_size <= 0) {
        print("Error: Gagal membaca file PNG!\n");
        sys_free(file_buffer);
        return;
    }

    // 4. Dekode PNG menggunakan stb_image
    int width, height, channels;
    uint8_t *img_data = stbi_load_from_memory(file_buffer, exact_size, &width, &height, &channels, 4);

    if (!img_data) {
        print("Error: File rusak atau STB gagal decode!\n");
        sys_free(file_buffer);
        return;
    }

    // 4. Gambar piksel demi piksel ke layar
    int index = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t r = img_data[index++];
            uint8_t g = img_data[index++];
            uint8_t b = img_data[index++];
            uint8_t a = img_data[index++]; // Alpha channel

            if (a > 0) {
                // Susun warna hex (sesuaikan susunan r,g,b jika warna terbalik)
                uint32_t hex_color = (r << 16) | (g << 8) | b;
                sys_draw_pixel(start_x + x, start_y + y, hex_color);
            }
        }
    }

    // 5. Bersihkan memori agar tidak bocor
    sys_free(img_data);
    sys_free(file_buffer);
}

// --- FUNGSI MAIN APLIKASI ---
void main() {
    print("Memuat gambar PNG...\n");
    
    draw_png_image("logo.png", 100, 100);
    
    print("Selesai menggambar!\n");
    
}