// ============================================================
// clock.c — Kyuzen Clock App (libgui standard)
// Jam digital real-time menggunakan RTC.
// Tampilkan HH:MM:SS dan tanggal, update setiap detik.
// ============================================================
#include "userlib.h"
#include "libgui.h"

// Warna tema
#define BG_BODY    0x121212
#define BG_CLOCK   0x1E1E2E
#define COL_CYAN   0x00E5FF
#define COL_SEP    0x00BFA5
#define COL_WHITE  0xFFFFFF
#define COL_GRAY   0x888888

// Nama hari / bulan (Indonesia)
static const char* BULAN[13] = {
    "", "Jan","Feb","Mar","Apr","Mei","Jun","Jul","Agu","Sep","Okt","Nov","Des"
};
static const char* HARI[7] = {
    "Min","Sen","Sel","Rab","Kam","Jum","Sab"
};

// Hitung hari dalam seminggu (Zeller)
static int day_of_week(int y, int m, int d) {
    if (m < 3) { m += 12; y--; }
    int k = y % 100, j = y / 100;
    int h = (d + (13*(m+1))/5 + k + k/4 + j/4 + 5*j) % 7;
    return (h + 6) % 7;
}

// State waktu global
static uint32_t waktu[6];
static uint32_t last_sec = 0xFF;

// Gambar digit jam 2x scale langsung ke canvas (koordinat absolut dalam canvas)
static void draw_scaled_char(gui_window_t* win, char ch, int ox, int oy,
                              uint32_t col, int scale) {
    if (ch < 0 || ch > 127) return;
    // Gunakan font8x16 — diekspos oleh libgui via canvas manipulation
    // Kita tulis pixel langsung ke win->canvas
    // (font extern di-link dari libgui.o)
    extern const unsigned char font8x16[256][16];
    const unsigned char* bm = font8x16[(int)(unsigned char)ch];
    uint32_t solid = col | 0xFF000000;
    int W = (int)win->width, H = (int)win->height;
    for (int row = 0; row < 16; row++)
        for (int bit = 0; bit < 8; bit++)
            if (bm[row] & (0x80 >> bit))
                for (int dy = 0; dy < scale; dy++)
                    for (int dx = 0; dx < scale; dx++) {
                        int px = ox + bit*scale + dx;
                        int py = oy + row*scale + dy;
                        if (px >= 0 && px < W && py >= 0 && py < H)
                            win->canvas[py * W + px] = solid;
                    }
}

static void put_str_abs(gui_window_t* win, const char* s, int x, int y, uint32_t col) {
    for (int i = 0; s[i]; i++) {
        extern const unsigned char font8x16[256][16];
        char ch = s[i];
        if (ch < 0 || ch > 127) continue;
        const unsigned char* bm = font8x16[(int)(unsigned char)ch];
        uint32_t solid = col | 0xFF000000;
        int W = (int)win->width, H = (int)win->height;
        for (int row = 0; row < 16; row++)
            for (int bit = 0; bit < 8; bit++)
                if (bm[row] & (0x80 >> bit)) {
                    int px = x + i*8 + bit, py = y + row;
                    if (px >= 0 && px < W && py >= 0 && py < H)
                        win->canvas[py*W+px] = solid;
                }
    }
}

static void fmt_2d(int n, char* buf) {
    buf[0] = '0' + (n / 10); buf[1] = '0' + (n % 10); buf[2] = '\0';
}
static void fmt_4d(int n, char* buf) {
    buf[0] = '0' + ((n/1000)%10); buf[1] = '0' + ((n/100)%10);
    buf[2] = '0' + ((n/10)%10);   buf[3] = '0' + (n%10);
    buf[4] = '\0';
}

void clock_render(gui_window_t* win) {
    int W = (int)win->width;
    int TH = GUI_TITLEBAR_H;

    // Background isi
    gui_draw_rect(win, 0, 0, W, (int)win->inner_h, BG_BODY);

    // Panel jam
    gui_draw_rect(win, 10, 8, W - 20, 72, BG_CLOCK);

    // HH:MM:SS dalam 2x scale (absolut di bawah titlebar)
    int jam = waktu[3], mnt = waktu[4], dtk = waktu[5];
    char timebuf[9];
    timebuf[0] = '0' + (jam/10); timebuf[1] = '0' + (jam%10);
    timebuf[2] = ':';
    timebuf[3] = '0' + (mnt/10); timebuf[4] = '0' + (mnt%10);
    timebuf[5] = ':';
    timebuf[6] = '0' + (dtk/10); timebuf[7] = '0' + (dtk%10);
    timebuf[8] = '\0';

    int ox = 18, oy = TH + 18;
    for (int ci = 0; timebuf[ci]; ci++) {
        uint32_t col = (timebuf[ci] == ':') ? COL_SEP : COL_CYAN;
        draw_scaled_char(win, timebuf[ci], ox + ci*20, oy, col, 2);
    }

    // Tanggal
    int dow = day_of_week(waktu[0], waktu[1], waktu[2]);
    char buf2d[3], buf4d[5];
    fmt_2d(waktu[2], buf2d);
    fmt_4d(waktu[0], buf4d);

    put_str_abs(win, HARI[dow], 16, TH + 88, COL_GRAY);
    put_str_abs(win, ",", 40, TH + 88, COL_GRAY);
    put_str_abs(win, buf2d, 56, TH + 88, COL_WHITE);
    put_str_abs(win, BULAN[waktu[1]], 80, TH + 88, COL_CYAN);
    put_str_abs(win, buf4d, 112, TH + 88, COL_WHITE);

    // Garis bawah
    gui_draw_rect(win, 10, (int)win->inner_h - 8, W - 20, 2, 0x1A237E);
}

void main(void) {
    gui_window_t* app = gui_create_window("Clock", 300, 160);
    if (!app) { sys_exit(); return; }

    gui_set_render(app, clock_render);

    // Custom mainloop: cek waktu, render hanya jika detik berubah
    kyuzen_event_t ev;
    while (app->is_running) {
        sys_get_time(waktu);
        if (waktu[5] != last_sec) {
            last_sec = waktu[5];
            clock_render(app);
            gui_flush(app);
        }

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
                    app->is_running = 0;
                }
            }
            if (ev.type == EVENT_KEY_PRESS && ev.param1 == 27)
                app->is_running = 0;
        }
        sys_yield();
    }

    gui_destroy(app);
    sys_exit();
}
