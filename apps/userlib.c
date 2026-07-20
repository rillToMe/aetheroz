#include "userlib.h"

void print(char* text) { __asm__ volatile("int $0x80" : : "a"(1), "b"((uint64_t)text)); }
void clear_screen() { __asm__ volatile("int $0x80" : : "a"(2)); }
uint32_t read_keyboard(char* buffer, uint32_t size) {
    uint64_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(3), "b"((uint64_t)buffer), "c"((uint64_t)size)); return (uint32_t)ret;
}
void sys_yield() { 
    __asm__ volatile("int $0x80" : : "a"(4)); 
    // Setelah iretq, IF=1 (interrupt aktif). Tidurkan CPU di LUAR interrupt handler.
    // Timer/keyboard/mouse akan membangunkan CPU dari hlt.
    __asm__ volatile("hlt"); 
}

void fs_format() { __asm__ volatile("int $0x80" : : "a"(5)); }
void fs_list() { __asm__ volatile("int $0x80" : : "a"(6)); }
void fs_read(char* filename) { __asm__ volatile("int $0x80" : : "a"(7), "b"((uint64_t)filename)); }
void fs_delete(char* filename) { __asm__ volatile("int $0x80" : : "a"(8), "b"((uint64_t)filename)); }

void* sys_alloc(uint32_t size) {
    uint64_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(9), "b"((uint64_t)size)); return (void*)ret;
}
void sys_free(void* ptr) { __asm__ volatile("int $0x80" : : "a"(10), "b"((uint64_t)ptr)); }
int sys_file_exists(char* filename) {
    uint64_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(11), "b"((uint64_t)filename)); return (int)ret;
}
uint32_t sys_file_size(char* filename) {
    uint64_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(12), "b"((uint64_t)filename)); return (uint32_t)ret;
}
int sys_read_file_to_buffer(char* filename, char* buffer) {
    uint64_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(13), "b"((uint64_t)filename), "c"((uint64_t)buffer)); return (int)ret;
}
uint64_t sys_uptime() {
    uint64_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(14)); return ret;
}
uint64_t sys_total_ram() {
    uint64_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(15)); return ret;
}
uint64_t sys_used_ram() {
    uint64_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(16)); return ret;
}
void get_cpu_string(char* buffer) {
    // Kita panggil Syscall 17, biarkan kernel Ring 0 yang membacakan CPUID!
    __asm__ volatile("int $0x80" : : "a"(17), "b"((uint64_t)buffer));
}
int sys_create_file(char* filename, char* data, uint32_t size) {
    uint64_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(18), "b"((uint64_t)filename), "c"((uint64_t)data), "d"((uint64_t)size)); return (int)ret;
}
void* sys_realloc(void* ptr, uint32_t old_size, uint32_t new_size) {
    uint64_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(19), "b"((uint64_t)ptr), "c"((uint64_t)old_size), "d"((uint64_t)new_size)); return (void*)ret;
}
void sys_get_time(uint32_t* time_array) {
    __asm__ volatile("int $0x80" : : "a"(20), "b"((uint64_t)time_array));
}
void sys_draw_pixel(int x, int y, uint32_t color) {
    __asm__ volatile("int $0x80" : : "a"(22), "b"((uint64_t)x), "c"((uint64_t)y), "d"((uint64_t)color));
}
void sys_draw_image(int x, int y, int width, int height, uint32_t* buffer) {
    __asm__ volatile("int $0x80" : : "a"(23), "b"((uint64_t)x), "c"((uint64_t)y), "d"((uint64_t)width), "S"((uint64_t)height), "D"((uint64_t)buffer));
}
int sys_get_file_list(file_info_t* buffer, int max_entries) {
    uint64_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(24), "b"((uint64_t)buffer), "c"((uint64_t)max_entries)); return (int)ret;
}
uint64_t sys_load_elf(char* filename) {
    uint64_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(25), "b"((uint64_t)filename)); return ret;
}
void sys_draw_string(const char* str, int x, int y, uint32_t color) {
    __asm__ volatile("int $0x80" : : "a"(26), "b"((uint64_t)str), "c"((uint64_t)x), "d"((uint64_t)y), "S"((uint64_t)color));
}
void sys_set_uid(uint32_t uid) {
    __asm__ volatile("int $0x80" : : "a"(27), "b"((uint64_t)uid));
}
uint32_t sys_get_uid() {
    uint64_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(28)); return (uint32_t)ret;
}
int sys_get_event(kyuzen_event_t* event_out) {
    uint64_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(29), "b"((uint64_t)event_out)); return (int)ret;
}
int sys_create_window(int x, int y, uint32_t width, uint32_t height) {
    uint64_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(30), "b"((uint64_t)x), "c"((uint64_t)y), "d"((uint64_t)width), "S"((uint64_t)height)); return (int)ret;
}
void sys_update_window(int win_id, uint32_t* buffer) {
    __asm__ volatile("int $0x80" : : "a"(31), "b"((uint64_t)win_id), "c"((uint64_t)buffer));
}
void sys_destroy_window(int win_id) {
    __asm__ volatile("int $0x80" : : "a"(32), "b"((uint64_t)win_id));
}
void sys_get_window_pos(int win_id, int* out_x, int* out_y) {
    // Syscall 40: rbx=win_id, rcx=out_x ptr, rdx=out_y ptr
    __asm__ volatile("int $0x80" : : "a"(40), "b"((uint64_t)win_id), "c"((uint64_t)out_x), "d"((uint64_t)out_y));
}

