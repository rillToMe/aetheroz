// ============================================================
// notepad.c — Kyuzen Text Editor (libgui standard)
// Bisa mengetik, backspace, enter, dan SAVE ke Hard Disk.
// ============================================================
#include "userlib.h"
#include "libgui.h"

#define WIN_W 420
#define WIN_H 340

// --- Warna Tema ---
#define BG_COLOR    0xFFFFFF
#define TEXT_COLOR  0x111111
#define TOOLBAR_BG  0xE0E0E0
#define BTN_SAVE_BG 0x4CAF50
#define BTN_TEXT    0xFFFFFF

static char text_buffer[4096];
static int text_len = 0;
static char current_file[64] = "catatan.txt"; // Default filename

void render_notepad(gui_window_t* win) {
    int W = (int)win->inner_w;
    int H = (int)win->inner_h;

    // 1. Background Kertas
    gui_draw_rect(win, 0, 0, W, H, BG_COLOR);

    // 2. Toolbar Atas
    gui_draw_rect(win, 0, 0, W, 30, TOOLBAR_BG);
    gui_draw_text(win, current_file, 10, 7, 0x555555);

    // 3. Tombol SAVE
    gui_draw_rect(win, W - 70, 4, 60, 22, BTN_SAVE_BG);
    gui_draw_text(win, "SAVE", W - 55, 7, BTN_TEXT);

    // 4. Render Teks
    int x = 5;
    int y = 35;
    for (int i = 0; i < text_len; i++) {
        if (text_buffer[i] == '\n') {
            x = 5;
            y += 16;
        } else {
            gui_draw_char(win, text_buffer[i], x, y, TEXT_COLOR);
            x += 8;
        }
        // Auto-wrap jika mentok kanan
        if (x >= W - 10) {
            x = 5;
            y += 16;
        }
    }

    // 5. Blinking Cursor (Berkedip setiap 500ms)
    if ((sys_uptime() / 500) % 2 == 0) {
        gui_draw_rect(win, x, y, 8, 16, TEXT_COLOR);
    }
}

void main(void) {
    // Cek apakah dibuka dari file manager (ada edit.tmp)
    if (sys_file_exists("edit.tmp")) {
        uint32_t tmp_size = sys_file_size("edit.tmp");
        if (tmp_size > 0 && tmp_size < 64) {
            sys_read_file_to_buffer("edit.tmp", current_file);
            current_file[tmp_size] = '\0';
            fs_delete("edit.tmp"); // Hapus temp file
        }
    }

    // Load isi file jika ada
    if (sys_file_exists(current_file)) {
        text_len = sys_file_size(current_file);
        if (text_len > 4095) text_len = 4095;
        sys_read_file_to_buffer(current_file, text_buffer);
        text_buffer[text_len] = '\0';
    } else {
        text_len = 0;
        text_buffer[0] = '\0';
    }

    gui_window_t* app = gui_create_window("Kyuzen Notepad", WIN_W, WIN_H);
    if (!app) { sys_exit(); return; }

    kyuzen_event_t ev;
    uint32_t last_uptime = 0;

    while (app->is_running) {
        // Render paksa tiap 500ms agar kursor bisa berkedip
        uint32_t now = sys_uptime();
        if (now - last_uptime >= 500) {
            last_uptime = now;
            render_notepad(app);
            gui_flush(app);
        }

        if (sys_get_event(&ev)) {
            // --- MOUSE EVENT ---
            if (ev.type == EVENT_MOUSE_MOVE) {
                app->mouse_x = ev.param1;
                app->mouse_y = ev.param2;
            }
            if (ev.type == EVENT_MOUSE_CLICK && ev.param1 == 0 && ev.param2 == 1) {
                if (ev.param3 != 0) app->mouse_x = ev.param3;
                
                int wx = 0, wy = 0;
                sys_get_window_pos(app->win_id, &wx, &wy);
                int rfx = app->mouse_x - wx;
                int rfy = app->mouse_y - wy;

                // Cek Klik Tombol Close (X)
                if (rfx >= (int)app->width - GUI_CLOSE_BTN_W && rfx < (int)app->width && rfy >= 0 && rfy < GUI_TITLEBAR_H) {
                    app->is_running = 0; break;
                }

                // Cek Klik Tombol SAVE
                int rel_x = rfx;
                int rel_y = rfy - GUI_TITLEBAR_H;
                if (rel_x >= (int)app->inner_w - 70 && rel_x <= (int)app->inner_w - 10 && rel_y >= 4 && rel_y <= 26) {
                    if (sys_file_exists(current_file)) fs_delete(current_file); // Hapus versi lama
                    sys_create_file(current_file, text_buffer, text_len);       // Simpan versi baru
                }
            }

            // --- KEYBOARD EVENT ---
            if (ev.type == EVENT_KEY_PRESS) {
                char c = (char)ev.param1;
                if (c == 27) { // ESC = Keluar
                    app->is_running = 0;
                } else if (c == 8) { // Backspace
                    if (text_len > 0) {
                        text_len--;
                        text_buffer[text_len] = '\0';
                    }
                } else if (c == '\n' || c == '\r') { // Enter
                    if (text_len < 4095) {
                        text_buffer[text_len++] = '\n';
                        text_buffer[text_len] = '\0';
                    }
                } else if (c >= 32 && c <= 126) { // Huruf / Angka / Simbol
                    if (text_len < 4095) {
                        text_buffer[text_len++] = c;
                        text_buffer[text_len] = '\0';
                    }
                }
                
                // Segera render ulang agar ketikan langsung muncul tanpa nunggu 500ms
                render_notepad(app);
                gui_flush(app);
            }
        }
        sys_yield();
    }

    gui_destroy(app);
    // Jika ada argumen temp file, kembalikan ke fileman saat diclose
    sys_exec("fileman.elf");
}