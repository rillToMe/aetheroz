#ifndef USERLIB_H
#define USERLIB_H

#include <stdint.h>
#include <stddef.h>   // size_t

// --- STRUKTUR PESAN EVENT (GUI) ---
#define EVENT_NONE          0
#define EVENT_KEY_PRESS     1   // Key down. P1 = ASCII (shift/caps diterapkan; 0 = non-printable)
                                // P2 = modifier bitmask (KEY_MOD_*), P3 = scancode
#define EVENT_MOUSE_MOVE    2
#define EVENT_MOUSE_CLICK   3
#define EVENT_SCROLL        4   // P1 = delta wheel (+1 bawah / -1 atas)
#define EVENT_KEY_RELEASE   5   // Key up. P1 = ASCII dasar (identitas tombol, tanpa shift/caps)
                                // P2 = modifier bitmask (setelah release diproses), P3 = scancode
#define EVENT_WIN_CLOSE     6   // (Phase 5C — dicadangkan) WM meminta app menutup window.
                                // win_id = window yang diminta; P1..P3 = 0.

// Bitmask modifier keyboard (P2 pada EVENT_KEY_PRESS / EVENT_KEY_RELEASE)
#define KEY_MOD_SHIFT       0x01   // Shift kiri/kanan
#define KEY_MOD_CTRL        0x02   // Ctrl kiri/kanan
#define KEY_MOD_ALT         0x04   // Alt kiri/kanan
#define KEY_MOD_CAPS        0x08   // CapsLock sedang aktif

// P3 = scancode set-1; bit 0x100 menyala = tombol extended (prefix E0,
// mis. Ctrl/Alt kanan, arrow keys). Pairing press↔release via P3, bukan P1.

typedef struct {
    uint32_t type;    // Jenis Event (Key, Mouse, Click)
    int32_t param1;   // Data 1 (ASCII huruf, atau X Mouse, atau Tombol Kiri/Kanan)
    int32_t param2;   // Data 2 (Y Mouse, atau Status Ditekan/Dilepas)
    int32_t param3;   // Tambahan
    int32_t win_id;   // Phase 5B: window tujuan event, diisi KWM saat routing.
                      // Nilai = id slot KWM + 1; 0 = tidak relevan.
} kyuzen_event_t;
// ----------------------------------

void print(char* text);
void clear_screen(void);
uint32_t read_keyboard(char* buffer, uint32_t size);
void sys_yield(void);
void sys_sleep(uint32_t ms);   // Non-busy sleep (Syscall 46)

void fs_format(void);
void fs_list(void);
void fs_read(char* filename);
void fs_delete(char* filename);

void* sys_alloc(uint32_t size);
void sys_free(void* ptr);
void* sys_realloc(void* ptr, uint32_t old_size, uint32_t new_size);
int sys_file_exists(char* filename);
uint32_t sys_file_size(char* filename);
int sys_read_file_to_buffer(char* filename, char* buffer, uint32_t buffer_capacity);
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

// sys_spawn (Phase 5A): jalankan ELF sebagai task ring-3 BARU yang konkuren —
// caller TETAP jalan (beda dengan sys_exec yang menggantikan caller).
// Return: task id (>= 0), atau -1 jika gagal (file tak ada, OOM, slot penuh).
int sys_spawn(char* filename);

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

// fd layer (Fase 5) — file descriptors over KyuzenFS. fd valid per-task.
// open flags
#define O_RDONLY  0x0
#define O_WRONLY  0x1
#define O_RDWR    0x2
#define O_CREAT   0x4
#define O_TRUNC   0x8
#define O_APPEND  0x10
// lseek whence
#define SEEK_SET  0
#define SEEK_CUR  1
#define SEEK_END  2

int sys_open(const char* path, uint32_t flags);        // -> fd (>=0) or -1
int sys_read_fd(int fd, void* buf, uint32_t count);     // -> bytes read
int sys_write_fd(int fd, const void* buf, uint32_t count); // -> bytes written
int sys_lseek(int fd, int32_t offset, int whence);      // -> new position
int sys_close(int fd);                                  // -> 0 or -1

// TCP client sockets (Fase 6). ip_be = IPv4 in network byte order.
int sys_socket(void);                                   // -> sockfd or -1
int sys_connect(int s, uint32_t ip_be, uint16_t port);  // 0 ok, -1 fail
int sys_send(int s, const void* buf, uint32_t len);     // bytes sent or -1
int sys_recv(int s, void* buf, uint32_t len);           // bytes, 0=closed, -1=err
int sys_sock_close(int s);                              // -> 0 or -1

#endif