int sys_kwm_create_window(int x, int y, uint32_t width, uint32_t height) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(30), "b"(x), "c"(y), "d"(width), "S"(height));
    return ret;
}

void sys_kwm_update_window(int win_id, uint32_t* buffer) {
    __asm__ volatile("int $0x80" : : "a"(31), "b"(win_id), "c"(buffer));
}

void sys_kwm_destroy_window(int win_id) {
    __asm__ volatile("int $0x80" : : "a"(32), "b"(win_id));
}

// sys_exec: Load app baru, replace current app, TIDAK PERNAH kembali ke caller.
// OS yang free RAM lama, load app baru, lalu lompat langsung ke entry-nya.
__attribute__((noreturn))
void sys_exec(char* filename) {
    __asm__ volatile("int $0x80" : : "a"(33), "b"((uint64_t)filename));
    __builtin_unreachable();
}

// sys_exit: App selesai, kembali ke shell.
// Kernel membebaskan RAM app dan jump langsung ke shell command loop.
__attribute__((noreturn))
void sys_exit(void) {
    __asm__ volatile("int $0x80" : : "a"(34));
    __builtin_unreachable();
}

void print_num(uint32_t num) {
    if (num == 0) { print("0"); return; }
    char buf[16]; int i = 14; buf[15] = '\0';
    while (num > 0 && i >= 0) { buf[i--] = (num % 10) + '0'; num /= 10; }
    print(&buf[i + 1]);
}

__attribute__((weak)) void* memcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest; const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < count; i++) d[i] = s[i];
    return dest;
}

__attribute__((weak)) void* memset(void* dest, int val, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    for (size_t i = 0; i < count; i++) d[i] = (uint8_t)val;
    return dest;
}

void sys_shutdown(void) {
    __asm__ volatile("int $0x80" : : "a"(38));
}

void sys_reboot(void) {
    __asm__ volatile("int $0x80" : : "a"(39));
}

uint32_t sys_get_total_disk(void) {
    uint32_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(35)); return ret;
}

// UBAH NAMA FUNGSI INI JADI sys_get_used_disk
uint32_t sys_get_used_disk(void) { 
    uint32_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(36)); return ret;
}

uint32_t sys_get_cpu_usage(void) {
    uint32_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(37)); return ret;
}

// sys_ping: Syscall 41 — ICMP Echo Request ke host.
// Output (reply/timeout) dicetak oleh kernel langsung ke TTY.
// Return: rata-rata RTT dalam ms, atau -1 (sebagai int64) jika gagal.
int sys_ping(const char *host) {
    int64_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(41), "b"((uint64_t)host));
    return (int)ret;
}

// sys_get_cr3: Syscall 42 — Read CR3 (PML4 physical address)
// For process isolation testing: different apps should see different CR3 values.
uint64_t sys_get_cr3(void) {
    uint64_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(42));
    return ret;
}

// sys_get_task_id: Syscall 43 — Get current task ID
// Returns -1 if CPU is idle, or the task slot index.
int sys_get_task_id(void) {
    int64_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(43));
    return (int)ret;
}

// sys_is_mapped: Syscall 44 — Check if virtual address is mapped
// Returns 1 if the page is present in current PML4, 0 if not.
// Safe: does NOT dereference the address.
int sys_is_mapped(void* addr) {
    int64_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(44), "b"((uint64_t)addr));
    return (int)ret;
}

// sys_get_pid: Syscall 45 — Get per-address-space unique cookie
// Returns a unique ID assigned by the OS when the address space was created.
// Different apps running sequentially will get different values.
uint32_t sys_get_pid(void) {
    uint64_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(45));
    return (uint32_t)ret;
}

// sys_sleep: Syscall 46 — Non-busy sleep selama `ms` milidetik.
// Task masuk state TASK_SLEEPING; CPU bebas menjalankan task lain sampai
// timer membangunkan task ini. Bukan busy-wait.
void sys_sleep(uint32_t ms) {
    __asm__ volatile("int $0x80" : : "a"(46), "b"((uint64_t)ms));
}
