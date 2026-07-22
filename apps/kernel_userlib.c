// ============================================================
// KERNEL USERLIB SHIM — apps/kernel_userlib.c
//
// Implementasi kernel-side dari fungsi-fungsi yang dipanggil
// oleh apps/shell.c, apps/zen.c, dan apps/login.c.
//
// Berjalan di Ring 0. Tidak pakai int $0x80.
// ============================================================

#include <stdint.h>
#include <stddef.h>
#include "fs.h"
#include "heap.h"  // size_t-aware kmalloc/krealloc

// --- Impor API Kernel ---
extern fs_node_t  tty_node;
extern void       tty_clear(void);
extern void       yield(void);
extern void       compositor_flush(void);  // Flush setiap print agar langsung tampil
extern uint32_t   write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
extern uint32_t   read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);

extern void       kfs_format(void);
extern void       kfs_list_files(void);
extern void       kfs_read_file(char* filename);
extern void       kfs_delete_file(char* filename);
extern int        kfs_exists(char* filename);
extern uint32_t   kfs_get_file_size(char* filename);
extern int        kfs_read_to_buffer(char* filename, char* out_buffer, uint32_t buffer_capacity);
extern int        kfs_create_file(char* filename, char* data, uint32_t size);

#include "timer.h"    // Unified timer API: timer_get_ms(), timer_sleep_ms(), timer_get_ticks()


extern uint64_t   pmm_get_total_ram(void);
extern uint64_t   pmm_get_used_ram(void);

extern int        current_uid;

// ============================================================

static uint32_t _kul_strlen(const char* str) {
    uint32_t len = 0;
    while (str[len]) len++;
    return len;
}

// print: tulis ke TTY dan langsung flush ke layar
void print(char* text) {
    if (!text || !tty_node.write) return;
    write_fs(&tty_node, 0, _kul_strlen(text), (uint8_t*)text);
    compositor_flush();  // Tampilkan seketika, tidak tunggu timer tick
}

// clear_screen: bersihkan TTY + flush
void clear_screen(void) {
    tty_clear();
    compositor_flush();
}

// read_keyboard: blocking read 1 karakter (yield CPU sambil nunggu)
uint32_t read_keyboard(char* buffer, uint32_t size) {
    uint32_t n = 0;
    while (n == 0) {
        n = read_fs(&tty_node, 0, size, (uint8_t*)buffer);
        if (n == 0) {
            // Tidurkan CPU sampai interrupt berikutnya (timer/keyboard).
            // AMAN karena ini bukan di dalam interrupt handler.
            __asm__ volatile("sti; hlt");
        }
    }
    return n;
}

// yield_counter didefinisikan di kernel/syscall.c, dipakai timer.c untuk CPU idle tracking
extern volatile uint32_t yield_counter;

void sys_yield(void) {
    yield_counter++; // Hint ke CPU idle tracker bahwa kita sedang menunggu
    // Tidurkan CPU sampai interrupt berikutnya (timer akan preempt otomatis)
    // AMAN: dipanggil dari kernel code, bukan dari dalam ISR.
    __asm__ volatile("sti; hlt");
}



// --- Filesystem ---
void     fs_format(void)                { kfs_format(); }
void     fs_list(void)                  { kfs_list_files(); compositor_flush(); }
void     fs_read(char* filename)        { kfs_read_file(filename); compositor_flush(); }
void     fs_delete(char* filename)      { kfs_delete_file(filename); }

int      sys_file_exists(char* fn)      { return kfs_exists(fn); }
uint32_t sys_file_size(char* fn)        { return kfs_get_file_size(fn); }
int      sys_read_file_to_buffer(char* fn, char* buf, uint32_t cap) { return kfs_read_to_buffer(fn, buf, cap); }
int      sys_create_file(char* fn, char* data, uint32_t size) { return kfs_create_file(fn, data, size); }

// --- Memori (size_t agar cocok dengan heap.h) ---
void*    sys_alloc(size_t size)                              { return kmalloc(size); }
void     sys_free(void* ptr)                                 { kfree(ptr); }
void*    sys_realloc(void* ptr, size_t old_sz, size_t new_sz){ return krealloc(ptr, old_sz, new_sz); }

// --- Info Sistem ---
uint64_t sys_uptime(void)    { return timer_get_ms(); }        // ms sejak boot (uint64_t, tidak overflow)
uint64_t sys_total_ram(void) { return pmm_get_total_ram(); }
uint64_t sys_used_ram(void)  { return pmm_get_used_ram(); }


void sys_get_time(uint32_t* time_array) {
    extern void rtc_read_time(uint32_t*);
    rtc_read_time(time_array);
}

// --- Identitas User ---
void     sys_set_uid(uint32_t uid) { current_uid = (int)uid; }
uint32_t sys_get_uid(void)         { return (uint32_t)current_uid; }

// --- ELF Loader ---
uint64_t sys_load_elf(char* filename) {
    extern uint32_t elf_load_file(char* filename);
    // Flush KEDUA buffer input sebelum app baru dimulai.
    // Shell adalah kernel task yang memanggil fungsi ini langsung (bukan via syscall),
    // jadi flush di syscall 25 tidak pernah dieksekusi untuk path ini.
    // Tanpa flush: keystroke login + perintah shell bocor ke app GUI baru.
    extern void flush_event_queue(void);
    extern void flush_kbd_buffer(void);
    flush_event_queue();
    flush_kbd_buffer();
    return (uint64_t)elf_load_file(filename);
}


// --- Cetak Angka ---
void print_num(uint32_t num) {
    if (num == 0) { print("0"); return; }
    char buf[16]; int i = 14; buf[15] = '\0';
    while (num > 0 && i >= 0) { buf[i--] = (char)((num % 10) + '0'); num /= 10; }
    print(&buf[i + 1]);
}

void sys_shutdown(void) {
    __asm__ volatile("int $0x80" : : "a"(38));
}

void sys_reboot(void) {
    __asm__ volatile("int $0x80" : : "a"(39));
}
// strcmp sudah ada di kernel/string.c
