// apps/libui.cpp — Widget Toolkit (Phase 6): Modern C++ di atas libgui (C)
//
// Arsitektur (sesuai keputusan user 2026-08-06):
//   Application (C / Rust / Zig / ...)
//     -> Public GUI C ABI (include/libui.h, opaque handles)
//     -> extern "C" wrappers (bagian bawah file ini)
//     -> Modern C++ Toolkit  (namespace ui)
//     -> libgui renderer (C, apps/libgui.c)
//     -> framebuffer / graphics driver
//
// Batasan toolchain bare-metal:
//   - TIDAK ada libstdc++/libc++ -> operator new/delete di-stub ke
//     sys_alloc/sys_free (userlib.o).
//   - -fno-exceptions -fno-rtti -> __cxa_pure_virtual di-stub.
//   - ELF loader TIDAK menjalankan .init_array -> TIDAK ada global/static
//     C++ object dengan constructor non-trivial. Semua object dibuat via
//     new pada waktu jalan (runtime), aman.
//   - vtables masuk .rodata (PT_LOAD R-X, app.ld) — relokasi selesai di
//     link time (non-PIE, base tetap 0x4000000), tidak butuh runtime reloc.
//   - userlib.h/libgui.h tidak punya extern "C" guard -> dibungkus di sini.

#include <stdint.h>
#include <stddef.h>

extern "C" {
#include "userlib.h"
#include "libgui.h"
}

// libui.h punya guard extern "C" sendiri — aman di-include dari C++.
#include "libui.h"

// Dekoder PNG bersama (apps/png.c, stb_image) — dilink oleh app yang memakai
// Image widget. Di-declare extern "C" karena png.c adalah file C.
extern "C" uint32_t* png_decode(const char* filename, int* out_w, int* out_h);
extern "C" void png_free(uint32_t* buf);

namespace {

// ------------------------------------------------------------
// Runtime shim: C++ memori -> syscalls KyuzenOS
// ------------------------------------------------------------
void  _ui_free(void* p)     { if (p) sys_free(p); }

int _ui_strlen(const char* s) { int n = 0; while (s[n]) n++; return n; }

// Copy string — toolkit OWNS salinannya (caller boleh pakai stack buffer).
char* _ui_strdup(const char* s) {
    int n = _ui_strlen(s) + 1;
    char* d = (char*)sys_alloc(n);
    if (!d) return 0;
    for (int i = 0; i < n; i++) d[i] = s[i];
    return d;
}

} // namespace

// C++ operator new/delete (global scope, bukan namespace) -> sys_alloc/sys_free.
void* operator new(unsigned long n)              { return sys_alloc((uint32_t)n); }
void* operator new[](unsigned long n)            { return sys_alloc((uint32_t)n); }
void  operator delete(void* p) noexcept          { if (p) sys_free(p); }
void  operator delete[](void* p) noexcept        { if (p) sys_free(p); }
void  operator delete(void* p, unsigned long) noexcept   { if (p) sys_free(p); }
void  operator delete[](void* p, unsigned long) noexcept { if (p) sys_free(p); }

// Dipanggil kalau vtable class abstrak terpanggil (bug) — jangan kembali.
extern "C" void __cxa_pure_virtual() { for (;;) {} }

