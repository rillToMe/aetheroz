// user_apps/desktop.c — Desktop Environment (Phase 10): shell DE.
//
// Wallpaper + launcher ikon + taskbar. Memakai libgui (renderer murni,
// bukan pohon widget) karena butuh kontrol penuh event loop: render HANYA
// saat berubah (full-screen canvas mahal — jangan per-frame). Taskbar
// poll daftar window (sys_kwm_get_windows) tiap ~10 iterasi; klik taskbar
// → sys_kwm_activate_window (bring-to-front + fokus).
//
// Build: desktop.o + userlib.o + libgui.o

#include "userlib.h"
#include "libgui.h"

#define KWM_WIN_DESKTOP 0x1   // mirror kernel kwm_internal.h (filter taskbar)

// --- geometri ---
#define TB_H     36       // tinggi taskbar (baris bawah)
#define CELL_W   92       // lebar sel ikon launcher
#define CELL_H   100      // tinggi sel (ikon 56 + label 16 + gap)
#define ICON_SZ  56
#define ICON_X0  24
#define ICON_Y0  24

// --- warna ---
#define WALL_BG     0x141A2E
#define WALL_TXT    0x3A4160
#define TASK_BG     0x0B0E1C
#define TASK_EDGE   0x2A3355
#define TASK_BTN    0x1A2138
#define TASK_ACTIVE 0x2E4A8E
#define ICON_TXT    0xC0C8E0

typedef struct {
    const char* label;
    const char* elf;
    uint32_t    color;
} LauncherApp;

static const LauncherApp APPS[8] = {
    { "Explorer",     "fileman.elf",  0x1565C0 },
    { "Kalkulator",   "calc.elf",     0x2E7D32 },
    { "Image Viewer", "viewer.elf",   0x6A1B9A },
    { "Text Editor",  "notepad.elf",  0xEF6C00 },
    { "Terminal",     "terminal.elf", 0x37474F },
    { "Setelan",      "settings.elf", 0x00838F },
    { "Jam",          "clock.elf",    0xAD1457 },
    { "Task Manager", "taskmgr.elf",  0x5D4037 },
};

// --- window list (taskbar) ---
#define MAX_WINS 16
static kwm_window_info_t g_wins[MAX_WINS];
static int g_nwins = 0;

static int slen(const char* s) { int n = 0; while (s[n]) n++; return n; }

// Poll daftar window; return 1 bila ada perubahan (tampilan taskbar berubah).
static int winlist_changed(void) {
    kwm_window_info_t tmp[MAX_WINS];
    int n = sys_kwm_get_windows(tmp, MAX_WINS);
    if (n < 0) return 0;
    int changed = (n != g_nwins);
    if (!changed) {
        for (int i = 0; i < n; i++) {
            if (g_wins[i].win_id != tmp[i].win_id ||
                g_wins[i].focused != tmp[i].focused ||
                g_wins[i].flags  != tmp[i].flags) { changed = 1; break; }
            for (int j = 0; j < 32; j++)
                if (g_wins[i].title[j] != tmp[i].title[j]) { changed = 1; break; }
            if (changed) break;
        }
    }
    g_nwins = n;
    for (int i = 0; i < n; i++) g_wins[i] = tmp[i];
    return changed;
}

static void draw_icon(gui_window_t* d, int i) {
    int cx = ICON_X0 + (i % 4) * CELL_W;
    int cy = ICON_Y0 + (i / 4) * CELL_H;
    gui_draw_rect(d, cx, cy, ICON_SZ, ICON_SZ, APPS[i].color);
    int len = slen(APPS[i].label);
    gui_draw_text(d, APPS[i].label, cx + (ICON_SZ - len * 8) / 2,
                  cy + ICON_SZ + 4, ICON_TXT);
}

static void render(gui_window_t* d) {
    int W = (int)d->width, H = (int)d->height;
    // wallpaper
    gui_draw_rect(d, 0, 0, W, H - TB_H, WALL_BG);
    gui_draw_text(d, "KyuzenOS", W - 8 * 8 - 16, 12, WALL_TXT);
    for (int i = 0; i < 8; i++) draw_icon(d, i);
    // taskbar
    gui_draw_rect(d, 0, H - TB_H, W, TB_H, TASK_BG);
    gui_draw_rect(d, 0, H - TB_H, W, 1, TASK_EDGE);
    int bx = 8;
    for (int i = 0; i < g_nwins; i++) {
        if (g_wins[i].flags & KWM_WIN_DESKTOP) continue;
        if (!g_wins[i].title[0]) continue;
        int bw = slen(g_wins[i].title) * 8 + 20;
        gui_draw_rect(d, bx, H - TB_H + 4, bw, TB_H - 8,
                      g_wins[i].focused ? TASK_ACTIVE : TASK_BTN);
        gui_draw_text(d, g_wins[i].title, bx + 10, H - TB_H + 9, 0xE0E0E0);
        bx += bw + 6;
    }
}

static void handle_click(gui_window_t* d, int mx, int my) {
    int H = (int)d->height;
    // taskbar → aktivasi window
    if (my >= H - TB_H) {
        int bx = 8;
        for (int i = 0; i < g_nwins; i++) {
            if (g_wins[i].flags & KWM_WIN_DESKTOP) continue;
            if (!g_wins[i].title[0]) continue;
            int bw = slen(g_wins[i].title) * 8 + 20;
            if (mx >= bx && mx < bx + bw) {
                sys_kwm_activate_window((int)g_wins[i].win_id - 1);
                return;
            }
            bx += bw + 6;
        }
        return;
    }
    // launcher ikon → spawn app
    for (int i = 0; i < 8; i++) {
        int cx = ICON_X0 + (i % 4) * CELL_W;
        int cy = ICON_Y0 + (i / 4) * CELL_H;
        if (mx >= cx && mx < cx + ICON_SZ && my >= cy && my < cy + ICON_SZ) {
            sys_spawn((char*)APPS[i].elf);
            return;
        }
    }
}

void main(void) {
    gui_window_t* d = gui_create_desktop();
    if (!d) { sys_exit(); }

    kyuzen_event_t ev;
    int frame = 0;
    int do_render = 1;   // render awal

    while (d->is_running) {
        if (sys_get_event(&ev)) {
            if (ev.type == EVENT_MOUSE_MOVE) {
                d->mouse_x = ev.param1;
                d->mouse_y = ev.param2;
            } else if (ev.type == EVENT_MOUSE_CLICK && ev.param1 == 0 && ev.param2 == 1) {
                handle_click(d, d->mouse_x, d->mouse_y);
            }
            do_render = 1;
        }
        // poll window list ~10Hz (bukan callback — tanpa infra notifikasi kernel)
        if (++frame % 10 == 0 && winlist_changed()) do_render = 1;

        if (do_render) {
            render(d);
            sys_kwm_update_window(d->win_id, d->canvas);
            do_render = 0;
        }
        sys_yield();
    }
    sys_exit();
}
