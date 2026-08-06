// user_apps/widget_demo.c — demo Toolkit (Phase 6) dari sisi C ABI.
//
// Membuktikan aplikasi C bisa memakai libui (Modern C++ internal)
// lewat API C murni tanpa bocor C++ ke app. Alur:
//   window -> vbox(label "Klik: N", button "+1")
//   klik "+1" -> counter++ -> label di-update (ui_label_set_text)
//
// Build: widget_demo.o + userlib.o + libgui.o + libui.o
//       (libui.o = apps/libui.cpp dikompilasi clang++)

#include "userlib.h"
#include "libui.h"

static ui_widget_t* lbl;
static int count = 0;

static void itoa(int n, char* buf) {
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[16]; int i = 0;
    while (n > 0 && i < 15) { tmp[i++] = '0' + (n % 10); n /= 10; }
    int j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

static void on_plus(void* userdata) {
    (void)userdata;
    count++;
    char buf[24];
    const char* pre = "Klik: ";
    int k = 0;
    while (pre[k]) { buf[k] = pre[k]; k++; }
    itoa(count, buf + k);
    ui_label_set_text(lbl, buf);
}

void main(void) {
    ui_window_t* win = ui_window_create(280, 140);
    if (!win) { sys_exit(); }

    ui_theme_t th;
    th.bg           = 0x121212;
    th.fg           = 0xE0E0E0;
    th.accent       = 0xE94560;
    th.button_bg    = 0x0F3460;
    th.button_fg    = 0xFFFFFF;
    th.button_hover = 0x2A4A7E;
    ui_window_set_theme(win, &th);

    ui_widget_t* box = ui_vbox_create(win, 10);

    lbl = ui_label_create(win, "Klik: 0");
    ui_layout_add(box, lbl);

    ui_widget_t* btn = ui_button_create(win, "+1");
    ui_button_set_click(btn, on_plus, 0);
    ui_layout_add(box, btn);

    ui_window_add(win, box);

    ui_window_run(win);          // blocking; keluar via X titlebar / ESC
    ui_window_destroy(win);
    sys_exit();
}
