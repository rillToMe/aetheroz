#include "io.h"
#include <stdint.h>

volatile uint32_t timer_ticks = 0;

void timer_handler() {
    timer_ticks++; // Hitung detak jantung CPU
    
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
    
    // 1. VISUAL SPINNER (Tetap berputar di pojok kanan, kolom 79)
    if (timer_ticks % 4 == 0) { 
        const char spinner[] = {'|', '/', '-', '\\'};
        uint8_t color_spinner = 0x0E; // Kuning Terang
        vga[79] = (uint16_t)spinner[(timer_ticks / 4) % 4] | ((uint16_t)color_spinner << 8);
    }

    // 2. UPTIME TIMER (Update setiap 18 tick, karena 1 detik = ~18.2 Hz)
    if (timer_ticks % 18 == 0) {
        uint32_t total_seconds = timer_ticks / 18;
        uint32_t seconds = total_seconds % 60;
        uint32_t minutes = (total_seconds / 60) % 60;
        uint32_t hours   = (total_seconds / 3600);

        uint8_t color_text = 0x0B; // Cyan Terang

        // Cetak tulisan " UPTIME: " mulai dari kolom 60
        const char* label = " UPTIME: ";
        for (int i = 0; label[i] != '\0'; i++) {
            vga[60 + i] = (uint16_t)label[i] | ((uint16_t)color_text << 8);
        }

        // Trik konversi angka ke ASCII: bagi 10 untuk digit puluhan, modulo 10 untuk satuan
        // Lalu ditambah '0' (kode ASCII 48) agar jadi karakter teks yang bisa dibaca layar

        // Cetak Jam (Kolom 69 & 70)
        vga[69] = (uint16_t)((hours / 10) + '0') | ((uint16_t)color_text << 8);
        vga[70] = (uint16_t)((hours % 10) + '0') | ((uint16_t)color_text << 8);
        vga[71] = (uint16_t)':' | ((uint16_t)color_text << 8);

        // Cetak Menit (Kolom 72 & 73)
        vga[72] = (uint16_t)((minutes / 10) + '0') | ((uint16_t)color_text << 8);
        vga[73] = (uint16_t)((minutes % 10) + '0') | ((uint16_t)color_text << 8);
        vga[74] = (uint16_t)':' | ((uint16_t)color_text << 8);

        // Cetak Detik (Kolom 75 & 76)
        vga[75] = (uint16_t)((seconds / 10) + '0') | ((uint16_t)color_text << 8);
        vga[76] = (uint16_t)((seconds % 10) + '0') | ((uint16_t)color_text << 8);
    }

    // Beritahu Master PIC bahwa interupsi selesai
    outb(0x20, 0x20);
}