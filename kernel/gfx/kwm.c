#include <stdint.h>
#include <stddef.h>
#include "kwm.h"
#include "kwm_internal.h"
#include "gfx.h"
#include "heap.h"
#include "task.h"
#include "spinlock.h"

// --- KYUZEN WINDOW MANAGER (KWM) ---
// FIX_004: batas dimensi/ukuran canvas — w*h*4 dari app tidak boleh wrap
// 32-bit (alokasi kecil untuk "canvas raksasa") atau menguras heap.
#define KWM_MAX_DIMENSION    4096U
#define KWM_MAX_CANVAS_BYTES (16U * 1024U * 1024U)

kwm_window_t kwm_windows[MAX_WINDOWS];
uint32_t next_z_index = 1;
spinlock_t kwm_lock = SPINLOCK_INIT;

// State global untuk drag session yang sedang aktif
static int     dragged_win_id = -1;  // -1 = tidak ada drag
static int32_t drag_offset_x  = 0;   // Offset klik dalam window (mencegah window "loncat")
static int32_t drag_offset_y  = 0;

// Caller MUST hold kwm_lock. FIX_004: canvas di-NULL-kan setelah free dan
// drag session ke slot ini diputus — tidak ada pointer/state menggantung.
static void kwm_free_slot(int i) {
    if (kwm_windows[i].canvas) {
        kfree(kwm_windows[i].canvas);
        kwm_windows[i].canvas = NULL;
    }
    kwm_windows[i].active = 0;
    kwm_windows[i].owner_task = -1;
    if (dragged_win_id == i) dragged_win_id = -1;
}

int kwm_create_window(int x, int y, uint32_t width, uint32_t height) {
    // FIX_004: validasi dulu. Ukuran dihitung 64-bit agar tidak wrap.
    if (width == 0 || height == 0 ||
        width > KWM_MAX_DIMENSION || height > KWM_MAX_DIMENSION) {
        return -1;
    }
    uint64_t bytes = (uint64_t)width * (uint64_t)height * 4ULL;
    if (bytes > (uint64_t)KWM_MAX_CANVAS_BYTES) return -1;

    int owner = smp_current_task_id();

    uint64_t flags = spinlock_lock_irqsave(&kwm_lock);
    for(int i = 0; i < MAX_WINDOWS; i++) {
        if(!kwm_windows[i].active) {
            // FIX_004: alokasi DULU — slot ditandai active hanya setelah
            // semua field siap (tidak ada zombie window saat kmalloc gagal).
            uint32_t* canvas = (uint32_t*)kmalloc((size_t)bytes);
            if (!canvas) {
                spinlock_unlock_irqrestore(&kwm_lock, flags);
                return -1;
            }
            kwm_windows[i].x = x;
            kwm_windows[i].y = y;
            kwm_windows[i].width = width;
            kwm_windows[i].height = height;
            kwm_windows[i].canvas = canvas;
            kwm_windows[i].owner_task = owner;
            kwm_windows[i].z_index = next_z_index++;
            kwm_windows[i].active = 1;
            spinlock_unlock_irqrestore(&kwm_lock, flags);
            screen_mark_dirty(x, y, width, height);
            return i;
        }
    }
    spinlock_unlock_irqrestore(&kwm_lock, flags);
    return -1;
}

// Owner task dari sebuah window (-1 jika slot kosong/id invalid).
int kwm_window_owner(int win_id) {
    if (win_id < 0 || win_id >= MAX_WINDOWS) return -1;
    uint64_t flags = spinlock_lock_irqsave(&kwm_lock);
    int owner = kwm_windows[win_id].active ? kwm_windows[win_id].owner_task : -1;
    spinlock_unlock_irqrestore(&kwm_lock, flags);
    return owner;
}

