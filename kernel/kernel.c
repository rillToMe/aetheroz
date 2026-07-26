#include <stdint.h>
#include <stddef.h>
#include "timer.h" 
#include "limine.h"
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
#include "vfs.h"
#include "task.h"
#include "timer.h"
#include "shell.h"
#include "lapic.h"
#include "smp.h"
#include "spinlock.h"
#include "display.h"

#ifdef STRESS_TEST
#include "pmm_stress.h"
#include "pmm_valid.h"
#endif

#ifdef CONC_TEST
#include "conc_test.h"
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
    .revision = 0\
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
extern void gdt_load(void);
extern void idt_load(void);
extern void pic_remap();
extern void init_keyboard();
extern void switch_to_user_mode(void (*user_func)());
extern void user_login();
extern void init_mouse();
extern void kfs_delete_file(char* filename);
extern void kprint(const char* str);
void kprint_num(uint64_t num);
// terminal_putchar tidak lagi dibutuhkan langsung (kprint ada di kyuzenfs.c)

// ============================================================
// SMP BRING-UP AWAL
//
// Tahap awal: AP/core lain dibuat online, masuk idle loop scheduler, dan
// menerima LAPIC timer/IPI reschedule. Subsystem besar masih belum semua
// SMP-safe, jadi task 0 tetap ditahan di BSP.
// ============================================================

#define SMP_AP_STACK_SIZE   16384

typedef struct {
    uint64_t stack_top;      // offset 0, dibaca oleh arch/x86/smp_ap_entry.asm
    uint32_t cpu_index;
    uint32_t processor_id;
    uint32_t lapic_id;
    volatile uint32_t online;
    uint8_t reserved[4];
    __attribute__((aligned(16))) uint8_t stack[SMP_AP_STACK_SIZE];
} smp_cpu_state_t;

static smp_cpu_state_t smp_cpu_states[SMP_MAX_CPUS];
static percpu_t smp_percpu[SMP_MAX_CPUS];
static volatile uint32_t smp_cpu_online_count = 1; // BSP sudah online
static volatile uint32_t smp_log_lock = 0;

extern void smp_ap_entry(struct limine_mp_info *cpu);

static void smp_spin_lock(volatile uint32_t *lock) {
    for (;;) {
        uint32_t taken = 1;
        __asm__ volatile(
            "lock xchg %0, %1"
            : "+r"(taken), "+m"(*lock)
            :
            : "memory"
        );
        if (taken == 0) return;
        while (*lock) {
            __asm__ volatile("pause");
        }
    }
}

static void smp_spin_unlock(volatile uint32_t *lock) {
    __asm__ volatile("" ::: "memory");
    *lock = 0;
}

static uint32_t smp_atomic_inc(volatile uint32_t *value) {
    uint32_t old = 1;
    __asm__ volatile(
        "lock xadd %0, %1"
        : "+r"(old), "+m"(*value)
        :
        : "memory"
    );
    return old + 1;
}

void smp_register_cpu(uint32_t cpu_id, uint32_t processor_id, uint32_t lapic_id, uint32_t online) {
    if (cpu_id >= SMP_MAX_CPUS) return;

    smp_percpu[cpu_id].processor_id = processor_id;
    smp_percpu[cpu_id].lapic_id = lapic_id;
    smp_percpu[cpu_id].online = online;
    smp_percpu[cpu_id].reschedule_pending = 0;
    smp_percpu[cpu_id].idle_ticks = 0;
    smp_percpu[cpu_id].scheduler_ticks = 0;
    smp_percpu[cpu_id].current_cr3 = 0;  // Will be set when CPU comes online
}

void smp_set_cpu_online(uint32_t cpu_id) {
    if (cpu_id >= SMP_MAX_CPUS) return;

    if (smp_percpu[cpu_id].online == 0) {
        smp_percpu[cpu_id].online = 1;
        smp_atomic_inc(&smp_cpu_online_count);
    }
}

percpu_t* smp_get_cpu(uint32_t cpu_id) {
    if (cpu_id >= SMP_MAX_CPUS) return NULL;
    return &smp_percpu[cpu_id];
}

percpu_t* smp_current_cpu(void) {
    return smp_get_cpu(smp_current_cpu_index());
}

void smp_note_idle_tick(uint32_t cpu_id) {
    if (cpu_id >= SMP_MAX_CPUS) return;
    __asm__ volatile("lock incq %0" : "+m"(smp_percpu[cpu_id].idle_ticks) :: "memory");
}

