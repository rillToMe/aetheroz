#include <stdint.h>
#include <stddef.h>
#include "multiboot.h"  
#define FONT8x16_IMPLEMENTATION 
#include "font8x16.h" // <-- Pustaka font ajaib yang baru kita unduh!
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


extern void init_gdt();
extern void init_idt();
extern void pic_remap();
extern void init_keyboard();
extern void switch_to_user_mode(void (*user_func)());
extern void user_shell();
extern void init_mouse();

// --- VARIABEL GLOBAL FRAMEBUFFER ---
uint32_t* fb_ptr = NULL;
uint32_t fb_width = 0;
uint32_t fb_height = 0;
uint32_t fb_pitch = 0;

// KANVAS BAYANGAN (BACKBUFFER) DI RAM
// 1024x768 = 786.432 Piksel. Bootloader akan otomatis mengalokasikan RAM ini!
uint32_t backbuffer[1024 * 768]; 

void draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= fb_width || y >= fb_height) return;
    // PENTING: Semua lukisan OS sekarang masuk ke RAM (Z-Index 0 & 1), bukan ke layar fisik!
    backbuffer[(y * (fb_pitch / 4)) + x] = color;
}

// --- MESIN COMPOSITOR Z-INDEX ---
extern void draw_mouse_to_frontbuffer();

// --- VARIABEL MOUSE DARI DRIVER ---
extern int32_t mouse_x;
extern int32_t mouse_y;
extern const uint8_t cursor_bitmap[16][12];

// Memori untuk menyimpan latar belakang mouse di dalam RAM
uint32_t mouse_bg_backbuffer[16][12];

// --- MESIN COMPOSITOR Z-INDEX (ZERO FLICKER) ---
void compositor_flush() {
    if (fb_width == 0) return;

    // 1. TEMPELKAN MOUSE KE BACKBUFFER (RAM)
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 12; x++) {
            if (mouse_y + y >= (int32_t)fb_height || mouse_x + x >= (int32_t)fb_width) continue;
            
            uint32_t offset = ((mouse_y + y) * (fb_pitch / 4)) + (mouse_x + x);
            
            // Simpan piksel asli backbuffer
            mouse_bg_backbuffer[y][x] = backbuffer[offset];
            
            // Timpa dengan warna kursor mouse
            if (cursor_bitmap[y][x] == 1) backbuffer[offset] = 0xFFFFFF; 
            else if (cursor_bitmap[y][x] == 2) backbuffer[offset] = 0x000000; 
        }
    }

    // 2. TUMPAHKAN 1 LAYAR PENUH KE MONITOR SECEPAT KILAT (HARDWARE ASSEMBLY)
    // Trik rep movsl ini menjamin kopi memori tercepat tanpa risiko crash SSE!
    uint32_t* dest = fb_ptr;
    uint32_t* src  = backbuffer;
    uint32_t count = (fb_pitch / 4) * fb_height;
    
    __asm__ volatile (
        "rep movsl"
        : "+D" (dest), "+S" (src), "+c" (count)
        :
        : "memory"
    );

    // 3. CABUT MOUSE DARI BACKBUFFER (RAM)
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 12; x++) {
            if (mouse_y + y >= (int32_t)fb_height || mouse_x + x >= (int32_t)fb_width) continue;
            
            uint32_t offset = ((mouse_y + y) * (fb_pitch / 4)) + (mouse_x + x);
            backbuffer[offset] = mouse_bg_backbuffer[y][x];
        }
    }
}

void draw_rect(uint32_t start_x, uint32_t start_y, uint32_t width, uint32_t height, uint32_t color) {
    for (uint32_t y = start_y; y < start_y + height; y++) {
        for (uint32_t x = start_x; x < start_x + width; x++) {
            draw_pixel(x, y, color);
        }
    }
}

// --- MESIN PENGGAMBAR TEKS GRAFIS ---
void draw_char(char c, uint32_t x, uint32_t y, uint32_t color) {
    if (c < 0 || c > 127) return;

    // Ambil cetak biru biner untuk huruf dari array font8x16
    const unsigned char* bitmap = font8x16[(int)c];

    for (int row = 0; row < 16; row++) { // Sekarang tingginya 16 piksel murni!
        for (int col = 0; col < 8; col++) {
            // Pustaka ini menggunakan urutan MSB (Most Significant Bit)
            // Jadi kita gunakan 0x80 (10000000) yang digeser ke kanan
            if (bitmap[row] & (0x80 >> col)) {
                draw_pixel(x + col, y + row, color);
            }
        }
    }
}