void kwm_update_window(int win_id, uint32_t* app_buffer) {
    if(win_id < 0 || win_id >= MAX_WINDOWS) return;
    uint64_t flags = spinlock_lock_irqsave(&kwm_lock);
    if(!kwm_windows[win_id].active || !kwm_windows[win_id].canvas || !app_buffer) {
        spinlock_unlock_irqrestore(&kwm_lock, flags);
        return;
    }
    // FIX_004: hanya pemilik yang boleh menulis canvas-nya.
    if (kwm_windows[win_id].owner_task != smp_current_task_id()) {
        spinlock_unlock_irqrestore(&kwm_lock, flags);
        return;
    }

    uint32_t size = kwm_windows[win_id].width * kwm_windows[win_id].height;
    uint32_t* dest = kwm_windows[win_id].canvas;
    __asm__ volatile ("rep movsl" : "+D" (dest), "+S" (app_buffer), "+c" (size) : : "memory");
    int32_t mx = kwm_windows[win_id].x, my = kwm_windows[win_id].y;
    uint32_t mw = kwm_windows[win_id].width, mh = kwm_windows[win_id].height;
    spinlock_unlock_irqrestore(&kwm_lock, flags);
    screen_mark_dirty(mx, my, mw, mh);
}

void kwm_destroy_window(int win_id) {
    if(win_id < 0 || win_id >= MAX_WINDOWS) return;
    uint64_t flags = spinlock_lock_irqsave(&kwm_lock);
    if(!kwm_windows[win_id].active) {
        spinlock_unlock_irqrestore(&kwm_lock, flags);
        return;
    }
    int32_t mx = kwm_windows[win_id].x, my = kwm_windows[win_id].y;
    uint32_t mw = kwm_windows[win_id].width, mh = kwm_windows[win_id].height;
    kwm_free_slot(win_id);
    spinlock_unlock_irqrestore(&kwm_lock, flags);
    screen_mark_dirty(mx, my, mw, mh);
}

// FIX_004: destroy HANYA window milik task_id — dipanggil saat app exit/exec
// menggantikan kwm_destroy_all_windows() agar lifecycle satu task tidak
// menghancurkan window task lain.
void kwm_destroy_windows_of(int task_id) {
    uint64_t flags = spinlock_lock_irqsave(&kwm_lock);
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (kwm_windows[i].active && kwm_windows[i].owner_task == task_id) {
            kwm_free_slot(i);
        }
    }
    spinlock_unlock_irqrestore(&kwm_lock, flags);
    screen_mark_dirty(0, 0, fb_width, fb_height);
}

// Destroy ALL KWM windows — hanya untuk path kernel/test, BUKAN syscall.
// Tanpa ini, compositor akan membaca memori bebas saat render.
void kwm_destroy_all_windows(void) {
    uint64_t flags = spinlock_lock_irqsave(&kwm_lock);
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (kwm_windows[i].active) {
            kwm_free_slot(i);
        }
    }
    spinlock_unlock_irqrestore(&kwm_lock, flags);
    screen_mark_dirty(0, 0, fb_width, fb_height);
}

// Kembalikan posisi window terkini (setelah drag, dsb) ke app via pointer.
// App harus panggil ini setiap kali ingin konversi koordinat layar → koordinat lokal window.
void kwm_get_window_pos(int win_id, int32_t* out_x, int32_t* out_y) {
    uint64_t flags = spinlock_lock_irqsave(&kwm_lock);
    if (win_id < 0 || win_id >= MAX_WINDOWS || !kwm_windows[win_id].active) {
        if (out_x) *out_x = 0;
        if (out_y) *out_y = 0;
        spinlock_unlock_irqrestore(&kwm_lock, flags);
        return;
    }
    if (out_x) *out_x = kwm_windows[win_id].x;
    if (out_y) *out_y = kwm_windows[win_id].y;
    spinlock_unlock_irqrestore(&kwm_lock, flags);
}

// ============================================================
// KWM V2 — Drag & Drop + Z-Index Dinamis
// ============================================================

// Bawa window ke depan (Z-index tertinggi)
// Dipanggil saat user klik pada window manapun.
static void kwm_bring_to_front(int win_id) {
    if (win_id < 0 || win_id >= MAX_WINDOWS) return;
    // Caller MUST hold kwm_lock
    kwm_windows[win_id].z_index = next_z_index++;
}

