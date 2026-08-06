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
};

// ------------------------------------------------------------
// Widget — basis pohon. Koordinat window-local konten.
// ------------------------------------------------------------
class Widget {
public:
    int x, y, w, h;
    bool visible;
    ui_click_cb click_cb;
    void* userdata;

    Widget() : x(0), y(0), w(0), h(0), visible(true), click_cb(0), userdata(0) {}
    virtual ~Widget() {}
    virtual void draw(Painter& p) = 0;
    virtual void set_hover(bool on) { (void)on; }
    // Hit-test: widget paling dalam yang memuat (mx,my), atau 0.
    virtual Widget* pick(int mx, int my) {
        if (!visible) return 0;
        return (mx >= x && mx < x + w && my >= y && my < y + h) ? this : 0;
    }
    virtual void on_click(int mx, int my) {
        (void)mx; (void)my;
        if (click_cb) click_cb(userdata);
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

    Window(uint32_t width, uint32_t height)
        : gw(gui_create_window(width, height)), root(0),
          running(gw != 0), mouse_x(0), mouse_y(0), hovered(0) {}

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
                    if (track_hover()) render();
                    break;
                case EVENT_MOUSE_CLICK:
                    if (ev.param3 != 0) mouse_x = ev.param3;
                    if (ev.param1 == 0 && ev.param2 == 1) {   // left down
                        Widget* wd = root ? root->pick(mouse_x, mouse_y) : 0;
                        if (wd) wd->on_click(mouse_x, mouse_y);
                        render();
                    }
                    break;
                case EVENT_KEY_PRESS:
                    if (ev.param1 == 27) running = false;   // ESC tutup
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

ui_widget_t* ui_vbox_create(ui_window_t* win, int spacing) {
    (void)win;
    return reinterpret_cast<ui_widget_t*>(new ui::VBox(spacing));
}

void ui_layout_add(ui_widget_t* layout, ui_widget_t* child) {
    reinterpret_cast<ui::Layout*>(layout)->add(reinterpret_cast<ui::Widget*>(child));
}

} // extern "C"
