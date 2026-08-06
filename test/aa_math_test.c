// Self-check include/aa_math.h — integer murni, jalan di host (bukan task QEMU).
//   clang test/aa_math_test.c -o /tmp/aa && /tmp/aa
// Gagal = math blend/AA compositor kernel & toolkit libui rusak.

#include <assert.h>
#include <stdio.h>
#include "../include/aa_math.h"

int main(void) {
    // aa_mix: ujung dan tengah.
    assert(aa_mix(0xFF0000, 0x0000FF, 255) == 0xFF0000);
    assert(aa_mix(0xFF0000, 0x0000FF, 0)   == 0x0000FF);
    uint32_t h = aa_mix(0xFFFFFF, 0x000000, 128);
    assert((h & 0xFF) == 128 && ((h >> 8) & 0xFF) == 128 && ((h >> 16) & 0xFF) == 128);
    assert((aa_mix(0xFFFFFF, 0x000000, 200) >> 24) == 0);  // byte alpha selalu 0

    // aa_shade: arah + clamp di ujung skala.
    assert(aa_shade(0x808080, 0)    == 0x808080);
    assert(aa_shade(0x000000, -50)  == 0x000000);
    assert(aa_shade(0xFFFFFF, 100)  == 0xFFFFFF);
    assert((aa_shade(0x808080, 50) & 0xFF) > 0x80);
    assert((aa_shade(0x808080, -50) & 0xFF) < 0x80);

    // aa_cov pada rect 20x20 radius 8 (atas & bawah bulat).
    assert(aa_cov(10, 10, 0, 0, 20, 20, 8, 8) == 255);  // interior
    assert(aa_cov(10,  2, 0, 0, 20, 20, 8, 8) == 255);  // band atas, kolom tengah
    assert(aa_cov( 2, 10, 0, 0, 20, 20, 8, 8) == 255);  // kolom tepi, baris tengah
    assert(aa_cov( 0,  0, 0, 0, 20, 20, 8, 8) == 0);    // sudut jauh di luar arc
    assert(aa_cov(19, 19, 0, 0, 20, 20, 8, 8) == 0);
    uint32_t arc = aa_cov(2, 2, 0, 0, 20, 20, 8, 8);    // di atas busur
    assert(arc > 0 && arc < 255);
    // rt=rb=0 → kotak, tidak ada sudut yang di-blend.
    assert(aa_cov(0, 0, 0, 0, 20, 20, 0, 0) == 255);

    printf("aa_math: OK\n");
    return 0;
}
