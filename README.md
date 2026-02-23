# AetherOS (x86_64, UEFI + Limine)

## Ringkas
- Boot via UEFI 64-bit dengan Limine 10.x.
- Kernel ELF 64-bit, freestanding.
- Framebuffer Limine digunakan untuk menggambar teks "OK".
- PMM bitmap + paging dasar sudah aktif.
- Semua akses memory fisik lewat HHDM (tanpa identity mapping).
- Lower half address space kosong untuk userland.
- Heap bump allocator (kmalloc) sudah aktif.
- VFS tree skeleton (root/child/lookup + resolve path absolut) sudah ada.
- /dev/null sudah ada (read EOF, write discard).
- /dev/zero sudah ada (read isi 0x00, write discard).
- FD table per-process (open/read/write/close) sudah ada.
- /dev/console sudah ada (write ke serial).
- Task struct lengkap + cooperative scheduler sudah ada.
- Syscall sleep berbasis wait queue + timeout (wakeup_tick + system_ticks) sudah ada.
- Wait queue minimal (TASK_WAITING + wake_one/wake_all) sudah ada.
- Virtual Memory Manager (VMM) per-address space sudah ada.
- IDT x86_64 dan interrupt 0x80 sudah aktif.
- SYSCALL/SYSRET userland sudah aktif (user mode via iretq).

## Struktur Penting
- kernel/kernel.c: entry `kernel_main`, init scheduler dan task.
- kernel/sched: task + scheduler + context switch.
- include/kernel/sched: API task + scheduler + wait queue.
- include/limine.h: header protocol Limine (versi resmi).
- linker.ld: layout ELF64 dan section `.limine_requests`.
- iso/boot/limine.conf: konfigurasi Limine.
- Makefile: build kernel + buat ESP image + run QEMU.
- kernel/arch/x86_64: GDT, IDT, paging, dan syscall entry.
- include/kernel/arch/paging.h: API HHDM (phys<->virt) dan paging init.
- kernel/memory/vmm.c: VMM (address space, map/unmap, switch CR3).
- include/kernel/memory/vmm.h: API VMM dan struct address_space.

## Alur Boot Singkat
- Limine menyediakan memmap, HHDM, framebuffer, dan base revision.
- kernel_main menginisialisasi memmap, PMM, paging + HHDM, VMM, heap.
- GDT/IDT/PIC/timer disiapkan lalu scheduler diaktifkan.
- VFS membangun tree /dev dan FD default 0/1/2.
- Test VMM, VFS, FD, /dev/zero, lalu status boot dicetak.
- Syscall diaktifkan, framebuffer menggambar "OK".
- Userland demo dibuat manual (1 page code + 1 page stack) dan masuk user mode.

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
- Serial menampilkan status boot dalam format clean:
[MM] T=0x... P=0x... B=0x... F=0x...
[PMM] A=0x...
[HEAP] H=0x...
[VFS] R=OK
[FD] r=0x... w=0x...
[DEV] Z=0x...
[VMM] = OK 
================================
[BOOT COMPLETE]
[SCHED] S
[TASK]
A[SLEEP] A
B[SLEEP] B
[WAKE] A
[WAKE] B
A[SLEEP] A

## Roadmap
- [x] Bootloader
- [x] Physical Memory Manager
- [x] Paging dasar (identity + HHDM + kernel map)
- [x] Kernel Heap (bump allocator)
- [x] VFS tree skeleton (root + lookup + resolve)
- [x] /dev/null (read EOF, write discard)
- [x] FD table per-process
- [x] /dev/console + default fd 0/1/2
- [x] /dev/zero (read isi 0x00, write discard)
- [x] Task struct lengkap + scheduler cooperative
- [x] Virtual Memory Manager (map/unmap)
- [x] Interrupt + IDT
- [x] Timer
- [ ] Scheduler preemptive
- [x] Syscall interface (int 0x80 + SYSCALL)
- [ ] Device abstraction



 
