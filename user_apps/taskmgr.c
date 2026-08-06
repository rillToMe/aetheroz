// ============================================================
// taskmgr.c — Task Manager (Phase 10): monitor RAM/CPU/Disk (libui).
// Bar progress + label, refresh 2×/dtk via ui_window_set_tick
// (render hanya saat ≥500ms berlalu). Tanpa per-task table:
// kernel tak mengekspos daftar task + usage per-task (ponytail:
// tambah syscall daftar task bila fase berikutnya butuh).
//
// Build: taskmgr.o + userlib.o + libgui.o + libui.o + png.o
// ============================================================
#include "userlib.h"
#include "libui.h"

static ui_widget_t* g_ram;   static ui_widget_t* g_rambar;
static ui_widget_t* g_cpu;   static ui_widget_t* g_cpubar;
static ui_widget_t* g_disk;  static ui_widget_t* g_diskbar;
static ui_widget_t* g_up;
static uint64_t last_refresh = 0;

static void itoa(uint32_t n, char* b) {
    if (n == 0) { b[0] = '0'; b[1] = '\0'; return; }
    char t[16]; int i = 0;
    while (n > 0 && i < 15) { t[i++] = '0' + (n % 10); n /= 10; }
    int j = 0; while (i > 0) b[j++] = t[--i]; b[j] = '\0';
}

// "RAM : 42 MB / 1024 MB"
static void label_ratio(ui_widget_t* lbl, const char* pre, uint32_t used,
                        uint32_t tot, const char* suf) {
    char b[48]; int k = 0;
    for (int i = 0; pre[i] && k < 20; i++) b[k++] = pre[i];
    char n[16]; itoa(used, n);
    for (int i = 0; n[i] && k < 30; i++) b[k++] = n[i];
    const char* sep = " MB / ";
    for (int i = 0; sep[i] && k < 38; i++) b[k++] = sep[i];
    itoa(tot, n);
    for (int i = 0; n[i] && k < 44; i++) b[k++] = n[i];
    for (int i = 0; suf[i] && k < 47; i++) b[k++] = suf[i];
    b[k] = '\0';
    ui_label_set_text(lbl, b);
}

static int tick(void* userdata) {
    (void)userdata;
    uint64_t now = sys_uptime();
    if (now - last_refresh < 500) return 0;
    last_refresh = now;

    uint32_t tram = sys_total_ram() / 1024 / 1024;
    uint32_t uram = sys_used_ram()  / 1024 / 1024;
    if (!tram) tram = 1;
    label_ratio(g_ram, "RAM : ", uram, tram, " MB");
    ui_progressbar_set_value(g_rambar, uram * 100 / tram);

    uint32_t cpu = sys_get_cpu_usage();
    label_ratio(g_cpu, "CPU : ", cpu, 100, " %");
    ui_progressbar_set_value(g_cpubar, cpu);

    uint32_t tdisk = sys_get_total_disk() / 1024 / 1024;
    uint32_t udisk = sys_get_used_disk()  / 1024 / 1024;
    if (!tdisk) tdisk = 1;
    label_ratio(g_disk, "Disk: ", udisk, tdisk, " MB");
    ui_progressbar_set_value(g_diskbar, udisk * 100 / tdisk);

    label_ratio(g_up, "Uptime: ", (uint32_t)(now / 1000), 0, " s");
    return 1;
}

void main(void) {
    ui_window_t* win = ui_window_create(340, 260);
    if (!win) { sys_exit(); }
    ui_window_set_title(win, "Task Manager");

    ui_widget_t* box = ui_vbox_create(win, 6);
    g_ram = ui_label_create(win, "RAM : -");
    ui_layout_add(box, g_ram);
    g_rambar = ui_progressbar_create(win, 300);
    ui_layout_add(box, g_rambar);
    g_cpu = ui_label_create(win, "CPU : -");
    ui_layout_add(box, g_cpu);
    g_cpubar = ui_progressbar_create(win, 300);
    ui_layout_add(box, g_cpubar);
    g_disk = ui_label_create(win, "Disk: -");
    ui_layout_add(box, g_disk);
    g_diskbar = ui_progressbar_create(win, 300);
    ui_layout_add(box, g_diskbar);
    g_up = ui_label_create(win, "Uptime: -");
    ui_layout_add(box, g_up);
    ui_window_add(win, box);

    ui_window_set_tick(win, tick, 0);
    tick(0);   // render nilai awal
    ui_window_run(win);   // blocking; keluar via X / ESC
    ui_window_destroy(win);
    sys_exit();
}
