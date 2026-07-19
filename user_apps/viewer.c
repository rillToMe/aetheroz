// ============================================================
// viewer.c — Kyuzen Image Viewer (libgui standard)
// Galeri gambar PNG: daftar file, klik untuk membuka, tombol
// Back untuk kembali ke daftar. Jendela berukuran tetap dan
// gambar diperkecil agar muat, sehingga tidak membebani compositor.
// ============================================================
#include "userlib.h"
#include "libgui.h"

// STB_IMAGE (FONT8x16_IMPLEMENTATION sudah ada di libgui.o, jangan ulang)
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

#define WIN_W 480
#define WIN_H 400

#define BG_COLOR   0xFFFFFF
#define HDR_COLOR  0xEEEEEE
#define TEXT_DARK  0x000000
#define TEXT_GRAY  0x555555
#define PICK_COLOR 0x00AEEF
#define IMG_BG     0x202020
#define BTN_COLOR  0x00AEEF
#define BTN_TEXT   0xFFFFFF

#define HEADER_H     30
#define LIST_START_Y 44
#define LIST_ROW_H   20
#define TOOLBAR_H    30
#define BACK_X       10
#define BACK_Y       4
#define BACK_W       70
#define BACK_H       22

#define MAX_FILES 32

enum { MODE_LIST, MODE_IMAGE };

static file_info_t files[MAX_FILES];
static int png_slots[MAX_FILES];   // Indeks ke files[] yang berekstensi .png
static int png_count = 0;
static int mode = MODE_LIST;

// PNG dikenali dari 4 karakter terakhir ".png" (huruf kecil, seperti fileman).
static int is_png(const char* name) {
    int n = 0; while (name[n]) n++;
    return n > 4 && name[n-4] == '.' && name[n-3] == 'p' &&
           name[n-2] == 'n' && name[n-1] == 'g';
}

static void scan_images(void) {
    int total = sys_get_file_list(files, MAX_FILES);
    png_count = 0;
    for (int i = 0; i < total; i++) {
        if (!files[i].is_folder && is_png(files[i].filename)) {
            png_slots[png_count++] = i;
        }
    }
}

static void render_list(gui_window_t* win) {
    int W = (int)win->inner_w;
    int H = (int)win->inner_h;

    gui_draw_rect(win, 0, 0, W, H, BG_COLOR);
    gui_draw_rect(win, 0, 0, W, HEADER_H, HDR_COLOR);
    gui_draw_text(win, "Galeri Gambar - klik untuk membuka", 15, 7, TEXT_GRAY);

    if (png_count == 0) {
        gui_draw_text(win, "Tidak ada gambar (.png) di penyimpanan.",
                      15, LIST_START_Y, TEXT_GRAY);
        return;
    }

    int sy = LIST_START_Y;
    for (int i = 0; i < png_count; i++) {
        gui_draw_text(win, ">", 15, sy, PICK_COLOR);
        gui_draw_text(win, files[png_slots[i]].filename, 30, sy, TEXT_DARK);
        sy += LIST_ROW_H;
    }
}

// Nearest-neighbor fit: perkecil agar muat di area tanpa mengubah rasio.
// Dijalankan sekali saat gambar dibuka, bukan tiap frame, jadi biaya
// per-pixel di sini tidak memengaruhi frame rate compositor.
static void blit_fit(gui_window_t* win, const uint8_t* img, int iw, int ih,
                     int area_x, int area_y, int area_w, int area_h) {
    int dw = iw;
    int dh = ih;
    if (iw > area_w || ih > area_h) {
        // Pilih dimensi pembatas via perbandingan silang (hindari float).
        if (iw * area_h >= ih * area_w) { dw = area_w; dh = ih * area_w / iw; }
        else                            { dh = area_h; dw = iw * area_h / ih; }
    }
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;

    int ox = area_x + (area_w - dw) / 2;
    int oy = area_y + (area_h - dh) / 2;
    int W = (int)win->width;
    int H = (int)win->height;

    for (int dy = 0; dy < dh; dy++) {
        int py = oy + dy;
        if (py < 0 || py >= H) continue;
        int sy = dy * ih / dh;
        for (int dx = 0; dx < dw; dx++) {
            int px = ox + dx;
            if (px < 0 || px >= W) continue;
            const uint8_t* p = img + (sy * iw + dx * iw / dw) * 4;
            win->canvas[py * W + px] =
                0xFF000000 | ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
        }
    }
}

