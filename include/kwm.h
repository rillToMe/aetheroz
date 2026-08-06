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

// Intercept mouse event sebelum dikirim ke user-space.
// Return: 1 = event dimakan KWM, 0 = teruskan ke app.
int  kwm_process_mouse(int32_t mouse_px, int32_t mouse_py,
                       uint8_t left_down, uint8_t left_up);

// Phase 5B — routing input per-task (dipanggil dari IRQ keyboard/mouse).
// out_win_id menerima nilai untuk field win_id event (slot KWM + 1;
// 0 = tidak relevan). Return: owner task id, atau -1 jika tidak ada target.
int  kwm_route_keyboard(int* out_win_id);                 // owner window fokus
// Phase 5C: out_lx/out_ly = koordinat window-local konten (jika NULL, tidak
// ditulis). Hit-test & translasi dilakukan di sini.
int  kwm_route_mouse(int32_t x, int32_t y, int* out_win_id,
                     int32_t* out_lx, int32_t* out_ly);   // owner window di bawah kursor

// Phase 5D: shortcut WM (Alt-Tab) di-intercept dari IRQ keyboard SEBELUM
// routing. Return 1 = dikonsumsi KWM (jangan di-route/ke TTY), 0 = lanjut.
int  kwm_handle_shortcut(uint8_t mods, uint8_t released, uint16_t key_id);

#endif
