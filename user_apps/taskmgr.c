#include <stdint.h>
#include "userlib.h"
#define FONT8x16_IMPLEMENTATION
#include "font8x16.h" // Diperlukan untuk render teks ke canvas lokal

// --- Konstanta Jendela ---
#define WIN_WIDTH 320
#define WIN_HEIGHT 240
#define BG_COLOR 0xFF202020   // Latar belakang gelap
#define BAR_BG   0xFF444444   // Background progress bar
#define RAM_COLOR 0xFF00FF55  // Hijau Neon
#define CPU_COLOR 0xFF0088FF  // Biru
#define DISK_COLOR 0xFFFFCC00 // Kuning
#define TEXT_COLOR 0xFFFFFFFF // Putih

// --- Fungsi Helper Manipulasi String ---
void num_to_str(uint32_t num, char* str) {
    if (num == 0) { str[0] = '0'; str[1] = '\0'; return; }
    int i = 0; char temp[16];
    while (num > 0) { temp[i++] = (num % 10) + '0'; num /= 10; }
    int j = 0;
    while (i > 0) { str[j++] = temp[--i]; }
    str[j] = '\0';
}

void str_append(char* dest, const char* src) {
    while (*dest) dest++;
    while (*src) *dest++ = *src++;
    *dest = '\0';
}

// --- Fungsi Render Grafis ke Memori Lokal (Double Buffering) ---
void draw_pixel(uint32_t* canvas, int x, int y, uint32_t color) {
    if (x < 0 || x >= WIN_WIDTH || y < 0 || y >= WIN_HEIGHT) return;
    canvas[(y * WIN_WIDTH) + x] = color;
}

void draw_rect(uint32_t* canvas, int start_x, int start_y, int w, int h, uint32_t color) {
    for (int y = start_y; y < start_y + h; y++) {
        for (int x = start_x; x < start_x + w; x++) {
            draw_pixel(canvas, x, y, color);
        }
    }
}

void draw_char(uint32_t* canvas, char c, int x, int y, uint32_t color) {
    if (c < 0 || c > 127) return;
    const unsigned char* bitmap = font8x16[(int)c];
    for (int row = 0; row < 16; row++) { 
        for (int col = 0; col < 8; col++) {
            if (bitmap[row] & (0x80 >> col)) draw_pixel(canvas, x + col, y + row, color);
        }
    }
}

void draw_string(uint32_t* canvas, const char* str, int x, int y, uint32_t color) {
    int curr_x = x;
    int curr_y = y;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') { curr_y += 16; curr_x = x; } 
        else { draw_char(canvas, str[i], curr_x, curr_y, color); curr_x += 8; }
    }
}