namespace ui {

// ------------------------------------------------------------
// Theme — layout field identik ui_theme_t (C ABI)
// ------------------------------------------------------------
struct Theme {
    uint32_t bg, fg, accent, button_bg, button_fg, button_hover;
    Theme() : bg(0x1A1A2E), fg(0xE0E0E0), accent(0xE94560),
              button_bg(0x0F3460), button_fg(0xFFFFFF), button_hover(0x2A4A7E) {}
};

// ------------------------------------------------------------
// Painter — satu-satunya jembatan widget -> renderer (libgui C)
// ------------------------------------------------------------
class Painter {
public:
    gui_window_t* win;
    const Theme& theme;
    Painter(gui_window_t* w, const Theme& t) : win(w), theme(t) {}
    void rect(int x, int y, int w, int h, uint32_t c) {
        gui_draw_rect(win, x, y, w, h, c | 0xFF000000);
    }
    void text(const char* s, int x, int y, uint32_t c) {
        gui_draw_text(win, s, x, y, c | 0xFF000000);
    }
    // Blit PNG XRGB8888 (px = iw×ih) diskalakan nearest-neighbor ke rect
    // (x,y,w,h). Menulis win->canvas langsung (libgui tak punya draw-image) —
    // pola yang sama dengan blit_fit viewer.c. ponytail: tanpa clip, rect
    // widget selalu di dalam window (layout root bermargin 8px).
    void image(int x, int y, int w, int h, const uint32_t* px, int iw, int ih) {
        int cw = (int)win->width;
        for (int py = 0; py < h; py++) {
            int sy = py * ih / h;
            for (int q = 0; q < w; q++) {
                int sx = q * iw / w;
                win->canvas[(y + py) * cw + x + q] = px[sy * iw + sx];
            }
        }
    }
};

// ------------------------------------------------------------
// Widget — basis pohon. Koordinat window-local konten.
// ------------------------------------------------------------
class Widget {
public:
    int x, y, w, h;
    bool visible;
    bool has_focus;          // diset Window saat fokus keyboard intra-window
    ui_click_cb click_cb;
    void* userdata;

    Widget() : x(0), y(0), w(0), h(0), visible(true), has_focus(false),
              click_cb(0), userdata(0) {}
    virtual ~Widget() {}
    virtual void draw(Painter& p) = 0;
    virtual void set_hover(bool on) { (void)on; }
    virtual void set_focus(bool on) { has_focus = on; }
    virtual bool focusable() { return false; }   // TextBox → true
    // Hit-test: widget paling dalam yang memuat (mx,my), atau 0.
    virtual Widget* pick(int mx, int my) {
        if (!visible) return 0;
        return (mx >= x && mx < x + w && my >= y && my < y + h) ? this : 0;
    }
    virtual void on_click(int mx, int my) {
        (void)mx; (void)my;
        if (click_cb) click_cb(userdata);
    }
    // Drag: dipanggil tiap MOUSE_MOVE selama mouse ditekan di widget ini
    // (grabbed). Return true = perlu redraw. on_release = tombol dilepas.
    virtual bool on_drag(int mx, int my) { (void)mx; (void)my; return false; }
    virtual void on_release() {}
    // Keyboard: hanya dipanggil bila widget ini yang punya fokus. ascii dari
    // P1 (0 = non-printable), scancode dari P3 (Backspace 0x0E, Enter 0x1C).
    virtual void on_key(uint8_t ascii, uint32_t scancode, uint32_t mods) {
        (void)ascii; (void)scancode; (void)mods;
    }
    void set_click(ui_click_cb cb, void* u) { click_cb = cb; userdata = u; }
};

// ------------------------------------------------------------
// Label — teks statis, lebar otomatis = panjang * 8px
// ------------------------------------------------------------
class Label : public Widget {
public:
    char* text;
    Label(const char* t) : text(_ui_strdup(t)) { w = _ui_strlen(text) * 8; h = 16; }
    virtual ~Label() { _ui_free(text); }
    void set_text(const char* t) {
        char* n = _ui_strdup(t);
        if (!n) return;
        _ui_free(text);
        text = n;
        w = _ui_strlen(text) * 8;
    }
    virtual void draw(Painter& p) override { p.text(text, x, y, p.theme.fg); }
};

// ------------------------------------------------------------
// Button — rect solid + label, hover state, klik -> callback
// ------------------------------------------------------------
class Button : public Widget {
public:
    char* text;
    bool hover;
    Button(const char* t) : text(_ui_strdup(t)), hover(false) { w = _ui_strlen(text) * 8 + 16; h = 24; }
    virtual ~Button() { _ui_free(text); }
    virtual void draw(Painter& p) override {
        p.rect(x, y, w, h, hover ? p.theme.button_hover : p.theme.button_bg);
        p.text(text, x + 8, y + 4, p.theme.button_fg);
    }
    virtual void set_hover(bool on) override { hover = on; }
};

// ------------------------------------------------------------
// TextBox — input satu baris; fokus keyboard via klik (Phase 7)
// ------------------------------------------------------------
class TextBox : public Widget {
public:
    enum { MAX_TEXT = 256 };
    char text[MAX_TEXT];
    int cur;                    // posisi kursor (indeks karakter)
    ui_click_cb enter_cb;
    void* enter_data;

