#include <stdint.h>
#include <stddef.h>
#include "limine.h" // <-- Kembali menggunakan Limine Native!
#define FONT8x16_IMPLEMENTATION 
#include "font8x16.h"
#include "fs.h"
#include "tty.h"
#include "pmm.h"
#include "paging.h"
#include "heap.h"
#include "string.h"
#include "ata.h"
#include "kyuzenfs.h"
#include "task.h"
#include "timer.h"
#include "shell.h"

// ============================================================
// LIMINE REQUESTS — Harus di section .requests agar bootloader bisa scan
// ============================================================
__attribute__((used, section(".requests_start_marker")))
static volatile uint64_t __limine_requests_start[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used)) static volatile uint64_t base_revision[] = LIMINE_BASE_REVISION(3);

__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".requests")))
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 0
};

// HHDM: Limine memetakan SELURUH RAM fisik di offset ini
// Physical address P accessible di hhdm_offset + P
__attribute__((used, section(".requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".requests_end_marker")))
static volatile uint64_t __limine_requests_end[] = LIMINE_REQUESTS_END_MARKER;
// ============================================================

// HHDM offset: dipakai oleh paging.c untuk convert phys → virt
uint64_t hhdm_offset = 0;


extern void init_gdt();
extern void init_idt();
extern void pic_remap();
extern void init_keyboard();
extern void switch_to_user_mode(void (*user_func)());
extern void user_login();
extern void init_mouse();
extern void kfs_delete_file(char* filename);
// terminal_putchar tidak lagi dibutuhkan langsung (kprint ada di kyuzenfs.c)

// --- VARIABEL GLOBAL FRAMEBUFFER ---
uint32_t* fb_ptr = NULL;
uint32_t fb_width = 0;
uint32_t fb_height = 0;
uint32_t fb_pitch = 0;

// Buffer resolusi maksimal 1920x1080 — cukup untuk semua konfigurasi QEMU/HW
// Jika base_canvas terlalu kecil dari fb_width*fb_height, pixel wrap dan muncul dua kali
uint32_t backbuffer[1920 * 1080];
uint32_t base_canvas[1920 * 1080];

// kprint didefinisikan di kernel/kyuzenfs.c (via write_fs & tty_node)
extern void kprint(const char* str);

void kprint_num(uint64_t num) {
    if (num == 0) { kprint("0"); return; }
    char buf[20]; int i = 18; buf[19] = '\0';
    while (num > 0) { buf[i--] = (num % 10) + '0'; num /= 10; }
    kprint(&buf[i + 1]);
}

void draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= fb_width || y >= fb_height) return;
    base_canvas[(y * (fb_pitch / 4)) + x] = color;
}

// --- KYUZEN WINDOW MANAGER (KWM) ---
#define MAX_WINDOWS 16
typedef struct {
    uint8_t active;
    int32_t x, y;
    uint32_t width, height;
    uint32_t* canvas; 
    uint32_t z_index;
} kwm_window_t;

kwm_window_t kwm_windows[MAX_WINDOWS];
uint32_t next_z_index = 1;

int kwm_create_window(int x, int y, uint32_t width, uint32_t height) {
    for(int i = 0; i < MAX_WINDOWS; i++) {
        if(!kwm_windows[i].active) {
            kwm_windows[i].active = 1;
            kwm_windows[i].x = x;
            kwm_windows[i].y = y;
            kwm_windows[i].width = width;
            kwm_windows[i].height = height;
            kwm_windows[i].canvas = (uint32_t*)kmalloc(width * height * 4);
            kwm_windows[i].z_index = next_z_index++;
            return i;
        }
    }
    return -1; 
}

void kwm_update_window(int win_id, uint32_t* app_buffer) {
    if(win_id < 0 || win_id >= MAX_WINDOWS || !kwm_windows[win_id].active) return;
    if(!kwm_windows[win_id].canvas || !app_buffer) return; 

    uint32_t size = kwm_windows[win_id].width * kwm_windows[win_id].height; 
    uint32_t* dest = kwm_windows[win_id].canvas;
    __asm__ volatile ("rep movsl" : "+D" (dest), "+S" (app_buffer), "+c" (size) : : "memory");
}

void kwm_destroy_window(int win_id) {
    if(win_id < 0 || win_id >= MAX_WINDOWS || !kwm_windows[win_id].active) return;
    if (kwm_windows[win_id].canvas) kfree(kwm_windows[win_id].canvas);
    kwm_windows[win_id].active = 0; 
}

extern int32_t mouse_x;
extern int32_t mouse_y;
extern const uint8_t cursor_bitmap[16][12];

void compositor_flush() {
    if (fb_width == 0) return;
    uint32_t screen_size = (fb_pitch / 4) * fb_height;

    uint32_t* dst_bg = backbuffer;
    uint32_t* src_bg = base_canvas;
    uint64_t copy_cnt = screen_size; // 64-bit counter (rcx)
    __asm__ volatile ("rep movsl" : "+D" (dst_bg), "+S" (src_bg), "+c" (copy_cnt) : : "memory");

    for(uint32_t z = 1; z <= next_z_index; z++) {
        for(int w = 0; w < MAX_WINDOWS; w++) {
            if(kwm_windows[w].active && kwm_windows[w].z_index == z) {
                uint32_t win_w = kwm_windows[w].width;
                uint32_t win_h = kwm_windows[w].height;
                uint32_t* canvas = kwm_windows[w].canvas;

                for(uint32_t wy = 0; wy < win_h; wy++) {
                    for(uint32_t wx = 0; wx < win_w; wx++) {
                        int screen_x = kwm_windows[w].x + wx;
                        int screen_y = kwm_windows[w].y + wy;
                        
                        if(screen_x >= 0 && screen_x < (int)fb_width && screen_y >= 0 && screen_y < (int)fb_height) {
                            uint32_t pixel = canvas[(wy * win_w) + wx];
                            if (pixel >> 24) {
                                backbuffer[(screen_y * (fb_pitch / 4)) + screen_x] = pixel & 0xFFFFFF;
                            }
                        }
                    }
                }
            }
        }
    }

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 12; x++) {
            if (mouse_y + y >= (int32_t)fb_height || mouse_x + x >= (int32_t)fb_width) continue;
            uint32_t offset = ((mouse_y + y) * (fb_pitch / 4)) + (mouse_x + x);
            if (cursor_bitmap[y][x] == 1) backbuffer[offset] = 0xFFFFFF; 
            else if (cursor_bitmap[y][x] == 2) backbuffer[offset] = 0x000000; 
        }
    }

    uint32_t* dest_mon = fb_ptr;
    uint32_t* src_mon  = backbuffer;
    uint64_t count_mon = screen_size;
    __asm__ volatile ("rep movsl" : "+D" (dest_mon), "+S" (src_mon), "+c" (count_mon) : : "memory");
}

