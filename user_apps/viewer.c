// ============================================================
// viewer.c — Kyuzen Image Viewer (libgui standard)
// Tampilkan file PNG dari disk menggunakan STB_IMAGE.
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

void main(void) {
    // 1. Baca view.tmp untuk tahu file yang diklik
    if (!sys_file_exists("view.tmp")) { sys_exit(); return; }

    char target_png[64];
    uint32_t tmp_size = sys_file_size("view.tmp");
    if (tmp_size == 0 || tmp_size >= 63) { sys_exit(); return; }

    sys_read_file_to_buffer("view.tmp", target_png);
    target_png[tmp_size] = '\0';

    // 2. Baca PNG dari disk
    uint32_t img_filesize = sys_file_size(target_png);
    if (img_filesize == 0) { sys_exit(); return; }

    uint8_t* file_buffer = (uint8_t*)sys_alloc(img_filesize);
    if (!file_buffer) { sys_exit(); return; }
    sys_read_file_to_buffer(target_png, (char*)file_buffer);

    // 3. Decode PNG
    int img_w, img_h, channels;
    uint8_t* img_data = stbi_load_from_memory(file_buffer, img_filesize,
                                               &img_w, &img_h, &channels, 4);
    sys_free(file_buffer);
    if (!img_data) { sys_exit(); return; }

    // 4. Hitung ukuran window
    int win_w = img_w < 200 ? 200 : img_w;
    int win_h = img_h + GUI_TITLEBAR_H;

    // 5. Buat window via libgui
    gui_window_t* app = gui_create_window(target_png, win_w, win_h);
    if (!app) { sys_free(img_data); sys_exit(); return; }

    // 6. Tumpahkan pixel gambar langsung ke canvas (di bawah title bar)
    int offset_x = (win_w - img_w) / 2;
    int index = 0;
    int W = (int)app->width;
    int H = (int)app->height;
    for (int y = 0; y < img_h; y++) {
        for (int x = 0; x < img_w; x++) {
            uint8_t r = img_data[index++];
            uint8_t g = img_data[index++];
            uint8_t b = img_data[index++];
            uint8_t a = img_data[index++];
            if (a > 0) {
                uint32_t color = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
                int px = x + offset_x;
                int py = y + GUI_TITLEBAR_H;
                if (px >= 0 && px < W && py >= 0 && py < H)
                    app->canvas[py * W + px] = color;
            }
        }
    }
    sys_free(img_data);

    // 7. Flush ke layar
    gui_flush(app);

    // 8. Event loop — hanya tunggu Close / ESC
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
                if (rfx >= (int)app->width - GUI_CLOSE_BTN_W &&
                    rfx <  (int)app->width &&
                    rfy >= 0 && rfy < GUI_TITLEBAR_H) {
                    app->is_running = 0; break;
                }
            }
            if (ev.type == EVENT_KEY_PRESS && ev.param1 == 27) {
                app->is_running = 0; break;
            }
        }
        sys_yield();
    }

    // 9. Kembali ke File Manager
    gui_destroy(app);
    sys_exec("fileman.elf");
}
