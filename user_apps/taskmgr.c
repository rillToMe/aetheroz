// ============================================================
// taskmgr.c — Kyuzen Task Manager
//
// Menggunakan Kyuzen GUI Framework (libgui).
// Tidak ada while-loop manual, tidak ada canvas alloc manual,
// tidak ada event-parsing kompleks — semuanya di-handle libgui.
// ============================================================

#include "userlib.h"
#include "libgui.h"

#define WIN_W 340
#define WIN_H 280

// Warna tema Task Manager
#define BG_COLOR   0x1A1A2E   // Dark navy
#define TEXT_COLOR 0xE0E0E0   // Putih terang
#define RAM_COLOR  0x00BCD4   // Cyan
#define CPU_COLOR  0x4CAF50   // Hijau
#define DISK_COLOR 0xFF9800   // Oranye

// Dipanggil setiap frame oleh gui_mainloop
void render(gui_window_t* win) {
    int W = (int)win->inner_w;

    // 1. Background area isi
    gui_draw_rect(win, 0, 0, W, (int)win->inner_h, BG_COLOR);

    // 2. Data RAM
    uint32_t total_ram = sys_total_ram() / (1024 * 1024);
    uint32_t used_ram  = sys_used_ram()  / (1024 * 1024);
    if (!total_ram) total_ram = 1;

    gui_draw_label_num(win, "RAM: ", used_ram, " MB", 10, 10, TEXT_COLOR);
    gui_draw_bar(win, 10, 30, W - 20, 16, used_ram, total_ram, RAM_COLOR);

    // 3. Data CPU
    uint32_t cpu_usage = sys_get_cpu_usage();
    gui_draw_label_num(win, "CPU: ", cpu_usage, " %", 10, 60, TEXT_COLOR);
    gui_draw_bar(win, 10, 80, W - 20, 16, cpu_usage, 100, CPU_COLOR);

    // 4. Data Disk
    uint32_t total_disk = sys_get_total_disk() / (1024 * 1024);
    uint32_t used_disk  = sys_get_used_disk()  / (1024 * 1024);
    if (!total_disk) total_disk = 1;

    gui_draw_label_num(win, "Disk: ", used_disk, " MB", 10, 110, TEXT_COLOR);
    gui_draw_bar(win, 10, 130, W - 20, 16, used_disk, total_disk, DISK_COLOR);

    // 5. Garis separator
    gui_draw_rect(win, 10, 160, W - 20, 1, 0x444466);

    // 6. Info footer
    uint32_t uptime_ms = sys_uptime();
    uint32_t uptime_s  = uptime_ms / 1000;
    gui_draw_label_num(win, "Uptime: ", uptime_s, " s", 10, 170, 0x888888);
    gui_draw_text(win, "Kyuzen OS v0.1 - Preemptive", 10, 190, 0x555577);
}

void main(void) {
    gui_window_t* app = gui_create_window(WIN_W, WIN_H);
    if (!app) { sys_exit(); return; }

    gui_set_render(app, render);
    gui_mainloop(app);   // blocks sampai user klik X atau ESC

    gui_destroy(app);
    sys_exit();
}