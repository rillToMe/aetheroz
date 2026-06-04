#include "fs.h"
#include <stdint.h>

extern fs_node_t tty_node;
extern uint32_t string_length(const char* str);
extern uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
extern uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
extern void tty_clear(void);
extern void yield(void); 

// Impor fungsi KyuzenFS & Heap
extern void kfs_format(void);
extern void kfs_list_files(void);
extern void kfs_read_file(char* filename);
extern void kfs_delete_file(char* filename);
extern void* kmalloc(uint32_t size);
extern void kfree(void* ptr);
extern void* krealloc(void* ptr, uint32_t old_size, uint32_t new_size);
extern int kfs_exists(char* filename);
extern uint32_t kfs_get_file_size(char* filename);
extern int kfs_read_to_buffer(char* filename, char* out_buffer);
extern int kfs_create_file(char* filename, char* data, uint32_t size);
extern int kfs_get_file_list(void* buffer, int max_entries);
extern uint32_t elf_load_file(char* filename);
extern void draw_string(const char* str, uint32_t x, uint32_t y, uint32_t color);

// Impor fungsi Sistem Statistik
extern uint32_t get_uptime(void);
extern uint32_t pmm_get_total_ram(void);
extern uint32_t pmm_get_used_ram(void);

// times
extern void read_rtc(uint32_t* time_buf);

extern void draw_pixel(int x, int y, uint32_t color);
extern void draw_image(int x, int y, int width, int height, uint32_t* buffer);
typedef struct {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
} registers_t;

uint32_t syscall_handler(registers_t *r) {
    if (r->eax == 1) { 
        write_fs(&tty_node, 0, string_length((char*)r->ebx), (uint8_t*)r->ebx); return 0; 
    } 
    else if (r->eax == 2) { tty_clear(); return 0; }
    else if (r->eax == 3) {
        char* msg = "\n[KERNEL] Aplikasi Ring 3 minta Exit. (System Halt)\n";
        write_fs(&tty_node, 0, string_length(msg), (uint8_t*)msg);
        __asm__ volatile("cli"); while(1) { __asm__ volatile("hlt"); } 
        return 0; 
    }
    else if (r->eax == 4) { return read_fs(&tty_node, 0, r->ecx, (uint8_t*)r->ebx); }
    else if (r->eax == 5) { yield(); return 0; }
    
    // Syscall FS Dasar
    else if (r->eax == 6) { kfs_format(); return 0; }
    else if (r->eax == 7) { kfs_list_files(); return 0; }
    else if (r->eax == 8) { kfs_read_file((char*)r->ebx); return 0; }
    else if (r->eax == 9) { kfs_delete_file((char*)r->ebx); return 0; }
    
    // --- SYSCALL BARU: MANAJEMEN MEMORI & FS LANJUTAN UNTUK ZEN EDITOR ---
    else if (r->eax == 11) { return (uint32_t)kmalloc(r->ebx); }
    else if (r->eax == 12) { kfree((void*)r->ebx); return 0; }
    else if (r->eax == 13) { return (uint32_t)krealloc((void*)r->ebx, r->ecx, r->edx); }
    else if (r->eax == 14) { return kfs_exists((char*)r->ebx); }
    else if (r->eax == 15) { return kfs_get_file_size((char*)r->ebx); }
    else if (r->eax == 16) { return kfs_read_to_buffer((char*)r->ebx, (char*)r->ecx); }
    else if (r->eax == 17) { return kfs_create_file((char*)r->ebx, (char*)r->ecx, r->edx); }
    // --- SYSCALL BARU: STATISTIK SISTEM ---
    else if (r->eax == 18) { return get_uptime(); }
    else if (r->eax == 19) { return pmm_get_total_ram(); }
    else if (r->eax == 20) { return pmm_get_used_ram(); }
    else if (r->eax == 21) {
        // R->EBX adalah pointer array kosong dari User Space
        read_rtc((uint32_t*)r->ebx);
        return 0;
    }
    else if (r->eax == 22) {
        draw_pixel((int)r->ebx, (int)r->ecx, (uint32_t)r->edx);
        return 0;
    }
    else if (r->eax == 23) {
        draw_image((int)r->ebx, (int)r->ecx, (int)r->edx, (int)r->esi, (uint32_t*)r->edi);
        return 0;
    }
    else if (r->eax == 24) {
        return kfs_get_file_list((void*)r->ebx, (int)r->ecx);
    }
    else if (r->eax == 25) {
        return elf_load_file((char*)r->ebx);
    }
    else if (r->eax == 26) {
        draw_string((const char*)r->ebx, (int)r->ecx, (int)r->edx, (uint32_t)r->esi);
        return 0;
    }
    return (uint32_t)-1;
}