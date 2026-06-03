#include "userlib.h"

void print(char* text) { __asm__ volatile("mov $1, %%eax \n mov %0, %%ebx \n int $0x80" : : "r"(text) : "%eax", "%ebx"); }
void clear_screen() { __asm__ volatile("mov $2, %%eax \n int $0x80" : : : "%eax"); }
uint32_t read_keyboard(char* buffer, uint32_t size) {
    uint32_t bytes_read;
    __asm__ volatile("mov $4, %%eax \n mov %1, %%ebx \n mov %2, %%ecx \n int $0x80 \n mov %%eax, %0 \n"
        : "=r"(bytes_read) : "r"(buffer), "r"(size) : "%eax", "%ebx", "%ecx");
    return bytes_read;
}
void sys_yield() { __asm__ volatile("mov $5, %%eax \n int $0x80" : : : "%eax"); }

void fs_format() { __asm__ volatile("mov $6, %%eax \n int $0x80" : : : "%eax"); }
void fs_list() { __asm__ volatile("mov $7, %%eax \n int $0x80" : : : "%eax"); }
void fs_read(char* filename) { __asm__ volatile("mov $8, %%eax \n mov %0, %%ebx \n int $0x80" : : "r"(filename) : "%eax", "%ebx"); }
void fs_delete(char* filename) { __asm__ volatile("mov $9, %%eax \n mov %0, %%ebx \n int $0x80" : : "r"(filename) : "%eax", "%ebx"); }

void* sys_alloc(uint32_t size) {
    uint32_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(11), "b"(size)); return (void*)ret;
}
void sys_free(void* ptr) { __asm__ volatile("int $0x80" : : "a"(12), "b"(ptr)); }
void* sys_realloc(void* ptr, uint32_t old_size, uint32_t new_size) {
    uint32_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(13), "b"(ptr), "c"(old_size), "d"(new_size)); return (void*)ret;
}
int sys_file_exists(char* filename) {
    uint32_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(14), "b"(filename)); return ret;
}
uint32_t sys_file_size(char* filename) {
    uint32_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(15), "b"(filename)); return ret;
}
int sys_read_file_to_buffer(char* filename, char* buffer) {
    uint32_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(16), "b"(filename), "c"(buffer)); return ret;
}
int sys_create_file(char* filename, char* data, uint32_t size) {
    uint32_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(17), "b"(filename), "c"(data), "d"(size)); return ret;
}

uint32_t sys_uptime() { uint32_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(18)); return ret; }
uint32_t sys_total_ram() { uint32_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(19)); return ret; }
uint32_t sys_used_ram() { uint32_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(20)); return ret; }

void print_num(uint32_t num) {
    if (num == 0) { print("0"); return; }
    char buf[16]; int i = 14; buf[15] = '\0';
    while (num > 0 && i >= 0) { buf[i--] = (num % 10) + '0'; num /= 10; }
    print(&buf[i + 1]);
}