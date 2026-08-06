#ifndef LIBUI_H
#define LIBUI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// libui — Widget Toolkit (Phase 6) — Public C ABI
//
// Aplikasi (C / Rust / Zig / dst.) berinteraksi dengan toolkit
// lewat API C murni ini. Semua handle OPAQUE — implementasi
// internal (apps/libui.cpp) adalah Modern C++ yang TIDAK bocor
// ke sini: tidak ada C++ type, template, exception, RTTI, atau
// STL di belakang API ini. Semua symbol luar tetap extern "C".
//
// Prefiks ui_ (bukan gui_) agar tidak bertabrakan dengan
// libgui.h yang sudah punya gui_window_t. libgui tetap renderer
// tingkat rendah (canvas + draw primitives); libui = pohon
// widget di atasnya.
//
// Alur:
//   ui_window_t* w = ui_window_create(280, 140);
//   ui_widget_t* box = ui_vbox_create(w, 10);
//   ui_widget_t* lbl = ui_label_create(w, "Klik: 0");
//   ui_layout_add(box, lbl);
//   ui_widget_t* btn = ui_button_create(w, "+1");
//   ui_button_set_click(btn, on_click, 0);
//   ui_layout_add(box, btn);
//   ui_window_add(w, box);
//   ui_window_run(w);          // blocking sampai window ditutup
//   ui_window_destroy(w);
// ============================================================

typedef struct ui_window ui_window_t;   // satu window + pohon widget
typedef struct ui_widget ui_widget_t;   // basis semua widget (label, button, layout)

// Tema — warna ARGB (alpha dipaksa 0xFF di painter).
typedef struct ui_theme {
    uint32_t bg;             // latar window
    uint32_t fg;             // teks umum
    uint32_t accent;         // aksen
    uint32_t button_bg;      // latar tombol
    uint32_t button_fg;      // teks tombol
    uint32_t button_hover;   // latar tombol saat hover
} ui_theme_t;

// Callback klik tombol. userdata = argumen ui_button_set_click.
typedef void (*ui_click_cb)(void* userdata);

// --- Window ---
// Buat window + pohon widget kosong. Return 0 jika gagal.
ui_window_t* ui_window_create(uint32_t width, uint32_t height);
void ui_window_destroy(ui_window_t* win);
void ui_window_set_theme(ui_window_t* win, const ui_theme_t* theme); // 0 = default
void ui_window_add(ui_window_t* win, ui_widget_t* widget);   // tambah ke layout root
void ui_window_run(ui_window_t* win);   // blocking sampai window ditutup (X / ESC)

// --- Label ---
// text di-copy oleh toolkit — caller boleh pakai stack buffer.
ui_widget_t* ui_label_create(ui_window_t* win, const char* text);
void ui_label_set_text(ui_widget_t* widget, const char* text);

// --- Button ---
ui_widget_t* ui_button_create(ui_window_t* win, const char* text);
void ui_button_set_click(ui_widget_t* widget, ui_click_cb cb, void* userdata);

// --- Layout ---
// VBox: susun anaknya vertikal (masing-masing setinggi ukurannya,
// diberi spacing pixel). Win disediakan agar API seragam tapi
// widget hasilnya milik caller (bukan window).
ui_widget_t* ui_vbox_create(ui_window_t* win, int spacing);
void ui_layout_add(ui_widget_t* layout, ui_widget_t* child);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBUI_H