// Intercept mouse event sebelum dikirim ke user-space.
//
// Dipanggil dari mouse_handler() SEBELUM push_event().
// Return: 1 = event "dimakan" oleh KWM (jangan kirim ke app)
//         0 = teruskan event ke app seperti biasa
//
// Params:
//   mouse_px, mouse_py  = posisi kursor saat ini
//   left_down = 1 saat tombol kiri baru ditekan (edge detect)
//   left_up   = 1 saat tombol kiri baru dilepas (edge detect)
int kwm_process_mouse(int32_t mouse_px, int32_t mouse_py,
                      uint8_t left_down, uint8_t left_up) {

    spinlock_lock(&kwm_lock);

    // 1. Mouse Up — akhiri drag session
    if (left_up) {
        dragged_win_id = -1;
        spinlock_unlock(&kwm_lock);
        return 0; // Kirim event "release" ke app juga
    }

    // 2. Sedang dalam Drag — update posisi window mengikuti kursor
    if (dragged_win_id != -1) {
        int32_t new_x = mouse_px - drag_offset_x;
        int32_t new_y = mouse_py - drag_offset_y;

        // Clamp: pastikan window tidak keluar layar
        if (new_x < 0) new_x = 0;
        if (new_y < 0) new_y = 0;
        if (new_x + (int32_t)kwm_windows[dragged_win_id].width  > (int32_t)fb_width)
            new_x = (int32_t)fb_width  - (int32_t)kwm_windows[dragged_win_id].width;
        if (new_y + (int32_t)kwm_windows[dragged_win_id].height > (int32_t)fb_height)
            new_y = (int32_t)fb_height - (int32_t)kwm_windows[dragged_win_id].height;

        int32_t old_x = kwm_windows[dragged_win_id].x;
        int32_t old_y = kwm_windows[dragged_win_id].y;
        uint32_t dw = kwm_windows[dragged_win_id].width;
        uint32_t dh = kwm_windows[dragged_win_id].height;
        kwm_windows[dragged_win_id].x = new_x;
        kwm_windows[dragged_win_id].y = new_y;
        spinlock_unlock(&kwm_lock);
        screen_mark_dirty(old_x, old_y, dw, dh);
        screen_mark_dirty(new_x, new_y, dw, dh);
        return 1; // Konsumsi event — jangan sampai app salah deteksi klik
    }

    // 3. Mouse Down — hit-test, Z-bring-to-front, cek title bar drag
    if (left_down) {
        int highest_z  = -1;
        int target_win = -1;

        for (int i = 0; i < MAX_WINDOWS; i++) {
            if (!kwm_windows[i].active) continue;
            int32_t wx  = kwm_windows[i].x;
            int32_t wy  = kwm_windows[i].y;
            int32_t ww  = (int32_t)kwm_windows[i].width;
            int32_t wh  = (int32_t)kwm_windows[i].height;

            if (mouse_px >= wx && mouse_px < wx + ww &&
                mouse_py >= wy && mouse_py < wy + wh) {
                if ((int)kwm_windows[i].z_index > highest_z) {
                    highest_z  = (int)kwm_windows[i].z_index;
                    target_win = i;
                }
            }
        }

        if (target_win != -1) {
            kwm_bring_to_front(target_win);

            int32_t tx = kwm_windows[target_win].x, ty = kwm_windows[target_win].y;
            uint32_t tw = kwm_windows[target_win].width, th = kwm_windows[target_win].height;

            int32_t close_btn_x = kwm_windows[target_win].x
                                  + (int32_t)kwm_windows[target_win].width - 40;

            if (mouse_py >= kwm_windows[target_win].y &&
                mouse_py <  kwm_windows[target_win].y + 24 &&
                mouse_px <  close_btn_x) {
                dragged_win_id = target_win;
                drag_offset_x  = mouse_px - kwm_windows[target_win].x;
                drag_offset_y  = mouse_py - kwm_windows[target_win].y;
                spinlock_unlock(&kwm_lock);
                screen_mark_dirty(tx, ty, tw, th);
                return 1;
            }
            spinlock_unlock(&kwm_lock);
            screen_mark_dirty(tx, ty, tw, th);
            return 0;
        }
    }

    spinlock_unlock(&kwm_lock);
    return 0; // Klik di area kosong — teruskan
}
