#include "string.h"

/* Menyalin blok memori dari src ke dest (tidak aman jika overlap). */
void* memcpy(void* dest, const void* src, size_t n) {
    uint8_t* pdest = (uint8_t*)dest;
    const uint8_t* psrc = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }
    return dest;
}

/*
 * Menyalin blok memori dengan benar meski src dan dest overlap.
 * Jika dest > src dan region overlap, salin dari belakang ke depan
 * agar byte yang belum disalin tidak tertimpa duluan.
 */
void* memmove(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    if (d == s || n == 0) {
        return dest;
    }
    if (d < s || d >= s + n) {
        /* Tidak overlap, atau dest ada di sebelum src -- salin maju */
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else {
        /* Overlap dengan dest > src -- salin mundur untuk keamanan */
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}

/* Mengisi blok memori dengan nilai tertentu. */
void* memset(void* s, int c, size_t n) {
    uint8_t* p = (uint8_t*)s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
    return s;
}

/* Membandingkan dua blok memori byte per byte. */
int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t* p1 = (const uint8_t*)s1;
    const uint8_t* p2 = (const uint8_t*)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return (int)p1[i] - (int)p2[i];
        }
    }
    return 0;
}

/* Menghitung panjang string (tidak termasuk null terminator). */
size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

/* Membandingkan dua string. Mengembalikan 0 jika identik. */
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/*
 * Membandingkan hingga n karakter pertama dari dua string.
 * Mengembalikan 0 jika n karakter pertama identik.
 */
int strncmp(const char* s1, const char* s2, size_t n) {
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/*
 * Menyalin hingga n karakter dari src ke dest.
 * Jika src lebih pendek dari n, sisa dest diisi '\0' (null-padding).
 * PERINGATAN: tidak menjamin null-terminator jika strlen(src) >= n.
 */
char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}