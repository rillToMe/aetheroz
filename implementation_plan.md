# Fix Kernel Freeze: CPU Idle Tracking di Kyuzen OS

## Root Cause Analysis

### Bug #1: `sti` di dalam Interrupt Gate → NESTED INTERRUPT CORRUPTION

Ini adalah **penyebab utama freeze**.

IDT entry untuk `int 0x80` (ISR 128) di-setup sebagai **Interrupt Gate** (`flags = 0xEE`):
```c
// idt.c:87
idt_set_gate(128, (uint64_t)isr128_stub, 0x08, 0xEE);
```

Pada x86_64, **Interrupt Gate otomatis meng-clear IF** (Interrupt Flag) saat CPU masuk ke handler. Ini artinya semua hardware interrupt (termasuk timer IRQ0) **di-disable** selama `syscall_handler()` berjalan.

Ketika kode lu memanggil `sti; hlt`:

```c
__asm__ volatile("sti; hlt");  // Line 86, syscall.c
```

Yang terjadi:
1. `sti` → re-enable interrupt **di dalam** syscall handler
2. Timer IRQ0 (INT 32) menembak → CPU masuk `timer_isr_stub`
3. `timer_isr_stub` memanggil `timer_handler()` yang melakukan operasi grafis (`draw_rect`, `draw_char`, `draw_string`)
4. `timer_handler` selesai → `iretq` dari timer ISR
5. CPU kembali ke instruksi **setelah `hlt`** di `syscall_handler`
6. `syscall_handler` return → kembali ke `isr128_stub`
7. `isr128_stub` melakukan `POPA64 + iretq`

> [!CAUTION]
> **Masalah kritis**: Timer ISR `iretq` akan kembali ke konteks **di dalam** syscall handler frame. Tapi `timer_isr_stub` **TIDAK mengirim EOI (End-Of-Interrupt) ke PIC!**

