#include "io.h"
#include <stdint.h>
#include "timer.h"
#include "task.h"

volatile uint32_t timer_ticks = 0;

// Impor variabel dan mesin gambar dari kernel.c
extern uint32_t fb_width;
extern void draw_rect(uint32_t start_x, uint32_t start_y, uint32_t width, uint32_t height, uint32_t color);
extern void draw_string(const char* str, uint32_t x, uint32_t y, uint32_t color);
extern void draw_char(char c, uint32_t x, uint32_t y, uint32_t color);

extern void tty_blink_cursor();

void init_timer(uint32_t freq) {
    uint32_t divisor = 1193180 / freq;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void timer_handler() {
    timer_ticks++; 
    
    if (fb_width > 0) {
        
        // 1. VISUAL SPINNER (Pojok Kanan Atas)
        if (timer_ticks % 10 == 0) {
            const char spinner[] = {'|', '/', '-', '\\'};
            char current_spin = spinner[(timer_ticks / 10) % 4];
            draw_rect(fb_width - 20, 5, 8, 16, 0x1E1E1E); 
            draw_char(current_spin, fb_width - 20, 5, 0xFFFF00);
        }

        // 2. UPTIME TIMER (Setiap 100 ticks = 1 detik)
        if (timer_ticks % 100 == 0) {
            // ... [Logika penghitung waktu milikmu biarkan sama persis] ...
            uint32_t total_seconds = timer_ticks / 100;
            uint32_t seconds = total_seconds % 60;
            uint32_t minutes = (total_seconds / 60) % 60;
            uint32_t hours   = (total_seconds / 3600);

            char time_str[] = "UPTIME: 00:00:00";
            time_str[8]  = (hours / 10) + '0';
            time_str[9]  = (hours % 10) + '0';
            time_str[11] = (minutes / 10) + '0';
            time_str[12] = (minutes % 10) + '0';
            time_str[14] = (seconds / 10) + '0';
            time_str[15] = (seconds % 10) + '0';

            draw_rect(fb_width - 150, 5, 16 * 8, 16, 0x1E1E1E); 
            draw_string(time_str, fb_width - 150, 5, 0x00FFFF); 
        }

        // 3. ANIMASI KURSOR KEDAP-KEDIP (Setiap 25 ticks)
        if (timer_ticks % 25 == 0) {
            tty_blink_cursor();
        }
    }

    outb(0x20, 0x20); // Lapor PIC
    yield();          // Rampas CPU untuk Multitasking
}

uint32_t get_uptime(void) {
    return timer_ticks / 100;
}