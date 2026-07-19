#ifndef USERLIB_H
#define USERLIB_H

#include <stdint.h>
#include <stddef.h>   // size_t

// --- STRUKTUR PESAN EVENT (GUI) ---
#define EVENT_NONE          0
#define EVENT_KEY_PRESS     1
#define EVENT_MOUSE_MOVE    2
#define EVENT_MOUSE_CLICK   3

typedef struct {
    uint32_t type;    // Jenis Event (Key, Mouse, Click)
    int32_t param1;   // Data 1 (ASCII huruf, atau X Mouse, atau Tombol Kiri/Kanan)
    int32_t param2;   // Data 2 (Y Mouse, atau Status Ditekan/Dilepas)
    int32_t param3;   // Tambahan
} kyuzen_event_t;
// ----------------------------------

void print(char* text);
void clear_screen(void);
uint32_t read_keyboard(char* buffer, uint32_t size);
void sys_yield(void);

void fs_format(void);
void fs_list(void);
void fs_read(char* filename);
void fs_delete(char* filename);

void* sys_alloc(uint32_t size);
void sys_free(void* ptr);
void* sys_realloc(void* ptr, uint32_t old_size, uint32_t new_size);
int sys_file_exists(char* filename);
uint32_t sys_file_size(char* filename);
int sys_read_file_to_buffer(char* filename, char* buffer);
int sys_create_file(char* filename, char* data, uint32_t size);

uint64_t sys_uptime(void);

uint64_t sys_total_ram(void);
uint64_t sys_used_ram(void);

void print_num(uint32_t num);
extern int strcmp(const char *s1, const char *s2);

void get_cpu_string(char* buffer);


void sys_get_time(uint32_t* time_array);

void sys_draw_pixel(int x, int y, uint32_t color);

void sys_draw_image(int x, int y, int width, int height, uint32_t* buffer);

typedef struct {
    char filename[24];
    uint32_t size;
    uint8_t is_folder;
} file_info_t;

int sys_get_file_list(file_info_t* buffer, int max_entries);

uint64_t sys_load_elf(char* filename);

// sys_exec: Load app baru, replace current app, TIDAK PERNAH kembali ke caller.
// OS yang free RAM lama, load app baru, lalu lompat langsung ke entry-nya.
void sys_exec(char* filename);

// sys_exit: App selesai, kembali ke shell. TIDAK PERNAH kembali ke caller.
void sys_exit(void);
void sys_draw_string(const char* str, int x, int y, uint32_t color);

void sys_set_uid(uint32_t uid);
uint32_t sys_get_uid();

// 2. Syscall Event Queue (Syscall 29)
int sys_get_event(kyuzen_event_t* event_out);

// 3. Syscall Window Manager (Syscall 30, 31, 32, 40)
int sys_create_window(int x, int y, uint32_t width, uint32_t height);
void sys_update_window(int win_id, uint32_t* buffer);
void sys_destroy_window(int win_id);
// Query posisi window terkini dari kernel (setelah drag, posisi berubah)
// Selalu panggil ini sebelum hit-test tombol, JANGAN hardcode koordinat!
void sys_get_window_pos(int win_id, int* out_x, int* out_y);

void* memcpy(void* dest, const void* src, size_t count);
void* memset(void* dest, int val, size_t count);

int sys_kwm_create_window(int x, int y, uint32_t width, uint32_t height);
void sys_kwm_update_window(int win_id, uint32_t* buffer);
void sys_kwm_destroy_window(int win_id);

void sys_shutdown(void);
void sys_reboot(void);

//statistik hardware
uint32_t sys_get_total_disk(void);
uint32_t sys_get_used_disk(void); 
uint32_t sys_get_cpu_usage(void);

// sys_ping: kirim ICMP Echo Request (ping) ke host.
// host = nama domain atau IP string ("google.com" atau "8.8.8.8")
// Return: rata-rata RTT dalam ms jika berhasil, -1 jika gagal/timeout
// Output ping dicetak langsung oleh kernel (kprint) ke TTY.
int sys_ping(const char *host);

// Diagnostic: process isolation testing
uint64_t sys_get_cr3(void);   // Return CR3 physical address (PML4 pointer)
int sys_get_task_id(void);    // Return current task ID (-1 if idle)
int sys_is_mapped(void* addr); // Return 1 if addr is mapped, 0 if not (safe probe)
uint32_t sys_get_pid(void);   // Return per-AS unique cookie (OS-generated)

#endif