// --- FUNGSI UTAMA APP ---
void main(void) {
    // Koordinat window di layar utama (dibutuhkan untuk hitungan mouse)
    int win_off_x = 200;
    int win_off_y = 150;
    
    // 1. Buat Window via KWM
    int win_id = sys_kwm_create_window(win_off_x, win_off_y, WIN_WIDTH, WIN_HEIGHT);
    if (win_id < 0) {
        sys_exit(); 
    }

    // 2. Alokasi RAM untuk Canvas
    uint32_t* canvas = (uint32_t*)sys_alloc(WIN_WIDTH * WIN_HEIGHT * 4);
    if (!canvas) {
        sys_kwm_destroy_window(win_id);
        sys_exit();
    }

    kyuzen_event_t ev;
    int running = 1;
    uint32_t waktu[6];
    uint32_t last_sec = 0xFF; 
    
    int mouse_x = 0, mouse_y = 0; // Cache posisi mouse

    while (running) {
        // Ambil waktu dari syscall RTC
        sys_get_time(waktu);
        uint32_t sec = waktu[5];

        // RENDER ULANG UI HANYA 1X SETIAP 1 DETIK BERLALU
        if (sec != last_sec) {
            last_sec = sec;

            // --- A. BERSIHKAN KANVAS ---
            draw_rect(canvas, 0, 0, WIN_WIDTH, WIN_HEIGHT, BG_COLOR);
            
            // Header / Judul Window & Tombol Tutup
            draw_rect(canvas, 0, 0, WIN_WIDTH, 24, 0xFF555555); // Bar judul
            draw_string(canvas, "Kyuzen Task Manager", 10, 4, TEXT_COLOR);
            draw_rect(canvas, WIN_WIDTH - 24, 0, 24, 24, 0xFFFF0000); 
            draw_string(canvas, "X", WIN_WIDTH - 16, 4, TEXT_COLOR);

            // --- B. AMBIL DATA REAL DARI KERNEL OS LU ---
            
            // RAM (Real, dikonversi dari Byte ke MB)
            uint32_t total_ram = sys_total_ram() / (1024 * 1024); 
            uint32_t used_ram = sys_used_ram() / (1024 * 1024);

            // CPU (Real, beban Scheduler)
            uint32_t cpu_usage = sys_get_cpu_usage(); 

            // DISK (Real KyuzenFS V3, dikonversi dari Byte ke MB)
            uint32_t total_disk = sys_get_total_disk() / (1024 * 1024); 
            uint32_t used_disk = sys_get_used_disk() / (1024 * 1024);   

            // Pengaman kalau total nilainya 0 (biar gak error dibagi nol pas ngegambar bar)
            if (total_ram == 0) total_ram = 1;
            if (total_disk == 0) total_disk = 1;

            // --- C. GAMBAR ELEMEN UI ---

            // 1. Render RAM Usage
            char str_ram[64] = "Memori RAM: ";
            char tmp[16];
            num_to_str(used_ram, tmp); str_append(str_ram, tmp);
            str_append(str_ram, " MB / ");
            num_to_str(total_ram, tmp); str_append(str_ram, tmp); str_append(str_ram, " MB");
            
            draw_string(canvas, str_ram, 10, 40, TEXT_COLOR);
            uint32_t ram_bar_width = (used_ram * (WIN_WIDTH - 20)) / total_ram;
            draw_rect(canvas, 10, 60, WIN_WIDTH - 20, 20, BAR_BG);
            draw_rect(canvas, 10, 60, ram_bar_width, 20, RAM_COLOR);

            // 2. Render CPU Usage
            char str_cpu[64] = "Penggunaan CPU: ";
            num_to_str(cpu_usage, tmp); str_append(str_cpu, tmp); str_append(str_cpu, " %");
            
            draw_string(canvas, str_cpu, 10, 100, TEXT_COLOR);
            draw_rect(canvas, 10, 120, WIN_WIDTH - 20, 20, BAR_BG);
            draw_rect(canvas, 10, 120, (cpu_usage * (WIN_WIDTH - 20)) / 100, 20, CPU_COLOR);

            // 3. Render Disk Usage
            char str_disk[64] = "Penyimpanan KyuzenFS: ";
            num_to_str(used_disk, tmp); str_append(str_disk, tmp);
            str_append(str_disk, " MB / ");
            num_to_str(total_disk, tmp); str_append(str_disk, tmp); str_append(str_disk, " MB");
            
            draw_string(canvas, str_disk, 10, 160, TEXT_COLOR);
            uint32_t disk_bar_width = (used_disk * (WIN_WIDTH - 20)) / total_disk;
            draw_rect(canvas, 10, 180, WIN_WIDTH - 20, 20, BAR_BG);
            draw_rect(canvas, 10, 180, disk_bar_width, 20, DISK_COLOR);

            // --- D. FLUSH KE MONITOR ---
            sys_kwm_update_window(win_id, canvas);
        }

        // --- E. EVENT HANDLING ---
        if (sys_get_event(&ev)) {
            if (ev.type == EVENT_MOUSE_MOVE) {
                mouse_x = ev.param1;
                mouse_y = ev.param2;
            }

            if (ev.type == EVENT_MOUSE_CLICK && ev.param1 == 0 && ev.param2 == 1) {
                if (ev.param3 != 0) mouse_x = ev.param3;
                int rel_x = mouse_x - win_off_x;
                int rel_y = mouse_y - win_off_y;

                if (rel_x >= (WIN_WIDTH - 24) && rel_x <= WIN_WIDTH && 
                    rel_y >= 0 && rel_y <= 24) {
                    running = 0; // KELUAR!
                }
            }

            if (ev.type == EVENT_KEY_PRESS && ev.param1 == 27) { 
                running = 0; 
            }
        }

        // --- F. YIELD ---
        sys_yield();
    }

    // --- CLEANUP ---
    sys_kwm_destroy_window(win_id);
    sys_free(canvas);
    sys_exit(); 
}