void smp_note_scheduler_tick(uint32_t cpu_id) {
    if (cpu_id >= SMP_MAX_CPUS) return;
    __asm__ volatile("lock incq %0" : "+m"(smp_percpu[cpu_id].scheduler_ticks) :: "memory");
}

void smp_mark_reschedule(uint32_t cpu_id) {
    if (cpu_id >= SMP_MAX_CPUS) return;
    smp_percpu[cpu_id].reschedule_pending = 1;
}

void smp_clear_reschedule(uint32_t cpu_id) {
    if (cpu_id >= SMP_MAX_CPUS) return;
    smp_percpu[cpu_id].reschedule_pending = 0;
}

int smp_reschedule_pending(uint32_t cpu_id) {
    if (cpu_id >= SMP_MAX_CPUS) return 0;
    return smp_percpu[cpu_id].reschedule_pending != 0;
}

uint32_t smp_current_cpu_index(void) {
    uint32_t current_lapic = lapic_id();

    for (uint32_t i = 0; i < SMP_MAX_CPUS; i++) {
        if (smp_percpu[i].online && smp_percpu[i].lapic_id == current_lapic) {
            return i;
        }
    }

    return 0;
}

uint32_t smp_online_cpu_count(void) {
    return smp_cpu_online_count;
}

void smp_ap_main(struct limine_mp_info *cpu, smp_cpu_state_t *state) {
    __asm__ volatile("cli");

    gdt_load();
    idt_load();
    lapic_init_ap();

    // Record this AP's current CR3 (inherited from BSP via Limine)
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    smp_percpu[state->cpu_index].current_cr3 = cr3 & 0xFFFFFFFFFFFFF000ULL;

    state->online = 1;
    smp_set_cpu_online(state->cpu_index);

#ifdef HEAP_WATCH_DEBUG
    // DR register bersifat per-CPU: setiap AP harus memasang watchpoint-nya
    // sendiri, kalau tidak writer dari AP ini tak pernah ketangkap.
    extern void heap_watch_set(uint64_t addr, int len_bytes);
    extern uint64_t heap_watch_target_addr;
    heap_watch_set(heap_watch_target_addr, 4);
#endif

    smp_spin_lock(&smp_log_lock);
    kprint("[smp] CPU #");
    kprint_num(cpu->processor_id);
    kprint(" online (lapic=");
    kprint_num(cpu->lapic_id);
    kprint(")\n");
    smp_spin_unlock(&smp_log_lock);

    // FIX_001: tinggalkan stack awal dari Limine — idle loop berjalan di
    // idle stack permanen per-CPU (dialokasikan di tasking_init).
    task_switch_to_idle_stack(task_idle_stack_top(state->cpu_index));
}

static void smp_init(void) {
    smp_register_cpu(0, 0, lapic_id(), 1);

    struct limine_mp_response *mp = mp_request.response;
    if (mp == NULL || mp->cpu_count == 0 || mp->cpus == NULL) {
        kprint("[smp] MP response unavailable; running single-core\n");
        return;
    }

    kprint("[smp] BSP lapic=");
    kprint_num(mp->bsp_lapic_id);
    kprint(", CPUs reported=");
    kprint_num(mp->cpu_count);
    kprint("\n");

    uint32_t ap_slot = 0;
    for (uint64_t i = 0; i < mp->cpu_count && ap_slot + 1 < SMP_MAX_CPUS; i++) {
        struct limine_mp_info *cpu = mp->cpus[i];
        if (cpu == NULL) continue;

        if (cpu->lapic_id == mp->bsp_lapic_id) {
            smp_cpu_states[0].cpu_index = 0;
            smp_cpu_states[0].processor_id = cpu->processor_id;
            smp_cpu_states[0].lapic_id = cpu->lapic_id;
            smp_cpu_states[0].online = 1;
            smp_register_cpu(0, cpu->processor_id, cpu->lapic_id, 1);
            continue;
        }

        ap_slot++;
        smp_cpu_state_t *state = &smp_cpu_states[ap_slot];
        state->cpu_index = ap_slot;
        state->processor_id = cpu->processor_id;
        state->lapic_id = cpu->lapic_id;
        state->online = 0;
        state->stack_top = ((uint64_t)&state->stack[SMP_AP_STACK_SIZE]) & ~0xFULL;
        smp_register_cpu(ap_slot, cpu->processor_id, cpu->lapic_id, 0);

        cpu->extra_argument = (uint64_t)state;
        __asm__ volatile("" ::: "memory");
        cpu->goto_address = smp_ap_entry;
    }

    uint64_t wait_ticks = 0;
    while (smp_cpu_online_count < (uint32_t)(ap_slot + 1) && wait_ticks < 10000000ULL) {
        wait_ticks++;
        __asm__ volatile("pause");
    }

    kprint("[smp] Online CPUs: ");
    kprint_num(smp_cpu_online_count);
    kprint("/");
    kprint_num(ap_slot + 1);
    kprint("\n");
}

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

