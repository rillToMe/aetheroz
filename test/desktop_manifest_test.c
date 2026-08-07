// Self-check discover_apps() + parser manifest di user_apps/desktop.c.
// Host build (bukan task QEMU) — desktop.c di-include langsung, syscall FS
// di-stub dengan FS palsu di bawah. -iquote (bukan -I) supaya include/string.h
// milik kernel tidak menutupi <string.h> host:
//   clang -iquote include test/desktop_manifest_test.c -o /tmp/dm && /tmp/dm

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define main desktop_main          // main() desktop bukan entry test ini
#include "../user_apps/desktop.c"
#undef main

// --- FS palsu ---
typedef struct { const char* name; const char* data; } FakeFile;
static FakeFile FS[] = {
    { "fileman.elf", 0 },
    { "fileman.app", "name=Explorer\ncolor=0x1565C0\n" },
    { "calc.elf",    0 },
    { "calc.app",    "name=Kalkulator\r\ncolor=0x2E7D32\r\n" },   // CRLF
    { "desktop.elf", 0 },
    { "desktop.app", "name=Desktop\nhidden=1\n" },
    { "badptr.elf",  0 },                                          // tanpa manifest
    { "kyuzen.png",  0 },                                          // bukan .elf
    { "notes.txt",   0 },
};
#define FS_N ((int)(sizeof(FS) / sizeof(FS[0])))

static int find(const char* n) {
    for (int i = 0; i < FS_N; i++) if (!strcmp(FS[i].name, n)) return i;
    return -1;
}
int sys_get_file_list(char* path, file_info_t* buf, int max) {
    (void)path;   // Fase 2: path diabaikan — stub host tetap list root
    int n = 0;
    for (int i = 0; i < FS_N && n < max; i++) {
        strcpy(buf[n].filename, FS[i].name);
        buf[n].size = FS[i].data ? (uint32_t)strlen(FS[i].data) : 0;
        buf[n].is_folder = 0;
        n++;
    }
    return n;
}
// Fase 3: desktop.c scan /apps — manifest dipanggil dengan "/apps/<base>.app".
static const char* strip_apps(const char* p) {
    if (p[0] == '/' && p[1] == 'a' && p[2] == 'p' && p[3] == 'p' &&
        p[4] == 's' && p[5] == '/') return p + 6;
    return p;
}
int sys_file_exists(char* f) { int i = find(strip_apps(f)); return i >= 0 && FS[i].data != 0; }
int sys_read_file_to_buffer(char* f, char* buf, uint32_t cap) {
    int i = find(strip_apps(f));
    if (i < 0 || !FS[i].data) return 0;
    uint32_t len = (uint32_t)strlen(FS[i].data);
    if (len > cap) return 0;
    memcpy(buf, FS[i].data, len);
    return 1;
}

// --- stub sisanya (tidak dipanggil oleh test) ---
uint64_t sys_uptime(void) { return 0; }
int  sys_spawn(char* f) { (void)f; return 0; }
void sys_yield(void) {}
void sys_exit(void) {}
int  sys_get_event(kyuzen_event_t* e) { (void)e; return 0; }
int  sys_kwm_get_windows(kwm_window_info_t* b, int m) { (void)b; (void)m; return 0; }
int  sys_kwm_activate_window(int id) { (void)id; return 0; }
void sys_kwm_update_window(int id, uint32_t* b) { (void)id; (void)b; }
gui_window_t* gui_create_desktop(void) { return 0; }
void gui_draw_rect(gui_window_t* w, int x, int y, int cw, int ch, uint32_t c) {
    (void)w; (void)x; (void)y; (void)cw; (void)ch; (void)c;
}
void gui_draw_text(gui_window_t* w, const char* t, int x, int y, uint32_t c) {
    (void)w; (void)t; (void)x; (void)y; (void)c;
}

int main(void) {
    // parse_color: hex, desimal, sampah.
    assert(parse_color("0x1565C0") == 0x1565C0);
    assert(parse_color("0XABCDEF") == 0xABCDEF);
    assert(parse_color("255") == 255);
    assert(parse_color("xyz") == 0);

    // Scan: hanya *.elf; desktop.elf disembunyikan manifest (hidden=1).
    assert(discover_apps() == 1);          // beda dari checksum awal (0)
    assert(g_napps == 3);

    assert(!strcmp(g_apps[0].label, "Explorer"));      // dari manifest
    assert(!strcmp(g_apps[0].elf, "/apps/fileman.elf"));
    assert(g_apps[0].color == 0x1565C0);

    assert(!strcmp(g_apps[1].label, "Kalkulator"));    // CRLF ikut ter-trim
    assert(g_apps[1].color == 0x2E7D32);

    assert(!strcmp(g_apps[2].label, "badptr"));        // fallback tanpa manifest
    assert(!strcmp(g_apps[2].elf, "/apps/badptr.elf"));
    assert(g_apps[2].color == APP_DEFAULT);

    assert(discover_apps() == 0);          // scan ulang tanpa perubahan FS

    // Grid: kapasitas dibatasi layar, tidak pernah > jumlah app.
    gui_window_t d = { 0 };
    d.width = 640; d.height = 480;
    assert(grid_cols(&d) == (640 - ICON_X0) / CELL_W);
    assert(grid_cap(&d) == g_napps);
    d.width = 100; d.height = 80;          // layar kerdil → minimal 1 kolom
    assert(grid_cols(&d) == 1);
    assert(grid_cap(&d) >= 1 && grid_cap(&d) <= g_napps);

    printf("desktop manifest: OK\n");
    return 0;
}
