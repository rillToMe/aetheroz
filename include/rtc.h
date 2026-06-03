#ifndef RTC_H
#define RTC_H

#include <stdint.h>

// Array time_buf akan diisi dengan: [Tahun, Bulan, Hari, Jam, Menit, Detik]
void read_rtc(uint32_t* time_buf);

#endif