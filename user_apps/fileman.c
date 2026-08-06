// ============================================================
// fileman.c — Explorer (Phase 10): File Manager KyuzenFS (libui).
// Table nama+ukuran; klik pilih; Buka/Refresh. .elf → spawn,
// .png → view.tmp + viewer.elf, .txt → edit.tmp + notepad.elf.
//
// Build: fileman.o + userlib.o + libgui.o + libui.o + png.o
// ============================================================
#include "userlib.h"
#include "libui.h"

#define MAX_FILES 16

static ui_window_t* g_win;
static ui_widget_t* g_table;
static file_info_t g_files[MAX_FILES];
static int g_nfiles = 0;

static int slen(const char* s) { int n = 0; while (s[n]) n++; return n; }
static void itoa(uint32_t n, char* b) {
    if (n == 0) { b[0] = '0'; b[1] = '\0'; return; }
    char t[16]; int i = 0;
    while (n > 0 && i < 15) { t[i++] = '0' + (n % 10); n /= 10; }
    int j = 0; while (i > 0) b[j++] = t[--i]; b[j] = '\0';
}

static void fill_table(void) {
    ui_table_clear(g_table);
    g_nfiles = sys_get_file_list(g_files, MAX_FILES);
    for (int i = 0; i < g_nfiles; i++) {
        char sz[16]; itoa(g_files[i].size, sz);
        const char* cells[2] = { g_files[i].filename, sz };
        ui_table_add_row(g_table, cells, 2);
    }
}

static void do_open(void* userdata) {
    (void)userdata;
    int sel = ui_table_selected(g_table);
    if (sel < 0 || sel >= g_nfiles) return;
    char* fname = g_files[sel].filename;
    int len = slen(fname);

    if (len > 4 && fname[len-4]=='.' && fname[len-3]=='e' &&
        fname[len-2]=='l' && fname[len-1]=='f') {
        ui_window_destroy(g_win);
        sys_exec(fname);            // tak pernah kembali
    } else if (len > 4 && fname[len-4]=='.' && fname[len-3]=='p' &&
               fname[len-2]=='n' && fname[len-1]=='g') {
        if (sys_file_exists("view.tmp")) fs_delete("view.tmp");
        sys_create_file("view.tmp", fname, len);   // arg ke viewer
        ui_window_destroy(g_win);
        sys_exec("viewer.elf");
    } else if (len > 4 && fname[len-4]=='.' && fname[len-3]=='t' &&
               fname[len-2]=='x' && fname[len-1]=='t') {
        if (sys_file_exists("edit.tmp")) fs_delete("edit.tmp");
        sys_create_file("edit.tmp", fname, len);   // arg ke notepad
        ui_window_destroy(g_win);
        sys_exec("notepad.elf");
    }
}

static void do_refresh(void* userdata) { (void)userdata; fill_table(); }

void main(void) {
    g_win = ui_window_create(400, 320);
    if (!g_win) { sys_exit(); }
    ui_window_set_title(g_win, "Explorer");

    ui_widget_t* box = ui_vbox_create(g_win, 6);
    g_table = ui_table_create(g_win, 380, 240);
    ui_table_add_column(g_table, "Nama", 270);
    ui_table_add_column(g_table, "Ukuran", 100);
    ui_layout_add(box, g_table);

    ui_widget_t* btns = ui_hbox_create(g_win, 8);
    ui_widget_t* b = ui_button_create(g_win, "Buka");
    ui_button_set_click(b, do_open, 0);
    ui_layout_add(btns, b);
    b = ui_button_create(g_win, "Refresh");
    ui_button_set_click(b, do_refresh, 0);
    ui_layout_add(btns, b);
    ui_layout_add(box, btns);

    ui_window_add(g_win, box);
    fill_table();

    ui_window_run(g_win);   // blocking; keluar via X / ESC
    ui_window_destroy(g_win);
    sys_exit();
}
