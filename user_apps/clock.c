// ============================================================
// clock.c — Jam (Phase 10): jam digital + tanggal (libui).
// Refresh 1×/dtk lewat ui_window_set_tick (render hanya saat
// detik berubah). Tanpa infra timer kernel — tick dibangkitkan
// event loop (~60/s) dan ditolak bila detik belum berubah.
//
// Build: clock.o + userlib.o + libgui.o + libui.o + png.o
// ============================================================
#include "userlib.h"
#include "libui.h"

static ui_widget_t* g_time;
static ui_widget_t* g_date;
static uint32_t last_sec = 0xFF;

static const char* BULAN[13] = {
    "", "Jan","Feb","Mar","Apr","Mei","Jun","Jul","Agu","Sep","Okt","Nov","Des"
};
static const char* HARI[7] = {
    "Min","Sen","Sel","Rab","Kam","Jum","Sab"
};

static int day_of_week(int y, int m, int d) {
    if (m < 3) { m += 12; y--; }
    int k = y % 100, j = y / 100;
    int h = (d + (13*(m+1))/5 + k + k/4 + j/4 + 5*j) % 7;
    return (h + 6) % 7;
}

static void fmt2(uint32_t v, char* b) { b[0]='0'+(v/10); b[1]='0'+(v%10); b[2]='\0'; }

// "HH:MM:SS" + tanggal "Rab, 06 Agu 2026" — pusatkan dgn spasi depan.
static int tick(void* userdata) {
    (void)userdata;
    uint32_t t[6];   // [year, month, day, hour, min, sec]
    sys_get_time(t);
    if (t[5] == last_sec && last_sec != 0xFF) return 0;   // detik sama → tetap
    last_sec = t[5];

    char hh[3], mm[3], ss[3];
    fmt2(t[3], hh); fmt2(t[4], mm); fmt2(t[5], ss);
    char tb[20];
    int k = 0;
    for (int i = 0; i < 6; i++) tb[k++] = ' ';   // pusatkan di window 260px
    tb[k++] = hh[0]; tb[k++] = hh[1]; tb[k++] = ':';
    tb[k++] = mm[0]; tb[k++] = mm[1]; tb[k++] = ':';
    tb[k++] = ss[0]; tb[k++] = ss[1];
    tb[k] = '\0';
    ui_label_set_text(g_time, tb);

    char dd[3]; fmt2(t[2], dd);
    char db[24];
    k = 0;
    for (int i = 0; i < 8; i++) db[k++] = ' ';
    const char* d = HARI[day_of_week(t[0], t[1], t[2])];
    for (int i = 0; d[i] && k < 14; i++) db[k++] = d[i];
    db[k++] = ','; db[k++] = ' ';
    db[k++] = dd[0]; db[k++] = dd[1]; db[k++] = ' ';
    const char* m = BULAN[t[1]];
    for (int i = 0; m[i] && k < 21; i++) db[k++] = m[i];
    db[k++] = ' ';
    db[k++] = '0' + (t[0]/1000)%10; db[k++] = '0' + (t[0]/100)%10;
    db[k++] = '0' + (t[0]/10)%10;   db[k++] = '0' + t[0]%10;
    db[k] = '\0';
    ui_label_set_text(g_date, db);
    return 1;
}

void main(void) {
    ui_window_t* win = ui_window_create(260, 120);
    if (!win) { sys_exit(); }
    ui_window_set_title(win, "Jam");

    ui_widget_t* box = ui_vbox_create(win, 8);
    g_time = ui_label_create(win, "--:--:--");
    ui_layout_add(box, g_time);
    g_date = ui_label_create(win, "");
    ui_layout_add(box, g_date);
    ui_window_add(win, box);

    ui_window_set_tick(win, tick, 0);
    tick(0);   // render nilai awal (sebelum detik pertama berubah)
    ui_window_run(win);   // blocking; keluar via X / ESC
    ui_window_destroy(win);
    sys_exit();
}