static void render_image(gui_window_t* win, const char* filename) {
    int W = (int)win->inner_w;
    int H = (int)win->inner_h;

    // Toolbar + tombol Back di atas area gambar.
    gui_draw_rect(win, 0, 0, W, TOOLBAR_H, HDR_COLOR);
    gui_draw_rect(win, BACK_X, BACK_Y, BACK_W, BACK_H, BTN_COLOR);
    gui_draw_text(win, "< Back", BACK_X + 10, BACK_Y + 4, BTN_TEXT);

    // Latar area gambar (letterbox untuk rasio yang tidak pas).
    gui_draw_rect(win, 0, TOOLBAR_H, W, H - TOOLBAR_H, IMG_BG);

    uint32_t fsize = sys_file_size((char*)filename);
    if (fsize == 0) {
        gui_draw_text(win, "Gagal membaca file.", 15, TOOLBAR_H + 10, BTN_TEXT);
        return;
    }

    uint8_t* raw = (uint8_t*)sys_alloc(fsize);
    if (!raw) {
        gui_draw_text(win, "Memori tidak cukup.", 15, TOOLBAR_H + 10, BTN_TEXT);
        return;
    }
    sys_read_file_to_buffer((char*)filename, (char*)raw);

    int iw, ih, channels;
    uint8_t* pixels = stbi_load_from_memory(raw, (int)fsize, &iw, &ih, &channels, 4);
    sys_free(raw);
    if (!pixels) {
        gui_draw_text(win, "Format PNG tidak didukung.", 15, TOOLBAR_H + 10, BTN_TEXT);
        return;
    }

    // Area gambar dalam koordinat canvas absolut: di bawah titlebar + toolbar.
    int area_y = GUI_TITLEBAR_H + TOOLBAR_H;
    blit_fit(win, pixels, iw, ih, 0, area_y, W, H - TOOLBAR_H);
    sys_free(pixels);
}

static void show_list(gui_window_t* win) {
    mode = MODE_LIST;
    render_list(win);
    gui_flush(win);
}

static void show_image(gui_window_t* win, const char* filename) {
    mode = MODE_IMAGE;
    render_image(win, filename);
    gui_flush(win);
}

// Buka langsung gambar yang dipilih di File Manager (via view.tmp), atau
// tampilkan daftar jika viewer dijalankan tanpa target. Return 1 jika
// sebuah gambar dibuka langsung.
static int open_from_fileman(gui_window_t* win) {
    if (!sys_file_exists("view.tmp")) return 0;

    uint32_t tsize = sys_file_size("view.tmp");
    if (tsize == 0 || tsize >= 63) return 0;

    char target[64];
    sys_read_file_to_buffer("view.tmp", target);
    target[tsize] = '\0';

    // Konsumsi flag sekali pakai: Back akan kembali ke daftar milik viewer,
    // bukan membuka ulang gambar yang sama saat viewer dijalankan lagi.
    fs_delete("view.tmp");

    show_image(win, target);
    return 1;
}

void main(void) {
    gui_window_t* app = gui_create_window("Image Viewer", WIN_W, WIN_H);
    if (!app) { sys_exit(); return; }

    scan_images();

    // Ingat asal peluncuran: hanya kembali ke File Manager bila viewer memang
    // dibuka dari sana (via view.tmp). Bila dijalankan mandiri, tutup ke shell.
    int launched_from_fileman = open_from_fileman(app);
    if (!launched_from_fileman) {
        show_list(app);
    }

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

                if (mode == MODE_LIST) {
                    int y0 = LIST_START_Y;
                    int y1 = y0 + png_count * LIST_ROW_H;
                    if (png_count > 0 &&
                        rel_x >= 10 && rel_x <= (int)app->inner_w - 10 &&
                        rel_y >= y0 && rel_y < y1) {
                        int idx = (rel_y - y0) / LIST_ROW_H;
                        if (idx >= 0 && idx < png_count) {
                            show_image(app, files[png_slots[idx]].filename);
                        }
                    }
                } else {
                    if (rel_x >= BACK_X && rel_x <= BACK_X + BACK_W &&
                        rel_y >= BACK_Y && rel_y <= BACK_Y + BACK_H) {
                        show_list(app);
                    }
                }
            }

            if (ev.type == EVENT_KEY_PRESS && ev.param1 == 27) {
                // ESC di mode gambar kembali ke daftar; di daftar menutup app.
                if (mode == MODE_IMAGE) {
                    show_list(app);
                } else {
                    app->is_running = 0; break;
                }
            }
        }
        sys_yield();
    }

    gui_destroy(app);
    if (launched_from_fileman) {
        sys_exec("fileman.elf");
    } else {
        sys_exit();
    }
}
