# AetherOS (x86_64, UEFI + Limine)

## Ringkas
- Boot via UEFI 64-bit dengan Limine 10.x.
- Kernel ELF 64-bit, freestanding.
- Framebuffer Limine digunakan untuk menggambar teks "OK".
- PMM bitmap + paging dasar sudah aktif.
- Heap bump allocator (kmalloc) sudah aktif.
- VFS tree skeleton (root/child/lookup + resolve path absolut) sudah ada.
- /dev/null sudah ada (read EOF, write discard).

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
- Serial menampilkan status memmap dan PMM (T/P/B/F/U/A).
- T= total usable bytes (aligned).
- P= total pages.
- B= ukuran bitmap.
- F= free pages.
- U= used pages.
- A= hasil pmm_alloc_page().
- Serial menampilkan H=0x... untuk test heap, R untuk test VFS resolve/read.
- Serial menampilkan N=0x0 dan W=0x10 untuk test /dev/null.

## Roadmap
- [x] Bootloader
- [x] Physical Memory Manager
- [x] Paging dasar (identity + HHDM + kernel map)
- [x] Kernel Heap (bump allocator)
- [x] VFS tree skeleton (root + lookup + resolve)
- [x] /dev/null (read EOF, write discard)
- [ ] Virtual Memory Manager (map/unmap)
- [ ] Interrupt + IDT
- [ ] Timer
- [ ] Scheduler
- [ ] Syscall interface
- [ ] Device abstraction


 