void draw_string(const char* str, uint32_t x, uint32_t y, uint32_t color) {
    uint32_t curr_x = x;
    uint32_t curr_y = y;

    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            curr_y += 16; // Jarak Vertikal (8px untuk huruf + 8px spasi kosong)
            curr_x = x;   
        } else {
            draw_char(str[i], curr_x, curr_y, color);
            curr_x += 8; // Geser ke kanan sesuai lebar font (8px)
        }
    }
}
// ------------------------------------

void kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    // 1. TANGKAP LAYAR GUI DARI MULTIBOOT
    if (magic == 0x2BADB002 && (mbi->flags & (1 << 12))) {
        fb_ptr = (uint32_t *)(uint32_t)mbi->framebuffer_addr;
        fb_width = mbi->framebuffer_width;
        fb_height = mbi->framebuffer_height;
        fb_pitch = mbi->framebuffer_pitch;

        uint32_t total_ram = (mbi->mem_upper * 1024) + (1024 * 1024);
        pmm_set_total_ram(total_ram);
    }

    // 2. INISIALISASI ARSITEKTUR KERNEL
    init_gdt(); 
    init_idt(); 
    pmm_init(); 
    init_paging((uint32_t)fb_ptr);
    init_heap();
    pic_remap(); 
    init_timer(50); 
    init_mouse();
    init_keyboard();
    // 3. AKTIFKAN LAYAR TTY (Harus sebelum FS agar kprint tidak error)
    init_tty(); 

    // 4. INISIALISASI DISK
    kfs_init();

    // 5. AUTO-INSTALL GAMBAR DARI CD KE HARD DISK
    // Cek bit ke-3 dari flags untuk memastikan Multiboot Modules tersedia
    if (mbi->flags & (1 << 3)) {
        if (mbi->mods_count > 0) {
            // Ambil array dari modul yang dimuat (logo.png)
            uint32_t* mods = (uint32_t*)mbi->mods_addr;
            uint32_t start_addr = mods[0]; // Alamat awal gambar di RAM
            uint32_t end_addr   = mods[1]; // Alamat akhir gambar di RAM
            uint32_t file_size  = end_addr - start_addr;

            // Jika belum ada di disk.img, langsung bakar ke disk!
            if (!kfs_exists("logo.png")) {
                kfs_create_file("logo.png", (char*)start_addr, file_size);
            }
        }
    }

    // 6. LOMPAT KE USER SPACE (Menjalankan kyuzen-shell!)
    switch_to_user_mode(user_shell);

    // Fallback jika Ring 3 gagal
    __asm__ volatile("cli");
    while (1) { __asm__ volatile("hlt"); }
}


// Dummy untuk linker
extern fs_node_t tty_node;
uint32_t string_length(const char* str) {
    uint32_t len = 0;
    while (str[len]) len++;
    return len;
}
void print_hex(uint32_t num) { 
    (void)num; // Trik membungkam compiler
}

// Impor fungsi penyetel stack khusus untuk mode user
extern void set_kernel_stack(uint32_t stack);

// --- KEMBALIKAN LOGIKA LOMPATAN RING 3 ---
void switch_to_user_mode(void (*user_func)()) {
    // PERBESAR: Berikan 1 MB (1024 * 1024) untuk Stack Aplikasi Ring 3
    uint32_t user_stack_size = 1024 * 1024;
    uint32_t user_stack = (uint32_t)kmalloc(user_stack_size) + user_stack_size;

    // PERBESAR: Berikan 64 KB (65536) untuk Stack Kernel saat Syscall
    uint32_t kernel_stack_size = 65536;
    uint32_t kernel_landing_stack = (uint32_t)kmalloc(kernel_stack_size) + kernel_stack_size;
    
    set_kernel_stack(kernel_landing_stack);

    // Assembly sakti untuk memanipulasi register (Sama seperti sebelumnya)
    __asm__ volatile(
        "cli \n" "mov $0x23, %%ax \n" "mov %%ax, %%ds \n" "mov %%ax, %%es \n"
        "mov %%ax, %%fs \n" "mov %%ax, %%gs \n" "pushl $0x23 \n" "pushl %0 \n"            
        "pushfl \n" "popl %%eax \n" "orl $0x200, %%eax \n" "pushl %%eax \n"         
        "pushl $0x1B \n" "pushl %1 \n" "iret \n"                
        : : "r"(user_stack), "r"(user_func) : "%eax"
    );
}