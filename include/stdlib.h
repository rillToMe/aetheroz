#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h> // Clang memiliki stddef.h bawaan (menyediakan size_t dan NULL)

// stb_image kadang memanggil fungsi nilai mutlak (meskipun kebanyakan di JPEG/BMP)
static inline int abs(int j) {
    return (j < 0) ? -j : j;
}

#endif