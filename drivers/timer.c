#include "io.h"
#include <stdint.h>
#include "timer.h" // Tambahan
#include "task.h"  // Tambahan untuk yield()

volatile uint32_t timer_ticks = 0;

// FITUR BARU: Mengubah kecepatan detak jantung CPU (PIT)
void init_timer(uint32_t freq) {
    uint32_t divisor = 1193180 / freq;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void timer_handler() {
    timer_ticks++; // Hitung detak jantung CPU
    
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
    
    // 1. VISUAL SPINNER (Tetap berputar di pojok kanan)
    if (timer_ticks % 10 == 0) { // Diperlambat sedikit animasinya karena sekarang detaknya 100Hz
        const char spinner[] = {'|', '/', '-', '\\'};
        uint8_t color_spinner = 0x0E; // Kuning Terang
        vga[79] = (uint16_t)spinner[(timer_ticks / 10) % 4] | ((uint16_t)color_spinner << 8);
    }

    // 2. UPTIME TIMER (DIUBAH KE 100 TICK = 1 DETIK)
    if (timer_ticks % 100 == 0) {
        uint32_t total_seconds = timer_ticks / 100;
        uint32_t seconds = total_seconds % 60;
        uint32_t minutes = (total_seconds / 60) % 60;
        uint32_t hours   = (total_seconds / 3600);

        uint8_t color_text = 0x0B; // Cyan Terang

        const char* label = " UPTIME: ";
        for (int i = 0; label[i] != '\0'; i++) {
            vga[60 + i] = (uint16_t)label[i] | ((uint16_t)color_text << 8);
        }

        vga[69] = (uint16_t)((hours / 10) + '0') | ((uint16_t)color_text << 8);
        vga[70] = (uint16_t)((hours % 10) + '0') | ((uint16_t)color_text << 8);
        vga[71] = (uint16_t)':' | ((uint16_t)color_text << 8);

        vga[72] = (uint16_t)((minutes / 10) + '0') | ((uint16_t)color_text << 8);
        vga[73] = (uint16_t)((minutes % 10) + '0') | ((uint16_t)color_text << 8);
        vga[74] = (uint16_t)':' | ((uint16_t)color_text << 8);

        vga[75] = (uint16_t)((seconds / 10) + '0') | ((uint16_t)color_text << 8);
        vga[76] = (uint16_t)((seconds % 10) + '0') | ((uint16_t)color_text << 8);
    }

    // 3. WAJIB lapor ke PIC bahwa interupsi selesai
    outb(0x20, 0x20);

    // 4. PREEMPTIVE MULTITASKING: Rampas CPU!
    yield();
}