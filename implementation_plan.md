# Preemptive Multitasking Scheduler — Kyuzen OS

## Analisis Sistem Existing

Sebelum menulis kode, ada beberapa hal kritis yang ditemukan dari audit:

### Stack Layout Saat Interrupt (IRQ0)

CPU x86_64 otomatis push 5 register saat interrupt:
```
[RSP+0]  → SS
[RSP+8]  → RSP (user-space RSP)
[RSP+16] → RFLAGS
[RSP+24] → CS
[RSP+32] → RIP
```

Lalu `timer_isr_stub` push 2 hal lagi:
```asm
push 0    ; error_code   [RSP-8]
push 32   ; int_num      [RSP-16]
```

Lalu `PUSHA64` push 15 register (dari MACRO yang ada):
```asm
push rax, rbx, rcx, rdx, rbp, rsi, rdi, r8..r15  (15 × 8 = 120 bytes)
```

**Total stack frame = 15 + 2 + 5 = 22 × 8 = 176 bytes**

Namun **URUTAN PUSH di `PUSHA64` existing TIDAK COCOK dengan `registers_t` yang diusulkan!**

PUSHA64 existing: `rax, rbx, rcx, rdx, rbp, rsi, rdi, r8, r9, r10, r11, r12, r13, r14, r15`  
Plan user: `r15..r8, rdi, rsi, rbp, rdx, rcx, rbx, rax` (urutan terbalik)

> [!CAUTION]
> **PUSHA64 macro harus diubah** agar cocok dengan struct `registers_t`. Karena PUSHA64 dipakai oleh SEMUA ISR stubs (timer, keyboard, syscall), perubahan ini berdampak ke seluruh interrupt system.

### Existing `task_t` vs Yang Dibutuhkan

Current `task_t`:
```c
typedef struct {
    uint64_t rsp;
    uint8_t  active;
} task_t;
```

Untuk preemptive, kita butuh tambahan:
- `state` (READY, RUNNING, SLEEPING, DEAD)
- `id` untuk identifikasi
- Stack base/size pointer (untuk cleanup)

### Dua Jalur Yield — Harus Harmonis

| | Cooperative `yield()` | Preemptive timer |
|--|--|--|
| Dipicu oleh | `sys_yield` syscall | IRQ0 timer |
| Context save | `switch_task()` ASM — callee-saved regs only | Full `registers_t` via ISR frame |
| Stack frame | 6 regs (rbp,rbx,r12-r15) + ret addr | 22 slots (full ISR frame) |

> [!CAUTION]
> **MASALAH BESAR**: Format stack dari `yield()/switch_task()` (6 regs) BERBEDA dengan format dari timer preemptive (22 regs). Jika task yang di-create via `create_task()` kemudian di-preempt oleh timer, stack-nya akan corrupt.

**Solusi**: Pilih satu model yang konsisten:
1. **Full ISR frame** untuk semua task — buat initial stack mirip seolah task baru di-interrupt
2. **Pisahkan jalur** — task yang berjalan via cooperative tetap pakai switch_task, preemptive hanya bisa switch task yang sudah dalam ISR frame

Rekomendasi: **Opsi 1 (Full ISR frame)** — lebih bersih dan standar (seperti Linux).

---

## User Review Required

> [!IMPORTANT]
> **Pilihan desain stack:** Apakah task yang dibuat via `create_task()` masih dipertahankan, atau kita refactor total ke model preemptive di mana semua task punya initial ISR frame? Jika dipertahankan, cooperative dan preemptive TIDAK bisa berjalan bersamaan dengan aman.

> [!WARNING]
> **PUSHA64 macro akan diubah** — semua ISR (keyboard, mouse, syscall) ikut terdampak. `isr128.asm` line 17 `mov [rsp + 112], rax` akan berubah offset-nya sesuai urutan register baru. Ini perlu dihitung ulang.

> [!NOTE]
> **CR3/Page table per-task**: Plan menyebut `cr3` di TCB. Apakah kita implementasikan address space isolation (VMM) atau semua task share satu address space (lebih sederhana)?

---

## Proposed Changes

### Arsitektur Baru: ISR-Frame Based Preemptive

Semua task (baru maupun existing) direpresentasikan dengan **full ISR stack frame**. Context switch = ganti RSP ke frame ISR task berikutnya.

```
Task stack layout (bottom → top, RSP points to r15):
  [bottom]  ... task stack data ...
  r15  ← RSP saat di-preempt
  r14
  r13
  r12
  r11
  r10
  r9
  r8
  rdi
  rsi
  rbp
  rdx
  rcx
  rbx
  rax
  int_num   (0 untuk initial frame)
  error_code(0 untuk initial frame)
  rip       → entry point fungsi task
  cs        → 0x08 (kernel CS)
  rflags    → 0x202 (IF=1)
  rsp       → top of stack (untuk ring 3: user RSP)
  ss        → 0x10 (kernel SS)
  [top]
```

