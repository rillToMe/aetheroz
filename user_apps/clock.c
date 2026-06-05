// ============================================================
// KYUZEN OS — CLOCK APP
// Jam digital real-time menggunakan RTC.
// Tampilkan HH:MM:SS dan YYYY-MM-DD, update setiap detik.
// Window: 300×160 px, di posisi layar (162, 200)
// ============================================================
#include "userlib.h"
#define FONT8x16_IMPLEMENTATION
#include "font8x16.h"

// --- Ukuran & posisi window ---
static int win_id  = -1;
static int win_w   = 300;
static int win_h   = 160;
static uint32_t* my_canvas = 0;

// ---------- helper gambar lokal ----------
static void fill(int x, int y, int w, int h, uint32_t c) {
    uint32_t col = c | 0xFF000000;
    for (int py = y; py < y + h; py++)
        for (int px = x; px < x + w; px++)
            if (px >= 0 && px < win_w && py >= 0 && py < win_h)
                my_canvas[py * win_w + px] = col;
}

static void put_char(char ch, int x, int y, uint32_t col) {
    if (ch < 0 || ch > 127) return;
    const unsigned char* bm = font8x16[(int)ch];
    uint32_t c = col | 0xFF000000;
    for (int row = 0; row < 16; row++)
        for (int bit = 0; bit < 8; bit++)
            if (bm[row] & (0x80 >> bit)) {
                int px = x + bit, py = y + row;
                if (px >= 0 && px < win_w && py >= 0 && py < win_h)
                    my_canvas[py * win_w + px] = c;
            }
}

static void put_str(const char* s, int x, int y, uint32_t col) {
    for (int i = 0; s[i]; i++) put_char(s[i], x + i * 8, y, col);
}

// Tampilkan angka 2 digit dengan leading zero
static void put_2d(int n, int x, int y, uint32_t col) {
    char buf[3];
    buf[0] = '0' + (n / 10);
    buf[1] = '0' + (n % 10);
    buf[2] = '\0';
    put_str(buf, x, y, col);
}

// Tampilkan angka 4 digit
static void put_4d(int n, int x, int y, uint32_t col) {
    char buf[5];
    buf[0] = '0' + ((n / 1000) % 10);
    buf[1] = '0' + ((n / 100)  % 10);
    buf[2] = '0' + ((n / 10)   % 10);
    buf[3] = '0' + (n % 10);
    buf[4] = '\0';
    put_str(buf, x, y, col);
}

// ---------- Nama hari / bulan ----------
static const char* BULAN[13] = {
    "", "Jan","Feb","Mar","Apr","Mei","Jun","Jul","Agu","Sep","Okt","Nov","Des"
};
static const char* HARI[7] = {
    "Min","Sen","Sel","Rab","Kam","Jum","Sab"
};

// Hitung hari dalam seminggu (Zeller mod)
static int day_of_week(int y, int m, int d) {
    if (m < 3) { m += 12; y--; }
    int k = y % 100, j = y / 100;
    int h = (d + (13*(m+1))/5 + k + k/4 + j/4 + 5*j) % 7;
    return (h + 6) % 7; // 0=Min
}