void draw_rect(uint32_t start_x, uint32_t start_y, uint32_t width, uint32_t height, uint32_t color) {
    for (uint32_t y = start_y; y < start_y + height; y++) {
        for (uint32_t x = start_x; x < start_x + width; x++) {
            draw_pixel(x, y, color);
        }
    }
}

void draw_image(int start_x, int start_y, int width, int height, uint32_t* buffer) {
    int i = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint32_t pixel = buffer[i++];
            uint8_t alpha = (pixel >> 24) & 0xFF;
            if (alpha > 0) {
                draw_pixel(start_x + x, start_y + y, pixel & 0xFFFFFF);
            }
        }
    }
}

void draw_char(char c, uint32_t x, uint32_t y, uint32_t color) {
    if (c < 0 || c > 127) return;
    const unsigned char* bitmap = font8x16[(int)c];
    for (int row = 0; row < 16; row++) { 
        for (int col = 0; col < 8; col++) {
            if (bitmap[row] & (0x80 >> col)) draw_pixel(x + col, y + row, color);
        }
    }
}

void draw_string(const char* str, uint32_t x, uint32_t y, uint32_t color) {
    uint32_t curr_x = x;
    uint32_t curr_y = y;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') { curr_y += 16; curr_x = x; } 
        else { draw_char(str[i], curr_x, curr_y, color); curr_x += 8; }
    }
}

