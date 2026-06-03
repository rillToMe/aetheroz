#include "rtc.h"

// Fungsi untuk membaca dan menulis ke Port Hardware
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

// Membaca register CMOS
static uint8_t get_rtc_register(int reg) {
    outb(0x70, reg);
    return inb(0x71);
}

// Konversi BCD ke Angka Biasa
static uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd / 16) * 10);
}

void read_rtc(uint32_t* time_buf) {
    uint8_t sec   = get_rtc_register(0x00);
    uint8_t min   = get_rtc_register(0x02);
    uint8_t hour  = get_rtc_register(0x04);
    uint8_t day   = get_rtc_register(0x07);
    uint8_t month = get_rtc_register(0x08);
    uint8_t year  = get_rtc_register(0x09);

    time_buf[0] = bcd_to_bin(year) + 2000;
    time_buf[1] = bcd_to_bin(month);
    time_buf[2] = bcd_to_bin(day);
    
    // CMOS biasanya menyimpan waktu dalam UTC/GMT+0.
    // Kita sesuaikan dengan zona waktu Waktu Indonesia Barat (GMT+7)
    uint32_t wib_hour = bcd_to_bin(hour) + 7;
    if (wib_hour >= 24) { wib_hour %= 24; } 

    time_buf[3] = wib_hour;
    time_buf[4] = bcd_to_bin(min);
    time_buf[5] = bcd_to_bin(sec);
}