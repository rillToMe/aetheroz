// ============================================================
// kernel/timer_callbacks.c — Default Timer Subscribers
//
// Berisi 4 fungsi callback yang didaftarkan ke timer:
//   1. cb_visual   — spinner + uptime display
//   2. cb_cursor   — cursor blink
//   3. cb_flush    — compositor frame flush
//   4. cb_cpu_mark — CPU idle marker (tidak perlu logic, sudah di timer.c)
//
// Untuk menambah fitur baru yang bergantung pada waktu,
// cukup buat fungsi baru dan daftarkan di timer_callbacks_init().
//
// PENTING: Semua callback harus CEPAT. Jangan lakukan blocking I/O di sini.
// ============================================================

#include <stdint.h>
#include "timer.h"

// --- Dependencies dari subsistem lain ---
extern uint32_t fb_width;
extern void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
extern void draw_char(char c, uint32_t x, uint32_t y, uint32_t color);
extern void draw_string(const char* str, uint32_t x, uint32_t y, uint32_t color);
extern void tty_blink_cursor(void);
extern void compositor_flush(void);

// ============================================================
// CALLBACK 1: Visual HUD (spinner + uptime di pojok kanan atas)
// ============================================================
static void cb_visual(uint32_t tick) {
    if (fb_width == 0) return;

    // Spinner — ganti frame setiap 0.2 detik (TICKS(200))
    const char frames[] = {'|', '/', '-', '\\'};
    char spin = frames[(tick / TICKS(200)) % 4];
    draw_rect(fb_width - 18, 3, 10, 16, 0x1E1E1E);
    draw_char(spin, fb_width - 18, 3, 0xFFFF00);

    // Uptime string — update setiap 1 detik tepat
    if (tick % TIMER_HZ != 0) return;

    uint32_t total_sec = timer_get_seconds();
    uint32_t sec  = total_sec % 60;
    uint32_t min  = (total_sec / 60) % 60;
    uint32_t hour = (total_sec / 3600);

    char buf[] = "UPTIME: 00:00:00";
    buf[8]  = (hour / 10) + '0';
    buf[9]  = (hour % 10) + '0';
    buf[11] = (min  / 10) + '0';
    buf[12] = (min  % 10) + '0';
    buf[14] = (sec  / 10) + '0';
    buf[15] = (sec  % 10) + '0';

    // 16 karakter x 8px/char = 128px + 8px margin = 136px wide
    draw_rect(fb_width - 155, 3, 136, 16, 0x1E1E1E);
    draw_string(buf, fb_width - 155, 3, 0x00FF00);
}

// ============================================================
// CALLBACK 2: Cursor blink — setiap 0.5 detik
// ============================================================
static void cb_cursor(uint32_t tick) {
    if (tick % TICKS(500) == 0) {
        tty_blink_cursor();
    }
}

// ============================================================
// CALLBACK 3: Screen flush — setiap tick (50fps max)
// Catatan: compositor_flush() sudah cepat (memcpy + window blit).
// Jika perlu hemat CPU, ubah ke: if (tick % 2 == 0) untuk 25fps.
// ============================================================
static void cb_flush(uint32_t tick) {
    (void)tick; // tick tidak dipakai, flush setiap frame
    compositor_flush();
}

// ============================================================
// ENTRY POINT — dipanggil dari kmain setelah init_timer()
// ============================================================
void timer_callbacks_init(void) {
    timer_register(cb_visual);   // Slot 0: HUD display
    timer_register(cb_cursor);   // Slot 1: cursor blink
    timer_register(cb_flush);    // Slot 2: screen flush
    // Slot 3-7: tersedia untuk driver/fitur baru
}
