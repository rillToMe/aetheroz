// user_apps/terminal.c — Terminal (Phase 10): console window (libui).
//
// Bukan PTY (ponytail: butuh infra kernel PTY + device node) — command loop
// userspace yang output-nya ditulis ke TextEdit readonly. Perintah yang
// kernel cetak ke TTY (mis. output ping) tidak muncul di sini; handler
// menampilkan hasil lewat return value syscall.
//
// Build: terminal.o + userlib.o + libgui.o + libui.o

#include "userlib.h"
#include "libui.h"

static ui_widget_t* out;   // output (TextEdit readonly)
static ui_widget_t* in;    // input (TextBox)

static int slen(const char* s) { int n = 0; while (s[n]) n++; return n; }

static void itoa(uint32_t n, char* b) {
    if (n == 0) { b[0] = '0'; b[1] = '\0'; return; }
    char t[16]; int i = 0;
    while (n > 0 && i < 15) { t[i++] = '0' + (n % 10); n /= 10; }
    int j = 0; while (i > 0) b[j++] = t[--i]; b[j] = '\0';
}

static void outln(const char* s) { ui_textedit_append(out, s); ui_textedit_append(out, "\n"); }

// Tulis label diikuti angka tanpa spasi (mis. "CPU : 42%").
static void outnum(const char* label, uint32_t v, const char* suffix) {
    char b[64];
    int k = 0;
    while (label[k]) { b[k] = label[k]; k++; }
    char num[16]; itoa(v, num);
    for (int i = 0; num[i] && k < 60; i++) b[k++] = num[i];
    if (suffix) for (int i = 0; suffix[i] && k < 63; i++) b[k++] = suffix[i];
    b[k] = '\0';
    outln(b);
}

// --- perintah ---
static void cmd_help(void) {
    outln("Perintah: help, clear, echo <teks>, ls, baca <file>,");
    outln("hapus <file>, fetch, sched, start <app>, time,");
    outln("ping <host>, shutdown, restart");
}
static void cmd_echo(const char* arg) {
    if (!arg || !arg[0]) { outln("Penggunaan: echo [teks]"); return; }
    outln(arg);
}
static void cmd_ls(void) {
    file_info_t fi[32];
    int n = sys_get_file_list(fi, 32);
    if (n < 0) { outln("ls: gagal"); return; }
    if (n == 0) { outln("(kosong)"); return; }
    for (int i = 0; i < n; i++) {
        char b[48];
        int k = 0;
        while (fi[i].filename[k] && k < 23) { b[k] = fi[i].filename[k]; k++; }
        b[k] = '\0';
        outln(b);
    }
}
static void cmd_baca(const char* arg) {
    if (!arg || !arg[0]) { outln("Penggunaan: baca [file]"); return; }
    if (!sys_file_exists((char*)arg)) { outln("baca: file tidak ada"); return; }
    uint32_t sz = sys_file_size((char*)arg);
    if (sz >= 8192) sz = 8191;
    char* buf = (char*)sys_alloc(sz + 1);
    if (!buf) { outln("baca: OOM"); return; }
    if (sys_read_file_to_buffer((char*)arg, buf, sz + 1)) {
        buf[sz] = '\0';
        ui_textedit_append(out, buf);
        ui_textedit_append(out, "\n");
    } else {
        outln("baca: gagal");
    }
    sys_free(buf);
}
static void cmd_hapus(const char* arg) {
    if (!arg || !arg[0]) { outln("Penggunaan: hapus [file]"); return; }
    fs_delete((char*)arg);
    outln("dihapus");
}
static void cmd_fetch(void) {
    char cpu[49]; get_cpu_string(cpu);
    uint32_t used = sys_used_ram() / 1024 / 1024;
    uint32_t tot  = sys_total_ram() / 1024 / 1024;
    outln("OS   : KyuzenOS (Ring 3)");
    outln("Arch : x86_64 (Long Mode)");
    char b[96];
    int k = 0;
    const char* p = "CPU  : ";
    while (p[k]) { b[k] = p[k]; k++; }
    for (int i = 0; cpu[i] && k < 90; i++) b[k++] = cpu[i];
    b[k] = '\0';
    outln(b);
    outnum("RAM  : ", used, " MB / ");
    {
        char n[16]; itoa(tot, n);
        ui_textedit_append(out, n);
        outln(" MB");
    }
}
static void cmd_sched(void) { outnum("CPU  : ", sys_get_cpu_usage(), "%"); }
static void cmd_time(void) {
    uint32_t t[6];   // [year, month, day, hour, min, sec]
    sys_get_time(t);
    char b[16];
    int k = 0;
    uint32_t hh = t[3], mm = t[4], ss = t[5];
    if (hh < 10) b[k++] = '0';
    char num[16]; itoa(hh, num);
    for (int i = 0; num[i]; i++) b[k++] = num[i];
    b[k++] = ':';
    if (mm < 10) b[k++] = '0';
    itoa(mm, num);
    for (int i = 0; num[i]; i++) b[k++] = num[i];
    b[k++] = ':';
    if (ss < 10) b[k++] = '0';
    itoa(ss, num);
    for (int i = 0; num[i]; i++) b[k++] = num[i];
    b[k] = '\0';
    outln(b);
}
static void cmd_ping(const char* arg) {
    if (!arg || !arg[0]) { outln("Penggunaan: ping [host]"); return; }
    int rtt = sys_ping(arg);   // kernel cetak detail ke TTY (tak tampil di sini)
    if (rtt < 0) outln("ping : tidak ada balasan (timeout)");
    else outnum("ping : ", (uint32_t)rtt, " ms");
}
static void cmd_start(const char* arg) {
    if (!arg || !arg[0]) { outln("Penggunaan: start [app]"); return; }
    char elf[32];
    int i = 0;
    while (arg[i] && i < 27) { elf[i] = arg[i]; i++; }
    elf[i] = '\0';
    int has_ext = (i >= 4 && elf[i-4]=='.' && elf[i-3]=='e' &&
                   elf[i-2]=='l' && elf[i-1]=='f');
    if (!has_ext && i < 28) { elf[i++]='.'; elf[i++]='e'; elf[i++]='l'; elf[i++]='f'; elf[i]='\0'; }
    if (!sys_file_exists(elf)) { outln("start: file tidak ada"); return; }
    int tid = sys_spawn(elf);
    if (tid < 0) outln("start: gagal (slot task penuh / OOM)");
    else {
        char b[64];
        int k = 0;
        const char* p = "start: ";
        while (p[k]) { b[k] = p[k]; k++; }
        for (int j = 0; elf[j] && k < 40; j++) b[k++] = elf[j];
        p = " (task ";
        for (int j = 0; p[j] && k < 58; j++) b[k++] = p[j];
        char n[16]; itoa((uint32_t)tid, n);
        for (int j = 0; n[j] && k < 62; j++) b[k++] = n[j];
        b[k++] = ')'; b[k] = '\0';
        outln(b);
    }
}

