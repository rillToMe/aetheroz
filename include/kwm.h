#ifndef KWM_H
#define KWM_H

#include <stdint.h>

// --- KYUZEN WINDOW MANAGER (KWM) — kernel/gfx/kwm.c ---

int  kwm_create_window(int x, int y, uint32_t width, uint32_t height);
void kwm_update_window(int win_id, uint32_t* app_buffer);
void kwm_destroy_window(int win_id);

// Owner task dari sebuah window (-1 jika slot kosong/id invalid).
int  kwm_window_owner(int win_id);

// Ukuran canvas window dalam byte (0 jika slot kosong/id invalid).
uint64_t kwm_window_canvas_bytes(int win_id);

// Destroy HANYA window milik task_id — dipanggil saat app exit/exec.
void kwm_destroy_windows_of(int task_id);

// Destroy ALL KWM windows — hanya untuk path kernel/test, BUKAN syscall.
void kwm_destroy_all_windows(void);

// Posisi window terkini (setelah drag, dsb) via pointer.
void kwm_get_window_pos(int win_id, int32_t* out_x, int32_t* out_y);

// Intercept mouse event sebelum dikirim ke user-space.
// Return: 1 = event dimakan KWM, 0 = teruskan ke app.
int  kwm_process_mouse(int32_t mouse_px, int32_t mouse_py,
                       uint8_t left_down, uint8_t left_up);

// 1 jika ada window aktif — routing wheel scroll (mouse IRQ).
int  kwm_has_active_windows(void);

#endif
