# CHANGELOG

## Unreleased
- Boot via UEFI Limine 10.x, kernel ELF64 freestanding.
- Memmap parsing + PMM bitmap allocator aktif.
- Paging dasar aktif (kernel map + HHDM), tanpa identity mapping untuk akses kernel.
- API HHDM phys<->virt tersedia.
- Heap bump allocator (kmalloc) berjalan di HHDM.
- VMM per-address space (map/unmap, switch CR3, vmm_test).
- Lower half address space kosong untuk userland.
- GDT/IDT aktif, interrupt 0x80 dan SYSCALL/SYSRET userland.
- Scheduler cooperative + task struct lengkap.
- VFS tree skeleton + FD table per-process.
- /dev/null, /dev/zero, /dev/console aktif.
- Framebuffer Limine menggambar teks "OK".
- Output boot dirapikan menjadi status bar multiline yang konsisten.
- Debug pointer internal dan tick spam dinonaktifkan.
- Timer PIT sudah aktif untuk tick dasar.
- Sleep berbasis wait queue + timeout (wakeup_tick + system_ticks) dan timer_wait_queue.
- Wait queue minimal dengan TASK_WAITING + wake_one/wake_all.

### Skeleton / Minimal (perlu upgrade)
- VFS hanya tree + lookup/resolve + open/read/write; belum ada mount, path relative, dan permission.
- tmpfs belum diimplementasi (file kosong).
- FD table hanya open/read/write/close; belum ada seek/dup/flags.
- Scheduler masih cooperative; belum preemptive.
- Syscall hanya write/exit/return/sleep; belum ada API dasar lain.
- IDT hanya set interrupt 0x80; exception/IRQ lain belum didaftarkan.
- Userland loader masih hardcoded 1 page code + 1 page stack.
- Framebuffer font hanya karakter terbatas (A/B/O/K/space).
- VGA text driver ada tapi belum terintegrasi ke kernel_main.
