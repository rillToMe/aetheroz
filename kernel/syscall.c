#include "fs.h"
#include <stdint.h>
#include "userlib.h"

// ========================================================
// STRUKTUR REGISTER 64-BIT (MURNI)
// Harus 100% cocok dengan urutan PUSHA64 di isr_macro.inc
// ========================================================
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;  // Register x86_64 manual
    uint64_t int_num, error_code;                // Kode Error
    uint64_t rip, cs, rflags, rsp, ss;           // Otomatis di-push oleh CPU 64-bit
} __attribute__((packed)) registers_t;


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

extern void get_cpu_string(char* buffer);
extern uint32_t pmm_get_used_ram(void);
extern uint32_t pmm_get_total_ram(void);
extern uint32_t kfs_get_total_space(void);
extern uint32_t kfs_get_used_space(void);
extern uint32_t get_cpu_usage(void);
// Counter: setiap kali sys_yield dipanggil, tambah counter ini.
// Timer membaca dan mereset setiap tick untuk menentukan apakah CPU idle.
volatile uint32_t yield_counter = 0;

// Impor dari KWM (Kyuzen Window Manager)
extern void draw_pixel(uint32_t x, uint32_t y, uint32_t color);
extern void draw_image(int start_x, int start_y, int width, int height, uint32_t* buffer);
extern void draw_string(const char* str, uint32_t x, uint32_t y, uint32_t color);

#include "timer.h"  // timer_get_ticks(), timer_get_cpu_usage()

int current_uid = 0; // Definisi global — UID proses yang sedang berjalan

