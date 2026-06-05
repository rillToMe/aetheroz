// ============================================================
// KERNEL CPU INFO — kernel/cpu.c
// Mengimplementasikan get_cpu_string() via instruksi CPUID.
// Dipanggil oleh syscall_handler (syscall 17) dan shell.c (fetch).
// ============================================================
#include <stdint.h>

void get_cpu_string(char* buffer) {
    uint32_t eax, ebx, ecx, edx;

    // CPUID leaf 0: vendor string (12 karakter di EBX, EDX, ECX)
    __asm__ volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0)
    );

    // Tulis vendor string ke 12 byte pertama
    ((uint32_t*)buffer)[0] = ebx;
    ((uint32_t*)buffer)[1] = edx;
    ((uint32_t*)buffer)[2] = ecx;

    // CPUID leaf 0x80000002-0x80000004: brand string (48 karakter)
    // Cek dulu apakah extended CPUID tersedia
    uint32_t max_ext;
    __asm__ volatile ("cpuid" : "=a"(max_ext) : "a"(0x80000000) : "ebx", "ecx", "edx");

    if (max_ext >= 0x80000004) {
        // Brand string dimulai dari offset 0 di buffer (overwrite vendor)
        uint32_t* p = (uint32_t*)buffer;
        for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; leaf++) {
            __asm__ volatile (
                "cpuid"
                : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                : "a"(leaf)
            );
            *p++ = eax;
            *p++ = ebx;
            *p++ = ecx;
            *p++ = edx;
        }
    } else {
        // Fallback: null-terminate setelah vendor string
        buffer[12] = '\0';
    }

    // Pastikan null-terminated (brand string sudah 48 byte, pastikan aman)
    buffer[48] = '\0';
}
