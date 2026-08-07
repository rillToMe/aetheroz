#include <stdint.h>
#include <stddef.h>
#include "timer.h"
#include "limine.h"
#include "fs.h"
#include "tty.h"
#include "pmm.h"
#include "paging.h"
#include "heap.h"
#include "string.h"
#include "ata.h"
#include "kyuzenfs.h"
#include "vfs.h"
#include "task.h"
#include "shell.h"
#include "lapic.h"
#include "smp.h"
#include "gfx.h"
#include "ghal.h"   // tipe ghal_* (dipakai GFX_SELFTEST)

#ifdef STRESS_TEST
#include "pmm_stress.h"
#include "pmm_valid.h"
#endif

#ifdef CONC_TEST
#include "conc_test.h"
#endif

#ifdef HEAP_STRESS_TEST
#include "heap_stress_test.h"
#endif

// LIMINE REQUESTS — Harus di section .requests agar bootloader bisa scan
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

// MP/SMP: minta Limine menyiapkan semua CPU dan biarkan AP menunggu entry point.
__attribute__((used, section(".requests")))
static volatile struct limine_mp_request mp_request = {
    .id = LIMINE_MP_REQUEST_ID,
    .revision = 0,
    .flags = 0
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

// kprint didefinisikan di kernel/kyuzenfs.c (via write_fs & tty_node)
extern void kprint(const char* str);

void kprint_num(uint64_t num) {
    if (num == 0) { kprint("0"); return; }
    char buf[20]; int i = 18; buf[19] = '\0';
    while (num > 0) { buf[i--] = (num % 10) + '0'; num /= 10; }
    kprint(&buf[i + 1]);
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

    // FIX_005 Tahap 4: SMEP/SMAP di BSP + pastikan CR0.WP. AP mengaktifkan
    // miliknya sendiri di smp_ap_main (CR4/CR0 per-core).
    {
        extern void cpu_enable_smap_smep(void);
        extern int  cpu_verify_wp(void);
        extern int  g_smap_enabled, g_smep_enabled;
        cpu_enable_smap_smep();
        int wp = cpu_verify_wp();
        kprint("[CPU] SMEP=");  kprint(g_smep_enabled ? "on" : "off");
        kprint(" SMAP=");       kprint(g_smap_enabled ? "on" : "off");
        kprint(" WP=");         kprint(wp ? "on" : "off");
        kprint("\n");
    }

    pic_remap();
    lapic_init_bsp();

    extern void pci_probe(void);
    pci_probe();

    init_timer(TIMER_HZ);      // Inisialisasi PIT pada frekuensi dari timer.h

    // Phase 2B: inisialisasi Graphics HAL SEBELUM timer_callbacks_init, supaya
    // compositor_flush (cb_flush) langsung punya backend + main surface.
    // Software backend butuh tahu framebuffer hardware untuk present.
    // compositor_ghal_init() membuat main surface DI SINI (konteks task),
    // bukan lazy di dalam IRQ — surface_create memakai kmalloc + virtqueue.
    {
        extern void ghal_set_framebuffer(uint32_t*, uint32_t, uint32_t, uint32_t);
        extern int  ghal_init(void);
        extern const char* ghal_active_backend_name(void);
        extern void compositor_ghal_init(void);
        ghal_set_framebuffer(fb_ptr, fb_width, fb_height, fb_pitch);
        if (ghal_init() == 0) {
            kprint("[GHAL] backend="); kprint(ghal_active_backend_name()); kprint("\n");
            compositor_ghal_init();
        } else {
            kprint("[GHAL] init failed — compositor fallback ke jalur langsung\n");
        }
    }

    timer_callbacks_init();    // Daftarkan subscriber default (visual, cursor, flush)
    tasking_init();            // Daftarkan kmain sebagai task awal scheduler

    init_mouse();
    init_keyboard();
    init_tty();
    kfs_init();
    // Serial COM1 selalu diinit — panic dump (panic.c) dan watchdog memakainya,
    // bukan hanya mode HEAP_WATCH. Aman dipanggil kapan pun.
    extern void serial_init(void);
    extern void serial_print(const char* s);
    serial_init();
    serial_print("\n[SERIAL] ready\n");
#ifdef HEAP_WATCH_DEBUG
    // Pasang hardware watchpoint DR0 (per-CPU!) di BSP SEBELUM AP online.
    // Setiap AP memasang DR0-nya sendiri di smp_ap_main().
    extern void heap_watch_set(uint64_t addr, int len_bytes);
    extern uint64_t heap_watch_target_addr;
    heap_watch_set(heap_watch_target_addr, 4);
    serial_print("[HEAP_WATCH] BSP DR0 watch magic @ target (4 bytes)\n");
#endif
    vfs_init();
    smp_init(mp_request.response);

    // 3. AUTO-INSTALL MODUL DARI LIMINE
    kprint("\n--- RADAR AUTO-INSTALL ---\n");
    if (module_request.response != NULL && module_request.response->module_count > 0) {
        kprint("Status: Limine mengirim modul!\n");
        kprint("Jumlah Modul: "); kprint_num(module_request.response->module_count); kprint("\n");

        // Fase 3: pastikan folder /apps ada — app .elf/.app tinggal di sini.
        if (!kfs_exists("/apps")) {
            kprint(" -> Membuat folder /apps...\n");
            kfs_create_folder("/apps");
        }

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

            // Fase 3: .elf / .app → /apps/, yang lain (png, dll) tetap root.
            int nlen = 0; while (clean_name[nlen]) nlen++;
            int is_app = nlen > 4 &&
                         ((clean_name[nlen-3] == 'e' && clean_name[nlen-2] == 'l' && clean_name[nlen-1] == 'f') ||
                          (clean_name[nlen-3] == 'a' && clean_name[nlen-2] == 'p' && clean_name[nlen-1] == 'p'));
            char dest[32];
            int d = 0;
            if (is_app) { const char* ap = "/apps/"; for (int j = 0; ap[j] && d < 31; j++) dest[d++] = ap[j]; }
            for (int j = 0; clean_name[j] && d < 31; j++) dest[d++] = clean_name[j];
            dest[d] = '\0';

            if (kfs_exists(dest)) {
                kfs_delete_file(dest);
            }

            int res = kfs_create_file(dest, (char*)mod->address, size);
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

#ifdef GFX_SELFTEST
    // Phase 2A/2B: laporkan backend aktif + ukuran scanout ke serial.
    // ghal_init() sudah dipanggil di atas (sebelum timer_callbacks_init).
    {
        extern const char* ghal_active_backend_name(void);
        extern void ghal_scanout_size(uint32_t*, uint32_t*);
        extern void serial_print(const char* s);
        uint32_t sw = 0, sh = 0;
        ghal_scanout_size(&sw, &sh);
        serial_print("[GFX SELFTEST] backend=");
        serial_print(ghal_active_backend_name());
        serial_print(" scanout=");
        {
            char tmp[8]; uint32_t v = sw; int idx = 0;
            if (v == 0) tmp[idx++] = '0';
            else { char r[8]; int n = 0; while (v) { r[n++] = (char)('0' + v % 10); v /= 10; } while (n) tmp[idx++] = r[--n]; }
            tmp[idx] = '\0'; serial_print(tmp); serial_print("x");
            v = sh; idx = 0;
            if (v == 0) tmp[idx++] = '0';
            else { char r[8]; int n = 0; while (v) { r[n++] = (char)('0' + v % 10); v /= 10; } while (n) tmp[idx++] = r[--n]; }
            tmp[idx] = '\0'; serial_print(tmp);
        }
        serial_print("\n");
    }
#endif

    // Inisialisasi network stack (lwIP + e1000 + DHCP).
    // WAJIB setelah sti karena DHCP wait loop menggunakan hlt
    // dan membutuhkan PIT IRQ untuk drive timer callbacks.
    extern void net_init(void);
    net_init();

    extern void ksock_init(void);
    ksock_init();

#ifdef STRESS_TEST
    valid_start();
    stress_start();
    while (1) __asm__ volatile("hlt");
#endif

#ifdef CONC_TEST
    conc_test_run();
    while (1) __asm__ volatile("hlt");
#endif

#ifdef HEAP_STRESS_TEST
    test_heap_stress_run_all();
    while (1) __asm__ volatile("hlt");
#endif

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
