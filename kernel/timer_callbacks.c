// ============================================================
// kernel/timer_callbacks.c — Default Timer Subscribers, Kyuzen OS
//
// STANDAR: Semua timing di sini pakai timer_get_ms() + timestamp target.
//   BUKAN: if (tick % 50 == 0) — fragile, tergantung TIMER_HZ
//   TAPI:  if (now >= next_event) — hardware-agnostic, benar di semua Hz
//
// Subscriber terdaftar:
//   Slot 0 — cb_visual  : spinner + uptime HUD
//   Slot 1 — cb_cursor  : cursor blink TTY
//   Slot 2 — cb_flush   : compositor screen flush
//   Slot 3-7 — reserved untuk driver/fitur baru
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
// CALLBACK 1: Visual HUD — spinner + uptime di pojok kanan atas
//
// Menggunakan timestamp ms, bukan modulo ticks.
// Ini benar di 50Hz, 100Hz, 144Hz — tidak perlu diubah saat ganti TIMER_HZ.
// ============================================================
static uint64_t next_spinner_update = 0;  // ms target: kapan spinner ganti frame
static uint64_t next_uptime_update  = 0;  // ms target: kapan uptime di-refresh


static void cb_visual(uint32_t tick) {
    (void)tick; // Tidak pakai raw tick — pakai ms timestamp
    if (fb_width == 0) return;

    uint64_t now = timer_get_ms();


    // --- Spinner: ganti frame setiap 200ms ---
    if (now >= next_spinner_update) {
        next_spinner_update = now + 200;

        static uint8_t spin_frame = 0;
        const char frames[] = {'|', '/', '-', '\\'};
        spin_frame = (spin_frame + 1) % 4;

        draw_rect(fb_width - 18, 3, 10, 16, 0x1E1E1E);
        draw_char(frames[spin_frame], fb_width - 18, 3, 0xFFFF00);
    }

    // --- Uptime: update setiap 1000ms (1 detik tepat) ---
    if (now >= next_uptime_update) {
        next_uptime_update = now + 1000;

        uint64_t total_sec = timer_get_seconds();

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

        // 16 char × 8px/char = 128px + 8px margin = 136px wide
        draw_rect(fb_width - 155, 3, 136, 16, 0x1E1E1E);
        draw_string(buf, fb_width - 155, 3, 0x00FF00);
    }
}

// ============================================================
// CALLBACK 2: Cursor blink — setiap 500ms
// ============================================================
static uint64_t next_cursor_blink = 0;


static void cb_cursor(uint32_t tick) {
    (void)tick;
    uint64_t now = timer_get_ms();

    if (now >= next_cursor_blink) {
        next_cursor_blink = now + 500; // Blink setiap 500ms
        tty_blink_cursor();
    }
}

// ============================================================
// CALLBACK 3: Screen flush — setiap tick (50fps max pada 50Hz)
// Untuk hemat CPU, ubah ke setiap 2 tick (25fps): tambah timestamp check.
// ============================================================
static void cb_flush(uint32_t tick) {
    (void)tick;
    compositor_flush();
}

// ============================================================
// ENTRY POINT — dipanggil dari kmain setelah init_timer()
// ============================================================
void timer_callbacks_init(void) {
    timer_register(cb_visual);   // Slot 0
    timer_register(cb_cursor);   // Slot 1
    timer_register(cb_flush);    // Slot 2
    // Slot 3-7: tersedia untuk network polling, audio tick, animasi, dll
}
