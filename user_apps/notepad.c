#include "userlib.h"
#include "libgui.h"

#define WIN_W 420
#define WIN_H 340

#define BG_COLOR    0xFFFFFF
#define TEXT_COLOR  0x111111
#define TOOLBAR_BG  0xE0E0E0
#define BTN_SAVE_BG 0x4CAF50
#define BTN_TEXT    0xFFFFFF
#define HIGHLIGHT   0xBBBBBB

static char text_buffer[4096];
static int text_len = 0;
static char current_file[64] = "catatan.txt";
static int file_len = 11;

// State Machine
static int editing_filename = 0;

void render_notepad(gui_window_t* win) {
    int W = (int)win->inner_w;
    int H = (int)win->inner_h;

    // 1. Background Kertas
    gui_draw_rect(win, 0, 0, W, H, BG_COLOR);

    // 2. Toolbar Atas
    gui_draw_rect(win, 0, 0, W, 30, TOOLBAR_BG);
    
    // 3. Render Filename Box (Berubah warna jika sedang diedit)
    if (editing_filename) {
        gui_draw_rect(win, 5, 4, 180, 22, HIGHLIGHT);
        if ((sys_uptime() / 500) % 2 == 0) {
            gui_draw_rect(win, 10 + (file_len * 8), 7, 8, 16, 0x000000); // Kursor filename
        }
    }
    gui_draw_text(win, current_file, 10, 7, 0x222222);

    // 4. Tombol SAVE
    gui_draw_rect(win, W - 70, 4, 60, 22, BTN_SAVE_BG);
    gui_draw_text(win, "SAVE", W - 55, 7, BTN_TEXT);

    // 5. Render Teks Isi
    int x = 5;
    int y = 35;
    for (int i = 0; i < text_len; i++) {
        if (text_buffer[i] == '\n') { x = 5; y += 16; } 
        else {
            gui_draw_char(win, text_buffer[i], x, y, TEXT_COLOR);
            x += 8;
        }
        if (x >= W - 10) { x = 5; y += 16; } // Wrap
    }

    // 6. Kursor Teks Isi
    if (!editing_filename && (sys_uptime() / 500) % 2 == 0) {
        gui_draw_rect(win, x, y, 8, 16, TEXT_COLOR);
    }
}

void main(void) {
    if (sys_file_exists("edit.tmp")) {
        uint32_t tmp_size = sys_file_size("edit.tmp");
        if (tmp_size > 0 && tmp_size < 64) {
            sys_read_file_to_buffer("edit.tmp", current_file);
            current_file[tmp_size] = '\0';
            file_len = (int)tmp_size;
        }
        // Selalu hapus .tmp — agar tidak mempengaruhi peluncuran berikutnya
        fs_delete("edit.tmp");
    }

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
        uint32_t now = sys_uptime();
        if (now - last_uptime >= 500) { // Paksa refresh untuk kursor
            last_uptime = now;
            render_notepad(app);
            gui_flush(app);
        }

        if (sys_get_event(&ev)) {
            // --- MOUSE MOVE: selalu update cache posisi ---
            if (ev.type == EVENT_MOUSE_MOVE) {
                app->mouse_x = ev.param1;
                app->mouse_y = ev.param2;
            }

            // --- MOUSE CLICK ---
            if (ev.type == EVENT_MOUSE_CLICK && ev.param2 == 1) {
                if (ev.param3 != 0) app->mouse_x = ev.param3;
                int wx = 0, wy = 0;
                sys_get_window_pos(app->win_id, &wx, &wy);

                int rfx = app->mouse_x - wx;
                int rfy = app->mouse_y - wy;
                int rel_x = rfx;
                int rel_y = rfy - GUI_TITLEBAR_H;

                // Cek Close
                if (rfx >= (int)app->width - GUI_CLOSE_BTN_W && rfy < GUI_TITLEBAR_H) {
                    app->is_running = 0; break;
                }

                // Cek Klik Area Toolbar (Filename Edit Mode)
                if (rel_y >= 0 && rel_y <= 30 && rel_x < (int)app->inner_w - 80) {
                    editing_filename = 1;
                } 
                // Cek Klik Area Teks (Keluar dari Edit Mode)
                else if (rel_y > 30) {
                    editing_filename = 0;
                }
                
                // Cek Klik SAVE
                if (rel_x >= (int)app->inner_w - 70 && rel_x <= (int)app->inner_w - 10 && rel_y >= 4 && rel_y <= 26) {
                    if (sys_file_exists(current_file)) fs_delete(current_file);
                    sys_create_file(current_file, text_buffer, text_len);
                    editing_filename = 0; // Selesai edit nama saat di save
                }
                
                render_notepad(app);
                gui_flush(app);
            }

            // --- KEYBOARD ---
            if (ev.type == EVENT_KEY_PRESS) {
                char c = (char)ev.param1;
                
                if (c == 27) { // ESC
                    app->is_running = 0; break;
                }

                if (editing_filename) {
                    // Logika Edit Nama File
                    if (c == '\n' || c == '\r') {
                        editing_filename = 0; // Enter = Selesai
                    } else if (c == 8 && file_len > 0) { // Backspace
                        current_file[--file_len] = '\0';
                    } else if (c >= 32 && c <= 126 && c != '/' && file_len < 30) {
                        current_file[file_len++] = c;
                        current_file[file_len] = '\0';
                    }
                } else {
                    // Logika Edit Teks Utama
                    if (c == 8 && text_len > 0) {
                        text_buffer[--text_len] = '\0';
                    } else if ((c == '\n' || c == '\r') && text_len < 4095) {
                        text_buffer[text_len++] = '\n';
                        text_buffer[text_len] = '\0';
                    } else if (c >= 32 && c <= 126 && text_len < 4095) {
                        text_buffer[text_len++] = c;
                        text_buffer[text_len] = '\0';
                    }
                }
                
                render_notepad(app);
                gui_flush(app);
            }
        }
        sys_yield();
    }
    gui_destroy(app);
    sys_exec("fileman.elf");
}