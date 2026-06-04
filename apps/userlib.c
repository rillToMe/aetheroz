#include "userlib.h"
#include "multiboot.h" // Tambahkan ini di deretan include atas

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

// Sekarang Syscall 19 akan mengembalikan RAM asli dari Bootloader!
uint32_t sys_total_ram() {
    uint32_t ret; 
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(19)); 
    return ret; // KEMBALIKAN ret, BUKAN variabel lokal!
}

uint32_t sys_used_ram() { uint32_t ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(20)); return ret; }

void print_num(uint32_t num) {
    if (num == 0) { print("0"); return; }
    char buf[16]; int i = 14; buf[15] = '\0';
    while (num > 0 && i >= 0) { buf[i--] = (num % 10) + '0'; num /= 10; }
    print(&buf[i + 1]);
}

// Fungsi penyedot identitas prosesor bawaan pabrik!
void get_cpu_string(char* buffer) {
    uint32_t eax, ebx, ecx, edx;
    
    // Macro khusus agar Clang tidak protes soal register EBX
    #define CPUID(code, a, b, c, d) \
        __asm__ volatile ( \
            "pushl %%ebx \n" \
            "cpuid \n" \
            "movl %%ebx, %1 \n" \
            "popl %%ebx \n" \
            : "=a"(a), "=r"(b), "=c"(c), "=d"(d) \
            : "a"(code) \
        )
    
    // 1. Cek apakah CPU mendukung fitur "Brand String" (0x80000000)
    CPUID(0x80000000, eax, ebx, ecx, edx);
    
    if (eax >= 0x80000004) {
        // 2. Jika mendukung, sedot 48 byte teksnya secara bertahap!
        uint32_t* ptr = (uint32_t*)buffer;
        CPUID(0x80000002, ptr[0], ptr[1], ptr[2], ptr[3]);
        CPUID(0x80000003, ptr[4], ptr[5], ptr[6], ptr[7]);
        CPUID(0x80000004, ptr[8], ptr[9], ptr[10], ptr[11]);
        buffer[48] = '\0'; // Tutup string dengan null-terminator
    } else {
        // Jika CPU sangat jadul dan tidak punya Brand String
        buffer[0] = 'U'; buffer[1] = 'n'; buffer[2] = 'k'; buffer[3] = 'n';
        buffer[4] = 'o'; buffer[5] = 'w'; buffer[6] = 'n'; buffer[7] = '\0';
    }
}

void sys_get_time(uint32_t* time_array) {
    __asm__ volatile("int $0x80" : : "a"(21), "b"(time_array));
}

void sys_draw_pixel(int x, int y, uint32_t color) {
    __asm__ volatile("int $0x80" : : "a"(22), "b"(x), "c"(y), "d"(color));
}

void sys_draw_image(int x, int y, int width, int height, uint32_t* buffer) {
    __asm__ volatile(
        "int $0x80"
        : 
        // a=23, b=x, c=y, d=width, S(esi)=height, D(edi)=buffer
        : "a"(23), "b"(x), "c"(y), "d"(width), "S"(height), "D"(buffer) 
    );
}

int sys_get_file_list(file_info_t* buffer, int max_entries) {
    uint32_t ret; 
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(24), "b"(buffer), "c"(max_entries)); 
    return ret;
}

uint32_t sys_load_elf(char* filename) {
    uint32_t entry_point;
    __asm__ volatile("int $0x80" : "=a"(entry_point) : "a"(25), "b"(filename)); 
    return entry_point;
}

void sys_draw_string(const char* str, int x, int y, uint32_t color) {
    __asm__ volatile("int $0x80" : : "a"(26), "b"(str), "c"(x), "d"(y), "S"(color));
}

void sys_set_uid(uint32_t uid) {
    __asm__ volatile("int $0x80" : : "a"(27), "b"(uid)); // Menggunakan 27
}

uint32_t sys_get_uid() {
    uint32_t uid;
    __asm__ volatile("int $0x80" : "=a"(uid) : "a"(28)); // Menggunakan 28
    return uid;
}

int sys_get_event(kyuzen_event_t* event_out) {
    uint32_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(29), "b"(event_out)); 
    return ret;
}