void screen_mark_dirty(int32_t x, int32_t y, uint32_t width, uint32_t height);

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
static spinlock_t kwm_lock = SPINLOCK_INIT;

int kwm_create_window(int x, int y, uint32_t width, uint32_t height) {
    uint64_t flags = spinlock_lock_irqsave(&kwm_lock);
    for(int i = 0; i < MAX_WINDOWS; i++) {
        if(!kwm_windows[i].active) {
            kwm_windows[i].active = 1;
            kwm_windows[i].x = x;
            kwm_windows[i].y = y;
            kwm_windows[i].width = width;
            kwm_windows[i].height = height;
            kwm_windows[i].canvas = (uint32_t*)kmalloc(width * height * 4);
            kwm_windows[i].z_index = next_z_index++;
            spinlock_unlock_irqrestore(&kwm_lock, flags);
            screen_mark_dirty(x, y, width, height);
            return i;
        }
    }
    spinlock_unlock_irqrestore(&kwm_lock, flags);
    return -1; 
}

void kwm_update_window(int win_id, uint32_t* app_buffer) {
    if(win_id < 0 || win_id >= MAX_WINDOWS) return;
    uint64_t flags = spinlock_lock_irqsave(&kwm_lock);
    if(!kwm_windows[win_id].active || !kwm_windows[win_id].canvas || !app_buffer) {
        spinlock_unlock_irqrestore(&kwm_lock, flags);
        return;
    }

    uint32_t size = kwm_windows[win_id].width * kwm_windows[win_id].height;
    uint32_t* dest = kwm_windows[win_id].canvas;
    __asm__ volatile ("rep movsl" : "+D" (dest), "+S" (app_buffer), "+c" (size) : : "memory");
    int32_t mx = kwm_windows[win_id].x, my = kwm_windows[win_id].y;
    uint32_t mw = kwm_windows[win_id].width, mh = kwm_windows[win_id].height;
    spinlock_unlock_irqrestore(&kwm_lock, flags);
    screen_mark_dirty(mx, my, mw, mh);
}

void kwm_destroy_window(int win_id) {
    if(win_id < 0 || win_id >= MAX_WINDOWS) return;
    uint64_t flags = spinlock_lock_irqsave(&kwm_lock);
    if(!kwm_windows[win_id].active) {
        spinlock_unlock_irqrestore(&kwm_lock, flags);
        return;
    }
    int32_t mx = kwm_windows[win_id].x, my = kwm_windows[win_id].y;
    uint32_t mw = kwm_windows[win_id].width, mh = kwm_windows[win_id].height;
    if (kwm_windows[win_id].canvas) kfree(kwm_windows[win_id].canvas);
    kwm_windows[win_id].active = 0;
    spinlock_unlock_irqrestore(&kwm_lock, flags);
    screen_mark_dirty(mx, my, mw, mh);
}

// Destroy ALL KWM windows — called on app exit to prevent dangling canvas pointers.
// Without this, the compositor would read freed memory when rendering.
void kwm_destroy_all_windows(void) {
    uint64_t flags = spinlock_lock_irqsave(&kwm_lock);
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (kwm_windows[i].active) {
            if (kwm_windows[i].canvas) kfree(kwm_windows[i].canvas);
            kwm_windows[i].active = 0;
        }
    }
    spinlock_unlock_irqrestore(&kwm_lock, flags);
    screen_mark_dirty(0, 0, fb_width, fb_height);
}