// ========================================================
// HANDLER SYSCALL 64-BIT
// (Dipanggil oleh isr128_stub saat aplikasi melempar int 0x80)
// ========================================================
void syscall_handler(registers_t *r) {
    // Nomor Syscall selalu ada di RAX
    uint64_t syscall_num = r->rax;
    uint64_t ret_val = 0; // Default return

    // --- Pemetaan Argumen Standar Kyuzen OS 64-bit ---
    // RAX = Nomor Syscall
    // RBX = Argumen 1
    // RCX = Argumen 2
    // RDX = Argumen 3
    // RSI = Argumen 4
    // RDI = Argumen 5

    if (syscall_num == 1) { // sys_print
        write_fs(&tty_node, 0, string_length((char*)r->rbx), (uint8_t*)r->rbx);
    } 
    else if (syscall_num == 2) { // sys_clear_screen
        tty_clear();
    } 
    else if (syscall_num == 3) { // sys_read_keyboard
        ret_val = read_fs(&tty_node, 0, r->rcx, (uint8_t*)r->rbx);
    }
    else if (syscall_num == 4) { // sys_yield
        yield_counter++;         // Tandai bahwa app sedang idle/menunggu
        yield();                 // Context switch (atau return jika single-task)
    }
    else if (syscall_num == 5) { // sys_fs_format
        kfs_format();
    }
    else if (syscall_num == 6) { // sys_fs_list
        kfs_list_files();
    }
    else if (syscall_num == 7) { // sys_fs_read
        kfs_read_file((char*)r->rbx);
    }
    else if (syscall_num == 8) { // sys_fs_delete
        kfs_delete_file((char*)r->rbx);
    }
    else if (syscall_num == 9) { // sys_alloc (kmalloc)
        ret_val = (uint64_t)kmalloc((uint32_t)r->rbx);
    }
    else if (syscall_num == 10) { // sys_free (kfree)
        kfree((void*)r->rbx);
    }
    else if (syscall_num == 11) { // sys_file_exists
        ret_val = kfs_exists((char*)r->rbx);
    }
    else if (syscall_num == 12) { // sys_file_size
        ret_val = kfs_get_file_size((char*)r->rbx);
    }
    else if (syscall_num == 13) { // sys_read_file_to_buffer
        ret_val = kfs_read_to_buffer((char*)r->rbx, (char*)r->rcx);
    }
    else if (syscall_num == 14) { // sys_uptime → returns ms sejak boot (hardware-agnostic)
        ret_val = timer_get_ms();
    }

    else if (syscall_num == 15) { // sys_total_ram
        ret_val = pmm_get_total_ram();
    }
    else if (syscall_num == 16) { // sys_used_ram
        ret_val = pmm_get_used_ram();
    }
    else if (syscall_num == 17) { // get_cpu_string
        get_cpu_string((char*)r->rbx);
    }
    else if (syscall_num == 18) { // sys_create_file
        ret_val = kfs_create_file((char*)r->rbx, (char*)r->rcx, (uint32_t)r->rdx);
    }
    else if (syscall_num == 19) { // krealloc
        ret_val = (uint64_t)krealloc((void*)r->rbx, (uint32_t)r->rcx, (uint32_t)r->rdx);
    }
    else if (syscall_num == 20) { // sys_get_time
        extern void rtc_read_time(uint32_t*);
        rtc_read_time((uint32_t*)r->rbx);
    }
    else if (syscall_num == 21) {
        // Reserved/Unused
    }
    else if (syscall_num == 22) { // sys_draw_pixel
        draw_pixel((uint32_t)r->rbx, (uint32_t)r->rcx, (uint32_t)r->rdx);
    }
    else if (syscall_num == 23) { // sys_draw_image
        draw_image((int)r->rbx, (int)r->rcx, (int)r->rdx, (int)r->rsi, (uint32_t*)r->rdi);
    }
    else if (syscall_num == 24) { // sys_get_file_list
        ret_val = kfs_get_file_list((void*)r->rbx, (int)r->rcx);
    }
    else if (syscall_num == 25) { // sys_load_elf
        ret_val = elf_load_file((char*)r->rbx);
    }
    else if (syscall_num == 26) { // sys_draw_string
        draw_string((const char*)r->rbx, (int)r->rcx, (int)r->rdx, (uint32_t)r->rsi);
    }
    // --- SYSCALL: MULTI-USER IDENTITY ---
    else if (syscall_num == 27) { // sys_set_uid
        current_uid = r->rbx; 
    }
    else if (syscall_num == 28) { // sys_get_uid
        ret_val = current_uid;   
    }
    // --- SYSCALL: EVENT QUEUE UNTUK GUI ---
    else if (syscall_num == 29) { // sys_poll_event
        kyuzen_event_t* out_event = (kyuzen_event_t*)r->rbx;
        
        extern int mouse_x, mouse_y;
        extern uint8_t mouse_left_clicked;

        // Poll event: klik atau gerakan mouse
        if (mouse_left_clicked) {
            out_event->type   = EVENT_MOUSE_CLICK;
            out_event->param1 = 0;        // 0 = Tombol Kiri (fileman cek param1==0!)
            out_event->param2 = 1;        // 1 = Ditekan
            out_event->param3 = mouse_x;  // Koordinat X saat klik — tidak perlu cache MOVE
            mouse_left_clicked = 0;
            ret_val = 1;
        } else {
            // Kirim posisi mouse saat ini (selalu, agar app bisa update hover)
            out_event->type   = EVENT_MOUSE_MOVE;
            out_event->param1 = mouse_x;
            out_event->param2 = mouse_y;
            out_event->param3 = 0;
            ret_val = 1;
        }
    }
    // --- SYSCALL BARU UNTUK KWM (Window Manager) ---
    else if (syscall_num == 30) { // sys_kwm_create_window
        extern int kwm_create_window(int, int, uint32_t, uint32_t);
        ret_val = kwm_create_window((int)r->rbx, (int)r->rcx, (uint32_t)r->rdx, (uint32_t)r->rsi);
    }
    else if (syscall_num == 31) { // sys_kwm_update_window
        extern void kwm_update_window(int, uint32_t*);
        kwm_update_window((int)r->rbx, (uint32_t*)r->rcx);
    }
    else if (syscall_num == 32) { // sys_kwm_destroy_window
        extern void kwm_destroy_window(int);
        kwm_destroy_window((int)r->rbx);
    }
    else if (syscall_num == 33) { // sys_exec — load & jalankan ELF baru, replace current app
        // PENTING: copy filename ke kernel stack DULU sebelum unmap!
        char kfname[64];
        {
            char* ufname = (char*)r->rbx;
            int fi = 0;
            while (fi < 63 && ufname[fi] != '\0') {
                kfname[fi] = ufname[fi];
                fi++;
            }
            kfname[fi] = '\0';
        }

        // 1. Unmap user space lama
        extern void vmm_unmap_user_space(void);
        vmm_unmap_user_space();

        // 2. Load ELF baru ke slot 0x4000000
        uint64_t entry = elf_load_file(kfname);

        // 3. Set RIP & RSP untuk IRETQ
        if (entry != 0) {
            r->rip = entry;

            // Reset RSP ke shell's saved stack.
            // g_shell_return_rsp = RSP tepat sebelum shell CALL app.
            // Kita set r->rsp = g_shell_return_rsp, sehingga setelah IRETQ
            // stack kembali ke kondisi "seolah belum pernah CALL".
            // Saat app baru return (ret), RET pops [RSP].
            // [g_shell_return_rsp] berisi apa yang ada di stack sebelum CALL:
            // yaitu shell's own stack frame → local vars, saved rbp, etc.
            // INI MEMANG BUKAN return address, tapi kita handle via sys_exit.
            extern uint64_t g_shell_return_rsp;
            if (g_shell_return_rsp != 0) {
                r->rsp = g_shell_return_rsp;
            }
            ret_val = 1;
        } else {
            ret_val = 0;
        }
    }
    else if (syscall_num == 34) { // sys_exit — app selesai, kembali ke shell
        // Bebaskan halaman user app
        extern void vmm_unmap_user_space(void);
        vmm_unmap_user_space();

        // Longjmp kembali ke shell: reset RSP dan jump ke user_shell()
        // Ini BYPASS iretq sepenuhnya — langsung ke shell command loop.
        // Aman karena int 0x80 = software interrupt (tidak perlu EOI).
        extern void user_shell(void);
        extern uint64_t g_shell_return_rsp;
        uint64_t safe_rsp = g_shell_return_rsp;
        if (safe_rsp == 0) {
            // Fallback: jika belum pernah launch app dari shell, halt
            for(;;) __asm__ volatile("hlt");
        }
        // Reset stack dan jump langsung ke shell loop.
        // Tidak pakai CALL (yang push return addr dan grow stack).
        // Pakai JMP → shell berjalan di stack level yang sama.
        __asm__ volatile(
            "mov %0, %%rsp\n"
            "xor %%rbp, %%rbp\n"
            "sti\n"                // Re-enable interrupts! (int 0x80 disabled mereka)
            "jmp *%1\n"
            : : "r"(safe_rsp), "r"((uint64_t)user_shell)
            : "memory"
        );
        __builtin_unreachable();
    }
    else if (syscall_num == 35) { // sys_get_total_disk
        ret_val = kfs_get_total_space();
    }
    else if (syscall_num == 36) { // sys_get_used_disk
        ret_val = kfs_get_used_space();
    }
    else if (syscall_num == 37) { // sys_get_cpu_usage
        ret_val = get_cpu_usage();
    }
    else if (syscall_num == 38) { // sys_shutdown
        extern void acpi_poweroff(void);
        acpi_poweroff();
    }
    else if (syscall_num == 39) { // sys_reboot
        extern void system_reboot(void);
        system_reboot();
    }
    

    // SIMPAN RETURN VALUE KE RAX (Penting untuk aplikasi Ring 3!)
    r->rax = ret_val;
}