    TextBox(int width) : cur(0), enter_cb(0), enter_data(0) {
        w = width; h = 24;
        text[0] = '\0';
    }
    void set_text(const char* t) {
        int n = 0; while (t[n] && n < MAX_TEXT - 1) n++;
        for (int i = 0; i < n; i++) text[i] = t[i];
        text[n] = '\0';
        cur = n;
    }
    virtual bool focusable() override { return true; }
    virtual void draw(Painter& p) override {
        p.rect(x, y, w, h, p.theme.button_bg);
        uint32_t border = has_focus ? p.theme.accent : p.theme.fg;
        p.rect(x, y, w, 1, border);
        p.rect(x, y + h - 1, w, 1, border);
        p.rect(x, y, 1, h, border);
        p.rect(x + w - 1, y, 1, h, border);
        p.text(text, x + 4, y + 4, p.theme.fg);
        if (has_focus) p.rect(x + 4 + cur * 8, y + 4, 1, 16, p.theme.accent);
    }
    virtual void on_key(uint8_t ascii, uint32_t scancode, uint32_t mods) override {
        (void)mods;
        if (ascii >= 32) {                       // printable → sisipkan
            if (cur < MAX_TEXT - 1) { text[cur++] = (char)ascii; text[cur] = '\0'; }
        } else if (scancode == 0x0E) {           // Backspace
            if (cur > 0) text[--cur] = '\0';
        } else if (scancode == 0x1C && enter_cb) {   // Enter
            enter_cb(enter_data);
        }
    }
};

// ------------------------------------------------------------
// CheckBox — kotak centang + label; klik toggle (Phase 7)
// ------------------------------------------------------------
class CheckBox : public Widget {
public:
    char* label;
    bool checked;
    ui_click_cb toggle_cb;
    void* toggle_data;

    CheckBox(const char* t) : label(_ui_strdup(t)), checked(false),
                              toggle_cb(0), toggle_data(0) {
        w = _ui_strlen(label) * 8 + 20; h = 20;
    }
    virtual ~CheckBox() { _ui_free(label); }
    void set_checked(bool c) { checked = c; }
    virtual void on_click(int mx, int my) override {
        (void)mx; (void)my;
        checked = !checked;
        if (toggle_cb) toggle_cb(toggle_data);
    }
    virtual void draw(Painter& p) override {
        p.rect(x, y, 12, 12, p.theme.button_bg);
        p.rect(x, y, 12, 1, p.theme.fg);
        p.rect(x, y + 11, 12, 1, p.theme.fg);
        p.rect(x, y, 1, 12, p.theme.fg);
        p.rect(x + 11, y, 1, 12, p.theme.fg);
        if (checked) {                            // centang diagonal accent
            for (int i = 0; i < 4; i++) p.rect(x + 2 + i, y + 6 + i, 1, 1, p.theme.accent);
            for (int i = 0; i < 6; i++) p.rect(x + 6 + i, y + 9 - i, 1, 1, p.theme.accent);
        }
        p.text(label, x + 20, y + 2, p.theme.fg);
    }
};

// ------------------------------------------------------------
// Slider — track + handle yang bisa diseret (Phase 7)
// ------------------------------------------------------------
class Slider : public Widget {
public:
    int min, max, val;
    bool dragging;
    ui_click_cb change_cb;
    void* change_data;

