#ifndef KWM_INTERNAL_H
#define KWM_INTERNAL_H

// State internal KWM yang dibagi antara kwm.c dan compositor.c.
// JANGAN di-include dari luar kernel/gfx/ — akses publik lewat include/kwm.h.

#include <stdint.h>
#include "spinlock.h"
#include "display.h"

#define MAX_WINDOWS 16

// --- Dekorasi milik WM (Phase 5C) ---
// Canvas window = KONTEN MURNI. Frame = konten + titlebar di atasnya, semua
// dihitung WM. Nilai memformalkan hardcode lama (24px drag / 40px close) yang
// duplikat antara kwm_process_mouse dan libgui.
#define KWM_TITLEBAR_H      24
#define KWM_CLOSE_BTN_W     40
#define KWM_TITLEBAR_COLOR  0x1F4E8C   // fokus (tint biru)
#define KWM_TITLEBAR_INACT  0x3A3A3A   // tidak fokus
#define KWM_CLOSE_COLOR     0xE53935

typedef struct {
    uint8_t active;
    // Posisi FRAME (termasuk titlebar): (x, y) = sudut kiri-atas titlebar.
    int32_t x, y;
    // Ukuran KONTEN (== ukuran canvas). Frame = width x (height + KWM_TITLEBAR_H).
    uint32_t width, height;
    // Phase 3A/5 adopsi: surface window = DisplayBuffer (owned, dibuat
    // display_buffer_create — stride == width).
    DisplayBuffer* canvas;
    uint32_t z_index;
    int32_t owner_task;   // FIX_004: task pemilik window (-1 = tidak ada)
    uint32_t flags;       // Phase 10: KWM_WIN_DESKTOP dll.
    char     title[32];   // Phase 10: judul titlebar + taskbar ("" = kosong)
} kwm_window_t;

// Phase 10: window desktop — full-screen, frameless (tanpa titlebar/close),
// z=0 (selalu di belakang), klik tidak refokus, tidak ikut Alt-Tab.
#define KWM_WIN_DESKTOP 0x1

extern kwm_window_t kwm_windows[MAX_WINDOWS];
extern uint32_t next_z_index;
extern spinlock_t kwm_lock;
// Phase 5B/5C: window pemegang fokus keyboard + tint titlebar. Dibaca
// compositor.c (di bawah kwm_lock) untuk warna titlebar.
extern int focused_win_id;

#endif