// Kembalikan posisi window terkini (setelah drag, dsb) ke app via pointer.
// App harus panggil ini setiap kali ingin konversi koordinat layar → koordinat lokal window.
void kwm_get_window_pos(int win_id, int32_t* out_x, int32_t* out_y) {
    uint64_t flags = spinlock_lock_irqsave(&kwm_lock);
    if (win_id < 0 || win_id >= MAX_WINDOWS || !kwm_windows[win_id].active) {
        if (out_x) *out_x = 0;
        if (out_y) *out_y = 0;
        spinlock_unlock_irqrestore(&kwm_lock, flags);
        return;
    }
    if (out_x) *out_x = kwm_windows[win_id].x;
    if (out_y) *out_y = kwm_windows[win_id].y;
    spinlock_unlock_irqrestore(&kwm_lock, flags);
}

// ============================================================
// KWM V2 — Drag & Drop + Z-Index Dinamis
// ============================================================

// State global untuk drag session yang sedang aktif
static int     dragged_win_id = -1;  // -1 = tidak ada drag
static int32_t drag_offset_x  = 0;   // Offset klik dalam window (mencegah window "loncat")
static int32_t drag_offset_y  = 0;

// Bawa window ke depan (Z-index tertinggi)
// Dipanggil saat user klik pada window manapun.
void kwm_bring_to_front(int win_id) {
    if (win_id < 0 || win_id >= MAX_WINDOWS) return;
    // Caller MUST hold kwm_lock
    kwm_windows[win_id].z_index = next_z_index++;
}

// Intercept mouse event sebelum dikirim ke user-space.
//
// Dipanggil dari mouse_handler() SEBELUM push_event().
// Return: 1 = event "dimakan" oleh KWM (jangan kirim ke app)
//         0 = teruskan event ke app seperti biasa
//
// Params:
//   mouse_px, mouse_py  = posisi kursor saat ini
//   left_down = 1 saat tombol kiri baru ditekan (edge detect)
//   left_up   = 1 saat tombol kiri baru dilepas (edge detect)
int kwm_process_mouse(int32_t mouse_px, int32_t mouse_py,
                      uint8_t left_down, uint8_t left_up) {

    spinlock_lock(&kwm_lock);

    // 1. Mouse Up — akhiri drag session
    if (left_up) {
        dragged_win_id = -1;
        spinlock_unlock(&kwm_lock);
        return 0; // Kirim event "release" ke app juga
    }

    // 2. Sedang dalam Drag — update posisi window mengikuti kursor
    if (dragged_win_id != -1) {
        int32_t new_x = mouse_px - drag_offset_x;
        int32_t new_y = mouse_py - drag_offset_y;

        // Clamp: pastikan window tidak keluar layar
        if (new_x < 0) new_x = 0;
        if (new_y < 0) new_y = 0;
        if (new_x + (int32_t)kwm_windows[dragged_win_id].width  > (int32_t)fb_width)
            new_x = (int32_t)fb_width  - (int32_t)kwm_windows[dragged_win_id].width;
        if (new_y + (int32_t)kwm_windows[dragged_win_id].height > (int32_t)fb_height)
            new_y = (int32_t)fb_height - (int32_t)kwm_windows[dragged_win_id].height;

        int32_t old_x = kwm_windows[dragged_win_id].x;
        int32_t old_y = kwm_windows[dragged_win_id].y;
        uint32_t dw = kwm_windows[dragged_win_id].width;
        uint32_t dh = kwm_windows[dragged_win_id].height;
        kwm_windows[dragged_win_id].x = new_x;
        kwm_windows[dragged_win_id].y = new_y;
        spinlock_unlock(&kwm_lock);
        screen_mark_dirty(old_x, old_y, dw, dh);
        screen_mark_dirty(new_x, new_y, dw, dh);
        return 1; // Konsumsi event — jangan sampai app salah deteksi klik
    }

    // 3. Mouse Down — hit-test, Z-bring-to-front, cek title bar drag
    if (left_down) {
        int highest_z  = -1;
        int target_win = -1;

        for (int i = 0; i < MAX_WINDOWS; i++) {
            if (!kwm_windows[i].active) continue;
            int32_t wx  = kwm_windows[i].x;
            int32_t wy  = kwm_windows[i].y;
            int32_t ww  = (int32_t)kwm_windows[i].width;
            int32_t wh  = (int32_t)kwm_windows[i].height;

            if (mouse_px >= wx && mouse_px < wx + ww &&
                mouse_py >= wy && mouse_py < wy + wh) {
                if ((int)kwm_windows[i].z_index > highest_z) {
                    highest_z  = (int)kwm_windows[i].z_index;
                    target_win = i;
                }
            }
        }

        if (target_win != -1) {
            kwm_bring_to_front(target_win);

            int32_t tx = kwm_windows[target_win].x, ty = kwm_windows[target_win].y;
            uint32_t tw = kwm_windows[target_win].width, th = kwm_windows[target_win].height;

            int32_t close_btn_x = kwm_windows[target_win].x
                                  + (int32_t)kwm_windows[target_win].width - 40;

            if (mouse_py >= kwm_windows[target_win].y &&
                mouse_py <  kwm_windows[target_win].y + 24 &&
                mouse_px <  close_btn_x) {
                dragged_win_id = target_win;
                drag_offset_x  = mouse_px - kwm_windows[target_win].x;
                drag_offset_y  = mouse_py - kwm_windows[target_win].y;
                spinlock_unlock(&kwm_lock);
                screen_mark_dirty(tx, ty, tw, th);
                return 1;
            }
            spinlock_unlock(&kwm_lock);
            screen_mark_dirty(tx, ty, tw, th);
            return 0;
        }
    }

    spinlock_unlock(&kwm_lock);
    return 0; // Klik di area kosong — teruskan
}