    Slider(int mn, int mx) : min(mn), max(mx), val(mn), dragging(false),
                             change_cb(0), change_data(0) {
        w = 160; h = 20;
        if (max <= min) max = min + 1;
    }
    void set_value(int v) {
        if (v < min) v = min;
        if (v > max) v = max;
        val = v;
    }
    void clamp_to(int mx) {
        int span = max - min;
        int nw = w - 8;
        set_value(min + (mx - x) * span / nw);   // mx - x = posisi dalam widget
    }
    virtual bool on_drag(int mx, int my) override {
        (void)my;
        if (!dragging) return false;
        int old = val;
        clamp_to(mx);
        if (val != old && change_cb) change_cb(change_data);
        return true;
    }
    virtual void on_release() override { dragging = false; }
    virtual void on_click(int mx, int my) override {
        (void)my;
        dragging = true;
        int old = val;
        clamp_to(mx);
        if (val != old && change_cb) change_cb(change_data);
    }
    virtual void draw(Painter& p) override {
        p.rect(x, y + h / 2 - 2, w, 4, p.theme.button_bg);
        int span = max - min;
        int hx = span ? (val - min) * (w - 8) / span : 0;
        p.rect(x + hx, y, 8, h, p.theme.accent);
    }
};

// ------------------------------------------------------------
// ProgressBar — fill horizontal read-only, 0..100 (Phase 7)
// ------------------------------------------------------------
class ProgressBar : public Widget {
public:
    int val;
    ProgressBar(int width) : val(0) { w = width; h = 16; }
    void set_value(int v) {
        if (v < 0) v = 0;
        if (v > 100) v = 100;
        val = v;
    }
    virtual void draw(Painter& p) override {
        p.rect(x, y, w, h, p.theme.button_bg);
        int fw = val * w / 100;
        if (fw > 0) p.rect(x, y, fw, h, p.theme.accent);
    }
};

// ------------------------------------------------------------
// Image — PNG dari KyuzenFS, nearest-neighbor ke rect (Phase 7)
// ------------------------------------------------------------
class Image : public Widget {
public:
    uint32_t* px;
    int iw, ih;
    Image(const char* filename, int dw, int dh) : px(0), iw(0), ih(0) {
        w = dw; h = dh;
        px = png_decode(filename, &iw, &ih);
    }
    virtual ~Image() { png_free(px); }
    virtual void draw(Painter& p) override {
        if (!px || iw <= 0 || ih <= 0) { p.rect(x, y, w, h, p.theme.button_bg); return; }
        p.image(x, y, w, h, px, iw, ih);
    }
};

// ------------------------------------------------------------
// Layout — kontainer widget; hit-test anak topmost-first
// ------------------------------------------------------------
class Layout : public Widget {
public:
    enum { MAX_CHILDREN = 16 };
    Widget* children[MAX_CHILDREN];
    int count;

    Layout() : count(0) { for (int i = 0; i < MAX_CHILDREN; i++) children[i] = 0; }
    virtual ~Layout() { for (int i = 0; i < count; i++) delete children[i]; }

    void add(Widget* c) { if (count < MAX_CHILDREN) children[count++] = c; }
    virtual void arrange() = 0;

    virtual void draw(Painter& p) override {
        arrange();
        for (int i = 0; i < count; i++) children[i]->draw(p);
    }
    virtual Widget* pick(int mx, int my) override {
        if (!visible) return 0;
        for (int i = count - 1; i >= 0; i--) {
            Widget* r = children[i]->pick(mx, my);
            if (r) return r;
        }
        return 0;
    }
};

// ------------------------------------------------------------
// VBox — susun anak vertikal berurutan, rata kiri
// ------------------------------------------------------------
class VBox : public Layout {
public:
    int spacing;
    VBox(int s) : spacing(s) { w = 0; h = 0; }
    virtual void arrange() override {
        int cy = y;
        for (int i = 0; i < count; i++) {
            children[i]->x = x;
            children[i]->y = cy;
            cy += children[i]->h + spacing;
        }
    }
};

// ------------------------------------------------------------
// Window — canvas (via libgui) + pohon widget + event loop
// ------------------------------------------------------------
class Window {
public:
    gui_window_t* gw;
    Theme theme;
    Layout* root;
    bool running;
    int mouse_x, mouse_y;
    Widget* hovered;
    Widget* focused;   // fokus keyboard intra-window (TextBox)
    Widget* grabbed;   // widget yang memegang drag (left-down sampai release)

