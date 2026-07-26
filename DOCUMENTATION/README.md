# Kyuzen OS — Documentation

Dokumentasi Kyuzen OS diorganisir per topik (meniru gaya `Documentation/` di
Linux), menggantikan satu file raksasa secara bertahap.

> Dokumen lama: [`../DOCUMENTATION.md`](../DOCUMENTATION.md) masih memuat
> dokumentasi Task & Multitasking — akan dimigrasikan ke folder ini.

## Daftar Isi

| Folder | Isi |
|--------|-----|
| [`design/`](design/) | Dokumen desain/arsitektur: keputusan, invarian, constraint ABI, dan verifikasinya |
| [`troubleshooting/`](troubleshooting/) | Post-mortem insiden/bug: gejala, investigasi, root cause, fix, dan teknik debugging yang bisa dipakai ulang |

### design/

| Dokumen | Ringkasan |
|---------|-----------|
| [`ring3-tahap1-cpl3.md`](design/ring3-tahap1-cpl3.md) | Ring 3 Tahap 1 — ELF apps jalan di CPL 3 via TSS per-CPU + syscall stack permanen; syscall tetap `int 0x80` |
| [`sched-module-split.md`](design/sched-module-split.md) | Pemecahan `kernel/task.c` (907 baris) jadi modul `kernel/sched/` (core, runqueue, lifecycle, block, debug) — tanpa perubahan perilaku, `task.h` tidak berubah |

### troubleshooting/

| Dokumen | Ringkasan |
|---------|-----------|
| [`2026-07-26-heap-corruption-bosd.md`](troubleshooting/2026-07-26-heap-corruption-bosd.md) | BOSD "heap_block_t magic mismatch" saat buka PNG — ternyata bukan corruptor, melainkan halaman heap terpetakan ke ROM BIOS karena free list PMM tercemar mapping Limine |

---

## Konvensi

- Satu topik = satu folder; satu insiden/topik = satu file `.md`.
- Nama file post-mortem: `YYYY-MM-DD-<judul-singkat>.md`.
- Tulis dalam Bahasa Indonesia; biarkan identifier, path, command, dan log
  apa adanya (tidak diterjemahkan).
- Sertakan bukti mentah (potongan log, alamat, disassembly) — bukan hanya
  kesimpulan — supaya pembaca bisa memverifikasi ulang.
