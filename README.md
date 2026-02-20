# AetherOS (x86_64, UEFI + Limine)

## Ringkas
- Boot via UEFI 64-bit dengan Limine 10.x.
- Kernel ELF 64-bit, freestanding.
- Framebuffer Limine digunakan untuk menggambar teks "OK".

## Struktur Penting
- kernel/kernel.c: entry `kernel_main`, request framebuffer Limine, draw text.
- include/limine.h: header protocol Limine (versi resmi).
- linker.ld: layout ELF64 dan section `.limine_requests`.
- iso/boot/limine.conf: konfigurasi Limine.
- Makefile: build kernel + buat ESP image + run QEMU.

## Build & Run (UEFI 64-bit)
1. Build dan buat ESP:

make run

2. QEMU akan boot dengan firmware UEFI x86_64 (lihat `FIRMWARE` di Makefile).

## Layout ESP
- /EFI/BOOT/BOOTX64.EFI
- /EFI/BOOT/limine.conf
- /kernel.elf

## limine.conf

TIMEOUT: 0

/AetherOS
    protocol: limine
    kernel_path: boot():/kernel.elf

## Output
- Kernel menggambar teks "OK" merah di framebuffer pada koordinat (100, 100).