extern int32_t mouse_x;
extern int32_t mouse_y;
extern const uint8_t cursor_bitmap[16][12];

#define CURSOR_WIDTH  12
#define CURSOR_HEIGHT 16

// Dirty-region state (Phase 3B). Every draw into base_canvas and every window
// move marks the touched rect here; compositor_flush recomposites and presents
// only these rects instead of the whole screen. Idle frames are near no-ops.
static DirtyRegionList g_screen_dirty;
static spinlock_t g_dirty_lock = SPINLOCK_INIT;
static int32_t g_last_cursor_x = -1;
static int32_t g_last_cursor_y = -1;

void screen_mark_dirty(int32_t x, int32_t y, uint32_t width, uint32_t height) {
    Rect r = { x, y, width, height };
    uint64_t flags = spinlock_lock_irqsave(&g_dirty_lock);
    dirty_region_mark(&g_screen_dirty, r);
    spinlock_unlock_irqrestore(&g_dirty_lock, flags);
}

// Copy a screen-space rect between two full-screen buffers, row by row.
static void blit_rect(uint32_t* dst, const uint32_t* src, Rect r, int pitch4) {
    for (uint32_t row = 0; row < r.height; row++) {
        uint32_t off = ((uint32_t)r.y + row) * (uint32_t)pitch4 + (uint32_t)r.x;
        uint32_t* d = dst + off;
        const uint32_t* s = src + off;
        for (uint32_t col = 0; col < r.width; col++) d[col] = s[col];
    }
}

// Composite every window overlapping `r` (z-order low→high) onto the backbuffer.
// Caller must hold kwm_lock.
static void composite_windows_in_rect(Rect r, int pitch4) {
    for (uint32_t z = 1; z <= next_z_index; z++) {
        for (int w = 0; w < MAX_WINDOWS; w++) {
            if (!(kwm_windows[w].active && kwm_windows[w].z_index == z && kwm_windows[w].canvas))
                continue;

            Rect wrect = { kwm_windows[w].x, kwm_windows[w].y,
                           kwm_windows[w].width, kwm_windows[w].height };
            Rect clip;
            if (!rect_intersect(wrect, r, &clip)) continue;

            const int32_t win_x = kwm_windows[w].x;
            const int32_t win_y = kwm_windows[w].y;
            const int win_w = (int)kwm_windows[w].width;
            uint32_t* canvas = kwm_windows[w].canvas;

            for (uint32_t yy = 0; yy < clip.height; yy++) {
                int32_t sy = clip.y + (int32_t)yy - win_y;
                const uint32_t* src = canvas + (uint32_t)sy * (uint32_t)win_w + (uint32_t)(clip.x - win_x);
                uint32_t* dst = backbuffer + ((uint32_t)clip.y + yy) * (uint32_t)pitch4 + (uint32_t)clip.x;
                for (uint32_t xx = 0; xx < clip.width; xx++) {
                    uint32_t pixel = src[xx];
                    // Alpha byte acts as a per-pixel mask: 0 = transparent.
                    if (pixel >> 24) dst[xx] = pixel & 0xFFFFFF;
                }
            }
        }
    }
}