// ----> ENTRY POINT 64-BIT BERSIH <---
void kernel_main(void) {
    // 0. AMBIL HHDM OFFSET — WAJIB SEBELUM APA PUN (dipakai oleh paging.c)
    if (hhdm_request.response != NULL) {
        hhdm_offset = hhdm_request.response->offset;
    } else {
        // Fallback: Limine default HHDM biasanya di 0xFFFF800000000000
        hhdm_offset = 0xFFFF800000000000ULL;
    }

    // 1. TANGKAP LAYAR DARI LIMINE
    if (framebuffer_request.response != NULL && framebuffer_request.response->framebuffer_count > 0) {
        struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
        fb_ptr = (uint32_t *)fb->address;
        fb_width = fb->width;
        fb_height = fb->height;
        fb_pitch = fb->pitch;
    } else {
        while(1) { __asm__ volatile("hlt"); } 
    }

    init_gdt(); 
    init_idt(); 

    // 2. PMM DYNAMIC VIA LIMINE
    if (memmap_request.response != NULL) {
        pmm_init_dynamic(memmap_request.response->entries, memmap_request.response->entry_count);
    } else {
        kprint("PANIC: Bootloader tidak mengirim Memory Map!\n");
        while(1) { __asm__ volatile("hlt"); }
    }

    init_paging(0); // paging.c membaca CR3 langsung, parameter tidak dipakai
    init_heap();
    pic_remap(); 
    init_timer(50); 
    init_mouse();
    init_keyboard();
    init_tty(); 
    kfs_init();

    // 3. AUTO-INSTALL MODUL DARI LIMINE
    kprint("\n--- RADAR AUTO-INSTALL ---\n");
    if (module_request.response != NULL && module_request.response->module_count > 0) {
        kprint("Status: Limine mengirim modul!\n");
        kprint("Jumlah Modul: "); kprint_num(module_request.response->module_count); kprint("\n");
        
        for (uint64_t i = 0; i < module_request.response->module_count; i++) {
            struct limine_file *mod = module_request.response->modules[i];
            uint64_t size = mod->size;
            kprint("Modul "); kprint_num(i+1); kprint(" | Ukuran: "); kprint_num(size); kprint(" Bytes\n");
            
            char* raw_name = mod->path;
            if (raw_name == NULL) continue;
            kprint(" -> Raw string: ["); kprint(raw_name); kprint("]\n");
            
            char clean_name[24];
            int k = 0;
            char* last_slash = raw_name;
            for (int j = 0; raw_name[j] != '\0'; j++) {
                if (raw_name[j] == '/') last_slash = &raw_name[j + 1];
            }
            
            for (int j = 0; last_slash[j] != '\0' && last_slash[j] != ' ' && last_slash[j] != '\n' && k < 22; j++) {
                clean_name[k] = last_slash[j]; k++;
            }
            clean_name[k] = '\0';
            
            kprint(" -> Ekstrak Nama: ["); kprint(clean_name); kprint("]\n");

            if (k == 0) continue;

            if (kfs_exists(clean_name)) {
                kfs_delete_file(clean_name);
            }

            int res = kfs_create_file(clean_name, (char*)mod->address, size);
            if(res) kprint(" -> [SUKSES DITULIS KE DISK]\n");
            else kprint(" -> [GAGAL DITULIS]\n");
        }
    } else {
        kprint("ERROR: LIMINE TIDAK MENGIRIM MODUL SAMA SEKALI!\n");
    }
    kprint("--------------------------\n");

    // Aktifkan interrupts SEBELUM masuk ke user code
    // Tanpa sti: timer IRQ tidak pernah fire, keyboard beku, OS freeze!
    __asm__ volatile("sti");

    switch_to_user_mode(user_login);

    __asm__ volatile("cli");
    while (1) { __asm__ volatile("hlt"); }
}

extern fs_node_t tty_node;
uint32_t string_length(const char* str) {
    uint32_t len = 0;
    while (str[len]) len++;
    return len;
}
void print_hex(uint32_t num) { kprint_num(num); }


void switch_to_user_mode(void (*user_func)()) {
    // Ring 3 belum diimplementasikan — panggil langsung di Ring 0
    // (Syscall via int $0x80 tetap bekerja karena IDT sudah di-setup)
    if (user_func) user_func();
}