// ---------- Render satu frame ----------
static void render(uint32_t* t) {
    // Warna tema: dark navy + aksen cyan
    uint32_t BG_TITLEBAR = 0x1A237E; // indigo gelap
    uint32_t BG_BODY     = 0x121212; // hitam pekat
    uint32_t BG_CLOCK    = 0x1E1E2E; // dark panel
    uint32_t COL_WHITE   = 0xFFFFFF;
    uint32_t COL_CYAN    = 0x00E5FF;
    uint32_t COL_GRAY    = 0x888888;
    uint32_t COL_RED     = 0xEF5350;
    uint32_t COL_SEP     = 0x00BFA5;  // teal untuk separator ":"

    // Bersihkan
    fill(0, 0, win_w, win_h, BG_BODY);

    // ---- Title bar ----
    fill(0, 0, win_w, 28, BG_TITLEBAR);
    put_str("  CLOCK", 4, 6, COL_WHITE);

    // Tombol X
    fill(win_w - 36, 0, 36, 28, COL_RED);
    put_str(" X", win_w - 28, 6, COL_WHITE);

    // ---- Panel jam ----
    fill(10, 36, win_w - 20, 72, BG_CLOCK);

    // HH : MM : SS — ukuran 3x (scale dengan 3 karakter berdampingan)
    // Gunakan skala 3x: tiap karakter 8px → 24px lebar, 16px → 48px tinggi
    // Kita simulasi dengan menggambar 3 kali per pixel
    int jam   = t[3], mnt = t[4], dtk = t[5];
    int cx = 18, cy = 46;

    // Gambar digit besar (scale 2x horizontal, 3x vertikal) di panel jam
    // HH
    char buf[9];
    buf[0] = '0' + (jam / 10); buf[1] = '0' + (jam % 10);
    buf[2] = ':';
    buf[3] = '0' + (mnt / 10); buf[4] = '0' + (mnt % 10);
    buf[5] = ':';
    buf[6] = '0' + (dtk / 10); buf[7] = '0' + (dtk % 10);
    buf[8] = '\0';

    // Gambar jam besar: scale 2.5x dengan menggambar setiap bit 2x lebar 3x tinggi
    for (int ci = 0; buf[ci]; ci++) {
        char ch = buf[ci];
        if (ch < 0 || ch > 127) continue;
        const unsigned char* bm = font8x16[(int)ch];
        uint32_t col = (ch == ':') ? COL_SEP : COL_CYAN;
        col |= 0xFF000000;
        int ox = cx + ci * 20;
        for (int row = 0; row < 16; row++) {
            for (int bit = 0; bit < 8; bit++) {
                if (bm[row] & (0x80 >> bit)) {
                    // 2x width, 2x height
                    int px0 = ox + bit * 2;
                    int py0 = cy + row * 2;
                    for (int dy = 0; dy < 2; dy++)
                        for (int dx = 0; dx < 2; dx++) {
                            int px = px0 + dx, py = py0 + dy;
                            if (px >= 0 && px < win_w && py >= 0 && py < win_h)
                                my_canvas[py * win_w + px] = col;
                        }
                }
            }
        }
    }

    // ---- Tanggal ----
    int y_date = 116;
    int dow = day_of_week(t[0], t[1], t[2]);

    // "Sen, 05 Jun 2026"
    put_str(HARI[dow], 16, y_date, COL_GRAY);
    put_str(",", 40, y_date, COL_GRAY);

    put_2d(t[2], 56, y_date, COL_WHITE);    // DD
    put_str(" ", 72, y_date, COL_WHITE);
    put_str(BULAN[t[1]], 80, y_date, COL_CYAN);  // Bulan
    put_str(" ", 104, y_date, COL_WHITE);
    put_4d(t[0], 112, y_date, COL_WHITE);    // YYYY

    // ---- Garis bawah tipis ----
    fill(10, win_h - 8, win_w - 20, 2, 0x1A237E);

    sys_update_window(win_id, my_canvas);
}

// ---------- Main ----------
void main(void) {
    win_id = sys_create_window(162, 200, win_w, win_h);
    if (win_id < 0) return;

    my_canvas = (uint32_t*)sys_alloc(win_w * win_h * 4);
    if (!my_canvas) { sys_destroy_window(win_id); return; }

    uint32_t waktu[6];
    uint32_t last_sec = 0xFF; // Paksa render pertama

    kyuzen_event_t ev;
    int mouse_x = 0, mouse_y = 0;
    int running = 1;

    while (running) {
        // Poll waktu: update render kalau detik berubah
        sys_get_time(waktu);
        if (waktu[5] != last_sec) {
            last_sec = waktu[5];
            render(waktu);
        }

        // Poll event
        if (sys_get_event(&ev)) {
            if (ev.type == EVENT_MOUSE_MOVE) {
                mouse_x = ev.param1;
                mouse_y = ev.param2;
            }
            if (ev.type == EVENT_MOUSE_CLICK && ev.param1 == 0 && ev.param2 == 1) {
                if (ev.param3 != 0) mouse_x = ev.param3;
                // Koordinat relatif terhadap window (posisi: 162, 200)
                int rx = mouse_x - 162;
                int ry = mouse_y - 200;
                // Klik tombol X: pojok kanan atas
                if (rx >= (win_w - 36) && rx <= win_w && ry >= 0 && ry <= 28)
                    running = 0;
            }
            if (ev.type == EVENT_KEY_PRESS && ev.param1 == 27) // ESC
                running = 0;
        }

        sys_yield();
    }

    sys_destroy_window(win_id);
    sys_free(my_canvas);
}
