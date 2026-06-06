#include "userlib.h"

void print(char* text) { __asm__ volatile("int $0x80" : : "a"(1), "b"((uint64_t)text)); }
void clear_screen() { __asm__ volatile("int $0x80" : : "a"(2)); }
uint32_t read_keyboard(char* buffer, uint32_t size) {
    uint64_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(3), "b"((uint64_t)buffer), "c"((uint64_t)size)); return (uint32_t)ret;
}
void sys_yield() { __asm__ volatile("int $0x80" : : "a"(4)); }

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
uint32_t sys_uptime() {
    uint64_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(14)); return (uint32_t)ret;
}
uint32_t sys_total_ram() {
    uint64_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(15)); return (uint32_t)ret;
}
uint32_t sys_used_ram() {
    uint64_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(16)); return (uint32_t)ret;
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