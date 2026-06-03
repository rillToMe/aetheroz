#include "string.h"

// Menyalin blok memori dari sumber (src) ke tujuan (dest)
void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* pdest = (uint8_t*)dest;
    const uint8_t* psrc = (const uint8_t*)src;
    
    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }
    
    return dest;
}

// Mengisi blok memori dengan nilai tertentu (sangat berguna nanti untuk mengosongkan RAM)
void* memset(void* s, int c, size_t n) {
    uint8_t* p = (uint8_t*)s;
    
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
    
    return s;
}