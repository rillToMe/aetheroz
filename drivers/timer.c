#include "io.h"
#include <stdint.h>
#include "timer.h"
#include "task.h"

volatile uint32_t timer_ticks = 0;

// --- VARIABEL PELACAK CPU ---
uint32_t cpu_idle_ticks = 0;
uint32_t cpu_total_ticks = 0;
uint32_t current_cpu_usage = 0;

// yield_counter dideklarasi di syscall.c — di-increment setiap kali sys_yield dipanggil
extern volatile uint32_t yield_counter;
static uint32_t prev_yield_snapshot = 0;

// Fungsi ini akan dipanggil oleh Syscall 37
uint32_t get_cpu_usage(void) {
    return current_cpu_usage;
}
// ---------------------------------------

extern uint32_t fb_width;
extern void draw_rect(uint32_t start_x, uint32_t start_y, uint32_t width, uint32_t height, uint32_t color);
extern void draw_string(const char* str, uint32_t x, uint32_t y, uint32_t color);
extern void draw_char(char c, uint32_t x, uint32_t y, uint32_t color);
extern void tty_blink_cursor();
extern void compositor_flush();

void init_timer(uint32_t freq) {
    uint32_t divisor = 1193180 / freq;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void timer_handler() {
    timer_ticks++; 
    
    // === LOGIKA PENGHITUNG BEBAN CPU ===
    cpu_total_ticks++;
    
    // Heuristik: Jika ada panggilan yield() sejak tick terakhir,
    // berarti CPU sedang menganggur (app menunggu input/sleeping).
    uint32_t current_yields = yield_counter;
    if (current_yields != prev_yield_snapshot) {
        cpu_idle_ticks++;
    }
    prev_yield_snapshot = current_yields;

    // Refresh data setiap 100 ticks (1 Detik pada 100Hz, 2 Detik pada 50Hz)
    if (cpu_total_ticks >= 100) {
        current_cpu_usage = 100 - ((cpu_idle_ticks * 100) / cpu_total_ticks);
        cpu_total_ticks = 0;
        cpu_idle_ticks = 0;
    }
    // ===================================
    
    // TIMER_HZ = 50 → 1 detik = 50 ticks
    #define TIMER_HZ 50

    if (fb_width > 0) {
        // 1. VISUAL SPINNER (Pojok Kanan Atas) — ganti setiap 0.2 detik
        const char spinner[] = {'|', '/', '-', '\\'};
        char current_spin = spinner[(timer_ticks / 10) % 4];
        draw_rect(fb_width - 18, 3, 10, 16, 0x1E1E1E);
        draw_char(current_spin, fb_width - 18, 3, 0xFFFF00);

        // 2. UPTIME TIMER — update setiap 1 detik = 50 ticks pada 50Hz
        if (timer_ticks % TIMER_HZ == 0) {
            uint32_t total_seconds = timer_ticks / TIMER_HZ; // 50 ticks = 1 detik
            uint32_t seconds = total_seconds % 60;
            uint32_t minutes = (total_seconds / 60) % 60;
            uint32_t hours   = (total_seconds / 3600);

            char time_str[] = "UPTIME: 00:00:00";
            time_str[8]  = (hours   / 10) + '0';
            time_str[9]  = (hours   % 10) + '0';
            time_str[11] = (minutes / 10) + '0';
            time_str[12] = (minutes % 10) + '0';
            time_str[14] = (seconds / 10) + '0';
            time_str[15] = (seconds % 10) + '0';

            // 16 karakter x 8px = 128px + margin = 136px, jauh dari spinner
            draw_rect(fb_width - 155, 3, 136, 16, 0x1E1E1E);
            draw_string(time_str, fb_width - 155, 3, 0x00FF00);
        }
    }

    // Blink cursor setiap 0.5 detik (25 ticks pada 50Hz)
    if (timer_ticks % 25 == 0) {
        tty_blink_cursor();
    }

    // Flush ke layar agar spinner & uptime tampil tanpa perlu input keyboard!
    // timer_handler menggambar ke base_canvas, tapi hanya compositor_flush()
    // yang memindahkan canvas ke framebuffer yang terlihat oleh user.
    compositor_flush();

    // KRITIS: Kirim EOI (End-Of-Interrupt) ke Master PIC!
    // Tanpa ini, PIC tidak akan pernah mengirim IRQ0 berikutnya.
    // (Sama seperti yang dilakukan keyboard_handler dan mouse_handler)
    outb(0x20, 0x20);
}