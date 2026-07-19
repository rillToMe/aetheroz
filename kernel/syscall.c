#include "fs.h"
#include <stdint.h>
#include "userlib.h"
#include "task.h"
#include "paging.h"
#include "smp.h"

// registers_t is provided by task.h — must match PUSHA64 in isr_macro.inc

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
extern uint64_t pmm_get_used_ram(void);
extern uint64_t pmm_get_total_ram(void);
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
// Per-address-space cookie generator: increments for each new AS.
static uint32_t as_cookie_counter = 0;
// Current address space cookie (readable by apps via sys_get_pid).
// Updated by sys_load_elf when creating a new AS.
static uint32_t current_as_cookie = 0;

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
        // Flush KEDUA buffer saat shell mulai baca:
        //   - event queue  → sisa event GUI (keystroke notepad dll)
        //   - kbd_buffer   → sisa karakter TTY (yang juga ditulis keyboard ISR)
        extern void flush_event_queue(void);
        extern void flush_kbd_buffer(void);
        flush_event_queue();
        flush_kbd_buffer();
        ret_val = read_fs(&tty_node, 0, r->rcx, (uint8_t*)r->rbx);
    }

    else if (syscall_num == 4) { // sys_yield
        yield_counter++; // Tandai CPU idle untuk CPU usage tracker
        // Preemptive: timer IRQ0 akan switch otomatis saat quantum habis
        // Tidak perlu explicit switch di sini
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
        // Flush KEDUA buffer sebelum app baru jalan
        extern void flush_event_queue(void);
        extern void flush_kbd_buffer(void);
        flush_event_queue();
        flush_kbd_buffer();

        // --- PER-PROCESS ISOLATION ---
        // Create a fresh address space for the new user app.
        // current_pml4 ALWAYS stays as kernel PML4. We route ELF pages
        // into the user PML4 via vmm_user_pml4, then switch CR3.
        extern phys_addr_t vmm_user_pml4;

        phys_addr_t new_pml4 = vmm_create_address_space();
        if (new_pml4 != PHYS_NULL) {
            // Clean old user pages if this task had a previous AS
            task_t *self = NULL;
            for (int i = 0; i < task_count; i++) {
                if (tasks[i].state == TASK_RUNNING) { self = &tasks[i]; break; }
            }
            if (self && self->pml4_phys != 0) {
                vmm_destroy_task_as(self->pml4_phys);
            }

            // Route user-range mappings into the new PML4 during ELF load
            vmm_user_pml4 = new_pml4;

            if (self) {
                self->pml4_phys = new_pml4;
                self->cookie = ++as_cookie_counter;
                current_as_cookie = self->cookie;
            }

            // Switch CR3 to the user PML4 BEFORE loading. elf_load_file copies
            // segment bytes directly to user virtual addresses (e.g. 0x4000000)
            // via memcpy, which the CPU translates through the *current* CR3.
            // The freshly-allocated pages are mapped into new_pml4, so CR3 must
            // already point there or the copy faults on an unmapped address.
            // Kernel higher-half (code/stack/heap/HHDM) is cloned into new_pml4,
            // so kernel execution continues safely after the switch.
            vmm_switch_pml4(new_pml4);
        }

        ret_val = elf_load_file((char*)r->rbx);

        // Done loading ELF — stop routing to user PML4
        vmm_user_pml4 = PHYS_NULL;
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
    else if (syscall_num == 29) { // sys_get_event
        kyuzen_event_t* out_event = (kyuzen_event_t*)r->rbx;

        // 1. Prioritaskan Event Queue asli (Keyboard IRQ + Mouse IRQ via push_event)
        //    Semua keystroke dan klik mouse sudah dimasukkan ke queue oleh ISR.
        extern int pop_event(kyuzen_event_t* out);
        if (pop_event(out_event)) {
            r->rax = 1;
            return; // Event berhasil diambil dari queue — langsung return
        }

        // 2. Fallback: Queue kosong → kirim posisi mouse terkini agar hover GUI tetap responsif
        //    (App pakai EVENT_MOUSE_MOVE untuk update highlight tombol, dll)
        extern int32_t mouse_x, mouse_y;
        extern uint8_t mouse_left_clicked;

        if (mouse_left_clicked) {
            out_event->type   = EVENT_MOUSE_CLICK;
            out_event->param1 = 0;         // 0 = tombol kiri
            out_event->param2 = 1;         // 1 = ditekan
            out_event->param3 = mouse_x;   // koordinat X saat klik
            mouse_left_clicked = 0;
            r->rax = 1;
        } else {
            // Selalu kirim posisi mouse agar app bisa track hover
            out_event->type   = EVENT_MOUSE_MOVE;
            out_event->param1 = mouse_x;
            out_event->param2 = mouse_y;
            out_event->param3 = 0;
            r->rax = 1;
        }
        return;
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

        // 0. Destroy all KWM windows FIRST — prevents compositor from
        //    accessing freed canvas memory after AS cleanup.
        extern void kwm_destroy_all_windows(void);
        kwm_destroy_all_windows();

        // 1. Destroy current address space and switch back to kernel PML4
        {
            task_t *self = NULL;
            for (int i = 0; i < task_count; i++) {
                if (tasks[i].state == TASK_RUNNING) { self = &tasks[i]; break; }
            }

            if (self && self->pml4_phys != 0) {
                vmm_destroy_task_as(self->pml4_phys);
                self->pml4_phys = 0;
            } else {
                vmm_unmap_user_space();
            }
        }

        // 1b. Flush input buffers so new app doesn't inherit old keystrokes
        {
            extern void flush_event_queue(void);
            extern void flush_kbd_buffer(void);
            flush_event_queue();
            flush_kbd_buffer();
        }

        // 2. Create fresh AS for the new app being exec'd
        {
            extern phys_addr_t vmm_user_pml4;
            phys_addr_t new_pml4 = vmm_create_address_space();
            if (new_pml4 != PHYS_NULL) {
                // Route ELF pages into the new PML4
                vmm_user_pml4 = new_pml4;
                task_t *self = NULL;
                for (int i = 0; i < task_count; i++) {
                    if (tasks[i].state == TASK_RUNNING) { self = &tasks[i]; break; }
                }
                if (self) self->pml4_phys = new_pml4;

                // Switch CR3 to the user PML4 BEFORE loading — elf_load_file
                // copies segment bytes to user virtual addresses via memcpy,
                // translated through the current CR3. The target pages live in
                // new_pml4, so CR3 must point there or the copy page-faults.
                vmm_switch_pml4(new_pml4);
            }
        }

        // 3. Load ELF baru ke slot 0x4000000
        uint64_t entry = elf_load_file(kfname);

        // Done loading — stop routing user-range mappings to the new PML4.
        // CR3 already points at the user PML4 (switched before load).
        {
            extern phys_addr_t vmm_user_pml4;
            vmm_user_pml4 = PHYS_NULL;
        }


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
        // Destroy all KWM windows FIRST — prevents dangling canvas pointers
        extern void kwm_destroy_all_windows(void);
        kwm_destroy_all_windows();

        // Destroy address space and switch back to kernel PML4
        {
            task_t *self = NULL;
            for (int i = 0; i < task_count; i++) {
                if (tasks[i].state == TASK_RUNNING) { self = &tasks[i]; break; }
            }

            if (self && self->pml4_phys != 0) {
                vmm_destroy_task_as(self->pml4_phys);
                self->pml4_phys = 0;
            } else {
                vmm_unmap_user_space();
            }
        }

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
    else if (syscall_num == 40) { // sys_get_window_pos
        // rbx = win_id, rcx = int32_t* out_x, rdx = int32_t* out_y
        extern void kwm_get_window_pos(int, int32_t*, int32_t*);
        kwm_get_window_pos((int)r->rbx, (int32_t*)r->rcx, (int32_t*)r->rdx);
    }
    else if (syscall_num == 41) { // sys_ping
        // RBX = const char* host (user-space pointer ke string hostname/IP)
        // Return: RTT dalam ms (>=0) jika berhasil, -1 jika timeout/error
        extern int kernel_ping(const char *host);
        const char *host = (const char *)r->rbx;
        int rtt = kernel_ping(host);
        ret_val = (uint64_t)(int64_t)rtt; // sign-extend -1 dengan benar
    }
    else if (syscall_num == 42) { // sys_get_cr3 — return current CR3 physical address
        // Diagnostic syscall for process isolation testing.
        // Returns the physical address of the current PML4 (CR3 value).
        ret_val = (uint64_t)vmm_read_cr3();
    }
    else if (syscall_num == 43) { // sys_get_task_id — return current task ID
        ret_val = (uint64_t)(int64_t)smp_current_task_id();
    }
    else if (syscall_num == 44) { // sys_is_mapped — check if vaddr is mapped
        // Returns 1 if the page containing vaddr is present in current PML4.
        // Safe: does NOT dereference the address, only walks page tables.
        ret_val = (uint64_t)paging_is_mapped((uint64_t)r->rbx);
    }
    else if (syscall_num == 45) { // sys_get_pid — return per-AS cookie
        // Returns the unique cookie of the current address space.
        ret_val = (uint64_t)current_as_cookie;
    }

    // SIMPAN RETURN VALUE KE RAX (Penting untuk aplikasi Ring 3!)
    r->rax = ret_val;
}