Lihat [timer_isr.asm](file:///e:/Project/02_Software_Engineering/low-level/kyuzen-os/arch/x86/timer_isr.asm):
```asm
timer_isr_stub:
    push 0
    push 32         ; IRQ 0
    PUSHA64
    call timer_handler  ; ← Tidak ada EOI setelah ini!
    POPA64
    add rsp, 16
    iretq
```

**Tanpa EOI**, PIC tidak akan pernah mengirim IRQ berikutnya. Setelah `hlt` pertama dibangunkan, timer IRQ tidak akan pernah menembak lagi → **FREEZE PERMANEN**.

### Bug #2: Menghapus `yield()` = Scheduler Mati

`yield()` di [task.c](file:///e:/Project/02_Software_Engineering/low-level/kyuzen-os/kernel/task.c) melakukan cooperative context switch antar task. Dengan menghapusnya dari syscall 4, scheduler berhenti bekerja sepenuhnya — semua task selain yang aktif tidak akan pernah mendapat giliran CPU.

### Bug #3: `is_cpu_idle` Race Condition

Flow saat ini:
```
syscall_handler → set is_cpu_idle=1 → sti;hlt → timer fires → timer_handler reads is_cpu_idle=1 → iretq → set is_cpu_idle=0
```

Ini **kebetulan benar** secara urutan, tapi sangat fragile. Jika ada interrupt lain yang membangunkan `hlt` selain timer (misalnya keyboard/mouse), `is_cpu_idle` tetap 1 saat timer tick berikutnya.

## Proposed Changes

### Arsitektur Solusi

Prinsip: **JANGAN pernah `sti` di dalam interrupt/syscall handler**. Tracking idle harus dilakukan oleh mekanisme yang sudah ada — yaitu memeriksa apakah CPU memang sedang menjalankan idle task.

Pendekatan: Kembalikan `yield()` untuk scheduler, dan ukur idle di `timer_handler` dengan memeriksa apakah task saat ini adalah idle loop (atau single-task mode = kernel idle).

---

### Kernel Syscall

#### [MODIFY] [syscall.c](file:///e:/Project/02_Software_Engineering/low-level/kyuzen-os/kernel/syscall.c)

Kembalikan syscall 4 ke pemanggilan `yield()` yang asli. Hapus `sti;hlt` dan `is_cpu_idle`.

```diff
 else if (syscall_num == 4) { // sys_yield
-    is_cpu_idle = 1; 
-    __asm__ volatile("sti; hlt"); 
-    is_cpu_idle = 0; 
+    yield();  // Context switch ke task berikutnya (cooperative)
 }
```

Hapus juga variabel `volatile int is_cpu_idle = 0;` dari file ini.

---

### Timer Driver

#### [MODIFY] [timer.c](file:///e:/Project/02_Software_Engineering/low-level/kyuzen-os/drivers/timer.c)

Ganti mekanisme idle tracking. Daripada bergantung pada flag `is_cpu_idle`, ukur idle berdasarkan apakah scheduler sedang menjalankan task tunggal (single task = kernel idle). Karena OS lu belum punya preemptive scheduler dan hanya cooperative (`yield()`), kita bisa mendeteksi "idle" dengan cara: **jika `task_count <= 1`**, maka CPU pasti idle karena hanya ada kernel shell dan tidak ada background work.

Tapi pendekatan yang lebih akurat dan mudah: **Biarkan `yield()` yang set flag idle secara aman**.

Implementasi yang benar:

```c
// Dideklarasi di timer.c, di-set oleh task.c/yield()
volatile int is_cpu_idle = 0;
```

Di `task.c` → `yield()`:
```c
void yield() {
    if (task_count <= 1) {
        // Tidak ada task lain. CPU idle.
        // Set flag, enable interrupt, halt. Saat timer bangunkan, clear flag.
        extern volatile int is_cpu_idle;
        is_cpu_idle = 1;
        __asm__ volatile("sti; hlt");
        is_cpu_idle = 0;
        return;
    }
    int old_task = current_task;
    current_task = (current_task + 1) % task_count;
    switch_task(&tasks[old_task].rsp, tasks[current_task].rsp);
}
```

> [!IMPORTANT]
> **Kenapa `sti;hlt` aman di `yield()` tapi TIDAK di `syscall_handler()`?**
> 
> Karena `yield()` dipanggil secara **CALL** dari `syscall_handler()`, yang berarti return address ada di stack biasa. Saat timer IRQ menembak di `hlt`, timer ISR melakukan `iretq` yang mengembalikan eksekusi ke instruksi setelah `hlt` di `yield()`. Kemudian `yield()` return normal ke `syscall_handler()`, yang return ke `isr128_stub`, yang melakukan `iretq` final.
>
> **TAPI** — ini tetap bermasalah kalau **timer ISR tidak kirim EOI**!

---

### Timer ISR Assembly — **FIX KRITIS: Tambahkan EOI**

#### [MODIFY] [timer_isr.asm](file:///e:/Project/02_Software_Engineering/low-level/kyuzen-os/arch/x86/timer_isr.asm)

```diff
 timer_isr_stub:
     push 0
     push 32         ; IRQ 0
     PUSHA64
     call timer_handler
+    ; Kirim EOI ke Master PIC (port 0x20)
+    mov al, 0x20
+    out 0x20, al
     POPA64
     add rsp, 16
     iretq
```

> [!WARNING]
> Cek juga apakah keyboard dan mouse ISR sudah kirim EOI. Jika tidak, mereka juga akan freeze setelah interrupt pertama.

---

### Ringkasan Perubahan

| File | Perubahan |
|------|-----------|
| [syscall.c](file:///e:/Project/02_Software_Engineering/low-level/kyuzen-os/kernel/syscall.c) | Kembalikan `yield()`, hapus `sti;hlt` dan `is_cpu_idle` |
| [timer.c](file:///e:/Project/02_Software_Engineering/low-level/kyuzen-os/drivers/timer.c) | Pindahkan `is_cpu_idle` ke sini, hapus `extern` |
| [task.c](file:///e:/Project/02_Software_Engineering/low-level/kyuzen-os/kernel/task.c) | Tambah idle detection di `yield()` saat `task_count <= 1` |
| [timer_isr.asm](file:///e:/Project/02_Software_Engineering/low-level/kyuzen-os/arch/x86/timer_isr.asm) | **KRITIS**: Tambahkan EOI ke PIC sebelum `iretq` |

## Confirmed Findings — EOI Status

Saya sudah cek semua ISR. Hasilnya:

| ISR | EOI di ASM? | EOI di C handler? | Status |
|-----|-------------|-------------------|--------|
| Timer (`timer_isr.asm` / `timer_handler()`) | ❌ | ❌ | **🔴 MISSING — ROOT CAUSE** |
| Keyboard (`keyboard_isr.asm` / `keyboard_handler()`) | ❌ | ✅ `outb(0x20, 0x20)` di line 79 | ✅ OK |
| Mouse (`mouse_isr.asm` / `mouse_handler()`) | ❌ | ✅ `outb(0xA0, 0x20); outb(0x20, 0x20)` di line 122 | ✅ OK |

> [!CAUTION]
> **Timer handler TIDAK pernah mengirim EOI ke PIC.** Ini berarti setelah IRQ0 pertama, PIC tidak akan pernah deliver IRQ0 lagi. OS berjalan "normal" hanya karena `sti` di kernel_main memicu timer pertama, dan setelah itu timer mati — tapi keyboard/mouse tetap jalan karena mereka kirim EOI sendiri dari C code.
>
> **Sebelum lu menambahkan `sti;hlt` di syscall, timer_handler tetap dipanggil berulang kali** kemungkinan karena ada skenario lain (misalnya timer masih bisa fire karena edge-triggered atau PIC behavior tertentu di QEMU). Tapi begitu lu menaruh `sti;hlt` di interrupt context, masalahnya menjadi fatal.

## Open Questions

> [!IMPORTANT]  
> **Apakah lu ingin preemptive scheduling (timer-driven context switch)?**
> Saat ini scheduler lu cooperative-only. Dengan menambahkan `schedule()` call di `timer_handler()`, lu bisa punya preemptive multitasking dan CPU idle tracking yang lebih akurat. Tapi ini perubahan besar — bisa kita bahas nanti.

## Verification Plan

### Manual Verification
1. Build dan jalankan di QEMU
2. Verifikasi spinner di pojok kanan atas terus berputar (bukti timer tidak freeze)
3. Buka Task Manager app → verifikasi CPU usage menampilkan angka yang masuk akal
4. Klik tombol X untuk menutup app → verifikasi tidak freeze
5. Test keyboard dan mouse tetap responsif