static void on_enter(void* userdata) {
    (void)userdata;
    const char* cmd = ui_textbox_text(in);
    ui_textedit_append(out, "kyuzen@de> ");
    ui_textedit_append(out, cmd);
    ui_textedit_append(out, "\n");

    if (cmd[0]) {
        char line[256];
        int n = 0;
        while (cmd[n] && n < 255) { line[n] = cmd[n]; n++; }
        line[n] = '\0';
        char* arg = 0;
        for (int i = 0; i < n; i++) {
            if (line[i] == ' ') { line[i] = '\0'; arg = &line[i + 1]; break; }
        }
        const char* c = line;
        if      (strcmp(c, "help") == 0)      cmd_help();
        else if (strcmp(c, "clear") == 0)     ui_textedit_clear(out);
        else if (strcmp(c, "echo") == 0)      cmd_echo(arg);
        else if (strcmp(c, "ls") == 0)        cmd_ls();
        else if (strcmp(c, "baca") == 0)      cmd_baca(arg);
        else if (strcmp(c, "hapus") == 0)     cmd_hapus(arg);
        else if (strcmp(c, "fetch") == 0)     cmd_fetch();
        else if (strcmp(c, "sched") == 0)     cmd_sched();
        else if (strcmp(c, "time") == 0)      cmd_time();
        else if (strcmp(c, "start") == 0)     cmd_start(arg);
        else if (strcmp(c, "ping") == 0)      cmd_ping(arg);
        else if (strcmp(c, "shutdown") == 0)  sys_shutdown();
        else if (strcmp(c, "restart") == 0)   sys_reboot();
        else { ui_textedit_append(out, "Perintah tidak dikenali: "); outln(c); }
    }
    ui_textbox_set_text(in, "");
}

void main(void) {
    ui_window_t* win = ui_window_create(480, 360);
    if (!win) { sys_exit(); }
    ui_window_set_title(win, "Terminal");

    ui_widget_t* box = ui_vbox_create(win, 8);
    out = ui_textedit_create(win, 464, 240);
    ui_textedit_set_readonly(out, 1);
    ui_layout_add(box, out);
    in = ui_textbox_create(win, 464);
    ui_textbox_set_enter(in, on_enter, 0);
    ui_layout_add(box, in);
    ui_window_add(win, box);

    ui_textedit_append(out, "KyuzenOS Terminal — ketik help\n");

    ui_window_run(win);   // blocking; keluar via X titlebar / ESC
    ui_window_destroy(win);
    sys_exit();
}
