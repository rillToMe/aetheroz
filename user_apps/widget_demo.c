// user_apps/widget_demo.c — demo Toolkit libui dari sisi C ABI.
//
// Phase 6: window -> vbox(label "Klik: N", button "+1") — klik menaikkan
// counter via callback C. Phase 7: tambah widget standar — TextBox (fokus
// keyboard + Enter echo), CheckBox, Slider -> ProgressBar (callback change),
// dan Image (PNG dari KyuzenFS). Semua lewat API C murni, tanpa C++ bocor.
//
// Build: widget_demo.o + userlib.o + libgui.o + libui.o + png.o

#include "userlib.h"
#include "libui.h"

static ui_widget_t* lbl;        // counter "+1" (regression Phase 6)
static ui_widget_t* teks_lbl;   // echo isi TextBox saat Enter
static ui_widget_t* tb;
static ui_widget_t* slider;
static ui_widget_t* progress;
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

static void on_enter(void* userdata) {
    (void)userdata;
    const char* t = ui_textbox_text(tb);
    const char* pre = "Teks: ";
    char buf[72];
    int k = 0;
    while (pre[k]) { buf[k] = pre[k]; k++; }
    int i = 0;
    while (t[i] && k < 70) buf[k++] = t[i++];
    buf[k] = '\0';
    ui_label_set_text(teks_lbl, buf);
}

static void on_slider(void* userdata) {
    (void)userdata;
    ui_progressbar_set_value(progress, ui_slider_value(slider));
}

void main(void) {
    ui_window_t* win = ui_window_create(340, 380);
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

    // --- regression Phase 6: counter ---
    lbl = ui_label_create(win, "Klik: 0");
    ui_layout_add(box, lbl);
    ui_widget_t* btn = ui_button_create(win, "+1");
    ui_button_set_click(btn, on_plus, 0);
    ui_layout_add(box, btn);

    // --- TextBox: fokus keyboard, Enter -> echo ---
    teks_lbl = ui_label_create(win, "Teks: ");
    ui_layout_add(box, teks_lbl);
    tb = ui_textbox_create(win, 160);
    ui_textbox_set_enter(tb, on_enter, 0);
    ui_layout_add(box, tb);

    // --- CheckBox ---
    ui_widget_t* cb = ui_checkbox_create(win, "centang");
    ui_layout_add(box, cb);

    // --- Slider -> ProgressBar (callback change) ---
    slider = ui_slider_create(win, 0, 100);
    ui_slider_set_change(slider, on_slider, 0);
    ui_layout_add(box, slider);
    progress = ui_progressbar_create(win, 160);
    ui_layout_add(box, progress);

    // --- Image: PNG dari KyuzenFS ---
    ui_widget_t* img = ui_image_create(win, "kyuzen.png", 64, 64);
    ui_layout_add(box, img);

    ui_window_add(win, box);

    ui_window_run(win);          // blocking; keluar via X titlebar / ESC
    ui_window_destroy(win);
    sys_exit();
}
