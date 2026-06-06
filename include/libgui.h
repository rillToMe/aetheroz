#ifndef LIBGUI_H
#define LIBGUI_H

#include <stdint.h>
#include "userlib.h"

// ============================================================
// Kyuzen GUI Framework (libgui)
//
// Cara pakai:
//   gui_window_t* win = gui_create_window("Judul", 400, 300);
//   gui_set_render(win, my_render_fn);
//   gui_mainloop(win);   ← blocks sampai window ditutup
//   gui_destroy(win);
//   sys_exit();
//
// Render callback dipanggil tiap frame oleh gui_mainloop.
// Gunakan gui_draw_rect / gui_draw_text untuk menggambar.
// Koordinat SELALU relatif terhadap isi window (bukan title bar).
// ============================================================

// --- Layout Konstan ---
#define GUI_TITLEBAR_H     30    // Tinggi title bar (px)
#define GUI_CLOSE_BTN_W    40    // Lebar tombol Close dari kanan (px)
#define GUI_TITLEBAR_COLOR 0x111111
#define GUI_CLOSE_COLOR    0xE53935
#define GUI_CLOSE_HOVER    0xFF1744

// --- Objek Window Utama ---
typedef struct gui_window_t gui_window_t;

typedef void (*gui_render_fn)(gui_window_t* win);

struct gui_window_t {
    int         win_id;
    uint32_t    width;      // Lebar total canvas (termasuk title bar)
    uint32_t    height;     // Tinggi total canvas (termasuk title bar)
    uint32_t    inner_w;    // Lebar area isi (= width)
    uint32_t    inner_h;    // Tinggi area isi (= height - GUI_TITLEBAR_H)
    uint32_t*   canvas;     // Pointer ke pixel buffer
    char        title[64];
    int         is_running;

    // State mouse (di-update oleh gui_mainloop setiap frame)
    int         mouse_x;   // Posisi absolut layar
    int         mouse_y;
    int         rel_x;     // Posisi relatif terhadap window origin (bukan title bar)
    int         rel_y;

    // User render callback — dipanggil setiap ada update
    gui_render_fn on_render;
};

// ============================================================
// API PUBLIC
// ============================================================

// Buat window baru. Alokasi canvas + gambar title bar otomatis.
// width / height = dimensi TOTAL (termasuk title bar).
gui_window_t* gui_create_window(const char* title, uint32_t width, uint32_t height);

// Set fungsi render — dipanggil tiap frame.
void gui_set_render(gui_window_t* win, gui_render_fn fn);

// Jalankan event loop. Blocks sampai user klik X atau tekan ESC.
// Memanggil on_render setiap kali ada event / timer.
void gui_mainloop(gui_window_t* win);

// Hancurkan window dan bebaskan canvas.
void gui_destroy(gui_window_t* win);

// --- Drawing API ---
// Semua koordinat RELATIF terhadap area ISI (di bawah title bar).
// y=0 = baris pertama area isi, bukan title bar.

// Isi persegi panjang dengan warna solid.
void gui_draw_rect(gui_window_t* win, int x, int y, int w, int h, uint32_t color);

// Gambar satu karakter (dari font 8x16 built-in).
void gui_draw_char(gui_window_t* win, char c, int x, int y, uint32_t color);

// Gambar string teks.
void gui_draw_text(gui_window_t* win, const char* text, int x, int y, uint32_t color);

// Gambar string dengan angka uint32_t di belakangnya (misal: "RAM: 128 MB").
void gui_draw_label_num(gui_window_t* win, const char* label, uint32_t num,
                        const char* suffix, int x, int y, uint32_t color);

// Gambar progress bar horizontal.
// value/max = rasio (0..max), bar_color = warna isi.
void gui_draw_bar(gui_window_t* win, int x, int y, int w, int h,
                  uint32_t value, uint32_t max_value, uint32_t bar_color);

// Paksa flush canvas ke layar (biasanya dipanggil otomatis oleh mainloop).
void gui_flush(gui_window_t* win);

#endif // LIBGUI_H