void compositor_flush() {
    if (fb_width == 0) return;
    const int pitch4 = (int)(fb_pitch / 4);
    Rect screen = { 0, 0, fb_width, fb_height };

    uint64_t flags = spinlock_lock_irqsave(&g_dirty_lock);
    DirtyRegionList dirty = g_screen_dirty;
    dirty_region_clear(&g_screen_dirty);
    spinlock_unlock_irqrestore(&g_dirty_lock, flags);

    // Cursor moves every frame it's dragged; both the vacated and the new cell
    // must repaint, so fold them into the dirty set.
    int32_t cx = mouse_x, cy = mouse_y;
    if (g_last_cursor_x >= 0) {
        Rect old = { g_last_cursor_x, g_last_cursor_y, CURSOR_WIDTH, CURSOR_HEIGHT };
        dirty_region_mark(&dirty, old);
    }
    Rect cur = { cx, cy, CURSOR_WIDTH, CURSOR_HEIGHT };
    dirty_region_mark(&dirty, cur);

    if (dirty.count == 0) return;

    uint64_t kwm_flags = spinlock_lock_irqsave(&kwm_lock);
    for (uint32_t i = 0; i < dirty.count; i++) {
        Rect r;
        if (!rect_intersect(dirty.regions[i], screen, &r)) continue;
        blit_rect(backbuffer, base_canvas, r, pitch4);
        composite_windows_in_rect(r, pitch4);
    }
    spinlock_unlock_irqrestore(&kwm_lock, kwm_flags);

    for (int y = 0; y < CURSOR_HEIGHT; y++) {
        for (int x = 0; x < CURSOR_WIDTH; x++) {
            if (cy + y >= (int32_t)fb_height || cx + x >= (int32_t)fb_width) continue;
            uint32_t offset = ((cy + y) * pitch4) + (cx + x);
            if (cursor_bitmap[y][x] == 1) backbuffer[offset] = 0xFFFFFF;
            else if (cursor_bitmap[y][x] == 2) backbuffer[offset] = 0x000000;
        }
    }
    g_last_cursor_x = cx;
    g_last_cursor_y = cy;

    for (uint32_t i = 0; i < dirty.count; i++) {
        Rect r;
        if (!rect_intersect(dirty.regions[i], screen, &r)) continue;
        blit_rect(fb_ptr, backbuffer, r, pitch4);
    }
}

void draw_rect(uint32_t start_x, uint32_t start_y, uint32_t width, uint32_t height, uint32_t color) {
    for (uint32_t y = start_y; y < start_y + height; y++) {
        for (uint32_t x = start_x; x < start_x + width; x++) {
            draw_pixel(x, y, color);
        }
    }
    screen_mark_dirty((int32_t)start_x, (int32_t)start_y, width, height);
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
    screen_mark_dirty(start_x, start_y, (uint32_t)width, (uint32_t)height);
}

void draw_char(char c, uint32_t x, uint32_t y, uint32_t color) {
    if (c < 0 || c > 127) return;
    const unsigned char* bitmap = font8x16[(int)c];
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            if (bitmap[row] & (0x80 >> col)) draw_pixel(x + col, y + row, color);
        }
    }
    screen_mark_dirty((int32_t)x, (int32_t)y, 8, 16);
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
    lapic_init_bsp();

    extern void pci_probe(void);
    pci_probe();
    
    init_timer(TIMER_HZ);      // Inisialisasi PIT pada frekuensi dari timer.h
    timer_callbacks_init();    // Daftarkan subscriber default (visual, cursor, flush)
    tasking_init();            // Daftarkan kmain sebagai task awal scheduler

    init_mouse();
    init_keyboard();
    init_tty();
    kfs_init();
#ifdef HEAP_WATCH_DEBUG
    extern void serial_init(void);
    extern void serial_print(const char* s);
    serial_init();
    serial_print("\n[HEAP_WATCH_DEBUG] serial ready\n");
    // Pasang hardware watchpoint DR0 (per-CPU!) di BSP SEBELUM AP online.
    // Setiap AP memasang DR0-nya sendiri di smp_ap_main().
    extern void heap_watch_set(uint64_t addr, int len_bytes);
    extern uint64_t heap_watch_target_addr;
    heap_watch_set(heap_watch_target_addr, 4);
    serial_print("[HEAP_WATCH] BSP DR0 watch magic @ target (4 bytes)\n");
#endif
    vfs_init();
    smp_init();

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
