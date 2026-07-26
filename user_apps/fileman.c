// ============================================================
// fileman.c — Kyuzen File Manager (libgui standard)
// Menampilkan daftar file KyuzenFS, klik untuk memilih & buka.
// ============================================================
#include "userlib.h"
#include "libgui.h"

#define WIN_W 400
#define WIN_H 320

// Warna
#define BG_COLOR   0xFFFFFF
#define HDR_COLOR  0xEEEEEE
#define SEL_COLOR  0x111111
#define TEXT_DARK  0x000000
#define TEXT_LIGHT 0xFFFFFF
#define TEXT_GRAY  0x555555
#define BUKA_COLOR 0x00AEEF

// State
static file_info_t files[16];
static int total_files   = 0;
static int selected_file = -1;

// Gambar satu baris file
#define LIST_START_Y  40    // relatif ke area isi
#define LIST_ROW_H    20

void fileman_render(gui_window_t* win) {
    int W = (int)win->inner_w;
    int H = (int)win->inner_h;

    // Background
    gui_draw_rect(win, 0, 0, W, H, BG_COLOR);

    // Header info
    gui_draw_rect(win, 0, 0, W, 30, HDR_COLOR);
    gui_draw_text(win, "Isi Penyimpanan KZFS:", 15, 7, TEXT_GRAY);

    // Daftar file
    int sy = LIST_START_Y;
    for (int i = 0; i < total_files; i++) {
        if (i == selected_file) {
            gui_draw_rect(win, 10, sy - 2, W - 20, 20, SEL_COLOR);
            gui_draw_text(win, ">", 15, sy, TEXT_LIGHT);
            gui_draw_text(win, files[i].filename, 30, sy, TEXT_LIGHT);
        } else {
            gui_draw_text(win, ">", 15, sy, TEXT_GRAY);
            gui_draw_text(win, files[i].filename, 30, sy, TEXT_DARK);
        }
        sy += LIST_ROW_H;
    }

    // Status bar
    gui_draw_rect(win, 0, H - 30, W, 30, HDR_COLOR);
    if (selected_file != -1) {
        gui_draw_text(win, "Terpilih: ", 10, H - 22, TEXT_GRAY);
        gui_draw_text(win, files[selected_file].filename, 90, H - 22, TEXT_DARK);

        // Tombol BUKA
        gui_draw_rect(win, W - 70, H - 27, 60, 22, BUKA_COLOR);
        gui_draw_text(win, "BUKA", W - 55, H - 23, TEXT_LIGHT);
    }
}

void main(void) {
    gui_window_t* app = gui_create_window("File Manager", WIN_W, WIN_H);
    if (!app) { sys_exit(); return; }

    total_files = sys_get_file_list(files, 16);
    fileman_render(app);
    gui_flush(app);

    kyuzen_event_t ev;
    while (app->is_running) {
        if (sys_get_event(&ev)) {
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

                // Close button (title bar zona kanan)
                if (rfx >= (int)app->width - GUI_CLOSE_BTN_W &&
                    rfx <  (int)app->width &&
                    rfy >= 0 && rfy < GUI_TITLEBAR_H) {
                    app->is_running = 0; break;
                }

                // Koordinat relatif ke area isi
                int rel_x = rfx;
                int rel_y = rfy - GUI_TITLEBAR_H;

                // Klik daftar file
                int list_x0 = 10, list_x1 = (int)app->inner_w - 10;
                int list_y0 = LIST_START_Y;
                int list_y1 = list_y0 + total_files * LIST_ROW_H;
                // Baris yang tergambar di balik status bar TIDAK boleh bisa
                // diklik — dengan >11 file, zona daftar menimpa tombol BUKA
                // dan klik BUKA diam-diam memilih file tersembunyi terakhir.
                int vis_y1 = (int)app->inner_h - 30;   // batas atas status bar
                if (list_y1 > vis_y1) list_y1 = vis_y1;

                if (rel_x >= list_x0 && rel_x <= list_x1 &&
                    rel_y >= list_y0 && rel_y <  list_y1) {
                    int idx = (rel_y - list_y0) / LIST_ROW_H;
                    if (idx >= 0 && idx < total_files) {
                        selected_file = idx;
                        fileman_render(app);
                        gui_flush(app);
                    }
                }

                // Klik tombol BUKA
                if (selected_file != -1) {
                    int H = (int)app->inner_h;
                    int bx0 = (int)app->inner_w - 70;
                    int bx1 = (int)app->inner_w - 10;
                    int by0 = H - 27, by1 = H - 5;
                    if (rel_x >= bx0 && rel_x <= bx1 &&
                        rel_y >= by0 && rel_y <= by1) {
                        char* fname = files[selected_file].filename;
                        int len = 0; while (fname[len]) len++;

                        if (len > 4 && fname[len-4]=='.' && fname[len-3]=='e' &&
                            fname[len-2]=='l' && fname[len-1]=='f') {
                            gui_destroy(app);
                            sys_exec(fname);
                            return;
                        } else if (len > 4 && fname[len-4]=='.' && fname[len-3]=='p' &&
                                   fname[len-2]=='n' && fname[len-1]=='g') {
                            if (sys_file_exists("view.tmp")) fs_delete("view.tmp");
                            sys_create_file("view.tmp", fname, len);
                            gui_destroy(app);
                            sys_exec("viewer.elf");
                            return;
                        }
                         else if (len > 4 && fname[len-4]=='.' && fname[len-3]=='t' &&
                                   fname[len-2]=='x' && fname[len-1]=='t') {
                            if (sys_file_exists("edit.tmp")) fs_delete("edit.tmp");
                            sys_create_file("edit.tmp", fname, len); // Kasih tau notepad file apa yg mau dibuka
                            gui_destroy(app);
                            sys_exec("notepad.elf");
                            return;
                        }
                    }
                }
            }

            if (ev.type == EVENT_KEY_PRESS && ev.param1 == 27) {
                app->is_running = 0; break;
            }
        }
        sys_yield();
    }

    gui_destroy(app);
    sys_exit();
}