    Window(uint32_t width, uint32_t height)
        : gw(gui_create_window(width, height)), root(0),
          running(gw != 0), mouse_x(0), mouse_y(0), hovered(0),
          focused(0), grabbed(0) {}

    ~Window() {
        if (root) delete root;
        if (gw) gui_destroy(gw);
    }

    void set_theme(const ui_theme_t* t) {
        if (!t) return;
        theme.bg = t->bg; theme.fg = t->fg; theme.accent = t->accent;
        theme.button_bg = t->button_bg; theme.button_fg = t->button_fg;
        theme.button_hover = t->button_hover;
    }

    // Root selalu VBox bermargin 8px — widget pertama sekalipun layout.
    void add(Widget* w) {
        if (!root) { root = new VBox(8); root->x = 8; root->y = 8; }
        root->add(w);
    }

    // Kembalikan true bila hovered berubah (memicu redraw).
    bool track_hover() {
        Widget* n = root ? root->pick(mouse_x, mouse_y) : 0;
        if (n == hovered) return false;
        if (hovered) hovered->set_hover(false);
        hovered = n;
        if (hovered) hovered->set_hover(true);
        return true;
    }

    void set_focus(Widget* n) {
        if (n == focused) return;
        if (focused) focused->set_focus(false);
        focused = n;
        if (focused) focused->set_focus(true);
    }

    void render() {
        Painter p(gw, theme);
        p.rect(0, 0, (int)gw->width, (int)gw->height, theme.bg);
        if (root) root->draw(p);
        gui_flush(gw);
    }

    void run() {
        if (!gw) return;
        kyuzen_event_t ev;
        render();
        while (running) {
            if (sys_get_event(&ev)) {
                switch (ev.type) {
                case EVENT_MOUSE_MOVE:
                    mouse_x = ev.param1; mouse_y = ev.param2;
                    if (grabbed && grabbed->on_drag(mouse_x, mouse_y)) render();
                    if (track_hover()) render();
                    break;
                case EVENT_MOUSE_CLICK:
                    if (ev.param1 == 0 && ev.param2 == 1) {   // left down
                        grabbed = root ? root->pick(mouse_x, mouse_y) : 0;
                        set_focus(grabbed && grabbed->focusable() ? grabbed : 0);
                        if (grabbed) grabbed->on_click(mouse_x, mouse_y);
                        render();
                    } else if (ev.param1 == 0 && ev.param2 == 0) {   // left up
                        if (grabbed) { grabbed->on_release(); grabbed = 0; }
                        render();
                    }
                    break;
                case EVENT_KEY_PRESS:
                    if (ev.param1 == 27) running = false;   // ESC tutup
                    else if (focused) {
                        focused->on_key((uint8_t)ev.param1,
                                        (uint32_t)ev.param3,
                                        (uint32_t)ev.param2);
                        render();
                    }
                    break;
                case EVENT_WIN_CLOSE:
                    running = false;
                    break;
                default:
                    break;
                }
            }
            sys_yield();
        }
    }
};

} // namespace ui

