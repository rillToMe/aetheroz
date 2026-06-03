#include <stdint.h>

void page_fault_handler() {
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
    
    // Warna: 0x1F = Teks Putih (F) di atas Background Biru (1)
    uint8_t bsod_color = 0x1F; 
    
    // 1. Timpa seluruh layar (80 kolom x 25 baris) dengan warna biru
    for (int i = 0; i < 80 * 25; i++) {
        vga[i] = (uint16_t)' ' | ((uint16_t)bsod_color << 8);
    }

    // 2. Cetak Judul Error
    const char* title = " *** KYUZEN OS FATAL ERROR: PAGE FAULT (0x0E) *** ";
    for (int i = 0; title[i] != '\0'; i++) {
        // Teks putih, Background Merah (4) khusus untuk judul
        vga[80 * 2 + 15 + i] = (uint16_t)title[i] | ((uint16_t)0x4F << 8); 
    }

    // 3. Cetak Pesan Bantuan
    const char* msg1 = "Kernel mencoba mengakses alamat memori yang tidak valid atau terlarang.";
    for (int i = 0; msg1[i] != '\0'; i++) {
        vga[80 * 5 + 5 + i] = (uint16_t)msg1[i] | ((uint16_t)bsod_color << 8);
    }

    const char* msg2 = "Sistem telah dihentikan demi mencegah kerusakan data lebih lanjut.";
    for (int i = 0; msg2[i] != '\0'; i++) {
        vga[80 * 7 + 5 + i] = (uint16_t)msg2[i] | ((uint16_t)bsod_color << 8);
    }

    const char* halt_msg = "SYSTEM HALTED. SILAKAN RESTART KOMPUTER ANDA.";
    for (int i = 0; halt_msg[i] != '\0'; i++) {
        // Teks kuning kelap-kelip (E) di atas biru
        vga[80 * 12 + 17 + i] = (uint16_t)halt_msg[i] | ((uint16_t)0x1E << 8);
    }

    // 4. MATIKAN CPU SELAMANYA!
    __asm__ volatile("cli"); // Matikan penerima interupsi
    while (1) {
        __asm__ volatile("hlt"); // Suruh CPU tidur tanpa henti
    }
}