---

### [MODIFY] `arch/x86/isr_macro.inc`

Ubah PUSHA64 agar cocok dengan `registers_t` struct (r8-r15 di-push duluan):

```asm
%macro PUSHA64 0
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rbp
    push rdx
    push rcx
    push rbx
    push rax
%endmacro

%macro POPA64 0
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rbp
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
%endmacro
```

> [!WARNING]
> Setelah ini, `isr128.asm` line 17 `mov [rsp + 112], rax` offset-nya masih **112** karena jumlah register yang di-push (15 × 8 = 120 bytes) — RAX sekarang di posisi terakhir pop, jadi di stack paling atas = `[rsp + 0]`. **Offset harus diubah ke `[rsp + 0]`**.

---

### [MODIFY] `kernel/task.h` — Expand TCB

```c
#define TASK_READY   0
#define TASK_RUNNING 1
#define TASK_SLEEPING 2
#define TASK_DEAD    3

typedef struct {
    uint32_t id;
    uint64_t rsp;         // Stack pointer saat di-preempt (full ISR frame)
    uint64_t stack_base;  // Untuk cleanup (kfree)
    uint8_t  state;       // TASK_READY, TASK_RUNNING, dll
    char     name[16];    // Nama task untuk debugging
} task_t;
```

---

### [MODIFY] `arch/x86/timer_isr.asm` — Preemptive Core

```asm
timer_isr_stub:
    push 0
    push 32
    PUSHA64

    mov rdi, rsp          ; Arg 1: current registers_t*
    call timer_handler    ; Returns: registers_t* (RSP task berikutnya)

    mov rsp, rax          ; CONTEXT SWITCH: ganti RSP ke task berikutnya

    POPA64
    add rsp, 16
    iretq
```

---

### [MODIFY] `drivers/timer.c` — timer_handler returns registers_t*

```c
typedef struct registers registers_t;  // forward declare

registers_t* timer_handler(registers_t* r) {
    timer_ticks++;
    // ... CPU tracking, callbacks ...
    
    // Preemptive: coba schedule setiap 20ms
    static uint64_t next_schedule = 0;
    uint64_t now = timer_get_ms();
    if (now >= next_schedule) {
        next_schedule = now + 20;
        extern registers_t* schedule(registers_t*);
        return schedule(r);
    }
    return r;
}
```

---

### [MODIFY] `kernel/task.c` — schedule() + refactor create_task()

```c
registers_t* schedule(registers_t* current_regs) {
    if (task_count <= 1) return current_regs;
    
    // Simpan RSP task saat ini
    tasks[current_task].rsp   = (uint64_t)current_regs;
    tasks[current_task].state = TASK_READY;
    
    // Round robin — skip task DEAD/SLEEPING
    int next = current_task;
    for (int i = 0; i < MAX_TASKS; i++) {
        next = (next + 1) % MAX_TASKS;
        if (tasks[next].state == TASK_READY) break;
    }
    
    current_task = next;
    tasks[current_task].state = TASK_RUNNING;
    return (registers_t*)tasks[current_task].rsp;
}
```

---

### [MODIFY] `arch/x86/switch.asm` — Hapus atau jadikan legacy

`switch_task()` tidak dibutuhkan lagi setelah preemptive aktif. `yield()` cukup dengan `int $0x80` atau dibiarkan memanggil `schedule()` via timer.

---

### [MODIFY] `isr128.asm` — Fix RAX offset setelah PUSHA64 reorder

Setelah PUSHA64 baru (r15 di-push duluan, rax paling akhir):
- RAX ada di `[rsp + 0]` (bukan `[rsp + 112]` lagi)

```asm
mov [rsp + 0], rax    ; Timpa RAX lama dengan return value syscall
```

---

## Open Questions

> [!IMPORTANT]
> **Q1**: Apakah `yield()` cooperative (via `switch_task`) masih dibutuhkan setelah preemptive aktif? Jika ya, perlu dua format stack berbeda yang harus di-bridge. Jika tidak, kita bisa simplify drastis.

> [!IMPORTANT]
> **Q2**: Task user-space (Ring 3) atau kernel threads saja? Ring 3 memerlukan stack switching (TSS RSP0) dan privilege level handling yang lebih kompleks.

> [!NOTE]
> **Q3**: Apakah `taskmgr.c` sudah pakai `sys_uptime` / `sys_get_cpu_usage` yang sudah kita fix? Perlu dipastikan tidak ada race condition saat membaca `current_cpu_usage` dari task yang di-preempt.

---

## Verification Plan

### Build Check
```bash
make clean && make boot_image.iso
```

### Manual Test
1. Boot → shell berjalan normal
2. Buat 2 task dari shell (misal: counter task)
3. Pastikan keduanya berjalan bersamaan tanpa freeze
4. Matikan task → scheduler skip task DEAD
5. `sys_yield` manual masih berfungsi (tidak crash)