// ============================================================
// Public C ABI — bridge ke toolkit C++. Handle opaque: void* di
// balik ui_window_t/ui_widget_t adalah pointer objek C++ (ui::*).
// ============================================================
extern "C" {

ui_window_t* ui_window_create(uint32_t width, uint32_t height) {
    return reinterpret_cast<ui_window_t*>(new ui::Window(width, height));
}

void ui_window_destroy(ui_window_t* win) {
    delete reinterpret_cast<ui::Window*>(win);
}

void ui_window_set_theme(ui_window_t* win, const ui_theme_t* theme) {
    reinterpret_cast<ui::Window*>(win)->set_theme(theme);
}

void ui_window_add(ui_window_t* win, ui_widget_t* widget) {
    reinterpret_cast<ui::Window*>(win)->add(reinterpret_cast<ui::Widget*>(widget));
}

void ui_window_run(ui_window_t* win) {
    reinterpret_cast<ui::Window*>(win)->run();
}

ui_widget_t* ui_label_create(ui_window_t* win, const char* text) {
    (void)win;
    return reinterpret_cast<ui_widget_t*>(new ui::Label(text));
}

void ui_label_set_text(ui_widget_t* widget, const char* text) {
    reinterpret_cast<ui::Label*>(widget)->set_text(text);
}

ui_widget_t* ui_button_create(ui_window_t* win, const char* text) {
    (void)win;
    return reinterpret_cast<ui_widget_t*>(new ui::Button(text));
}

void ui_button_set_click(ui_widget_t* widget, ui_click_cb cb, void* userdata) {
    reinterpret_cast<ui::Widget*>(widget)->set_click(cb, userdata);
}

ui_widget_t* ui_textbox_create(ui_window_t* win, int width) {
    (void)win;
    return reinterpret_cast<ui_widget_t*>(new ui::TextBox(width));
}

void ui_textbox_set_text(ui_widget_t* widget, const char* text) {
    reinterpret_cast<ui::TextBox*>(widget)->set_text(text);
}

const char* ui_textbox_text(ui_widget_t* widget) {
    return reinterpret_cast<ui::TextBox*>(widget)->text;
}

void ui_textbox_set_enter(ui_widget_t* widget, ui_click_cb cb, void* userdata) {
    ui::TextBox* tb = reinterpret_cast<ui::TextBox*>(widget);
    tb->enter_cb = cb; tb->enter_data = userdata;
}

ui_widget_t* ui_checkbox_create(ui_window_t* win, const char* label) {
    (void)win;
    return reinterpret_cast<ui_widget_t*>(new ui::CheckBox(label));
}

void ui_checkbox_set_checked(ui_widget_t* widget, int checked) {
    reinterpret_cast<ui::CheckBox*>(widget)->set_checked(checked != 0);
}

int ui_checkbox_checked(ui_widget_t* widget) {
    return reinterpret_cast<ui::CheckBox*>(widget)->checked ? 1 : 0;
}

void ui_checkbox_set_toggle(ui_widget_t* widget, ui_click_cb cb, void* userdata) {
    ui::CheckBox* cbx = reinterpret_cast<ui::CheckBox*>(widget);
    cbx->toggle_cb = cb; cbx->toggle_data = userdata;
}

ui_widget_t* ui_slider_create(ui_window_t* win, int min, int max) {
    (void)win;
    return reinterpret_cast<ui_widget_t*>(new ui::Slider(min, max));
}

void ui_slider_set_value(ui_widget_t* widget, int value) {
    reinterpret_cast<ui::Slider*>(widget)->set_value(value);
}

int ui_slider_value(ui_widget_t* widget) {
    return reinterpret_cast<ui::Slider*>(widget)->val;
}

void ui_slider_set_change(ui_widget_t* widget, ui_click_cb cb, void* userdata) {
    ui::Slider* s = reinterpret_cast<ui::Slider*>(widget);
    s->change_cb = cb; s->change_data = userdata;
}

ui_widget_t* ui_progressbar_create(ui_window_t* win, int width) {
    (void)win;
    return reinterpret_cast<ui_widget_t*>(new ui::ProgressBar(width));
}

void ui_progressbar_set_value(ui_widget_t* widget, int value) {
    reinterpret_cast<ui::ProgressBar*>(widget)->set_value(value);
}

ui_widget_t* ui_image_create(ui_window_t* win, const char* filename, int w, int h) {
    (void)win;
    return reinterpret_cast<ui_widget_t*>(new ui::Image(filename, w, h));
}

ui_widget_t* ui_vbox_create(ui_window_t* win, int spacing) {
    (void)win;
    return reinterpret_cast<ui_widget_t*>(new ui::VBox(spacing));
}

void ui_layout_add(ui_widget_t* layout, ui_widget_t* child) {
    reinterpret_cast<ui::Layout*>(layout)->add(reinterpret_cast<ui::Widget*>(child));
}

} // extern "C"
