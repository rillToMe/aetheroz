// ============================================================
// notepad.c — Text Editor (Phase 10): editor teks polos (libui).
// Menu File: Baru / Buka / Simpan. Nama file di TextBox, isi di
// TextEdit. Dari Explorer (edit.tmp) → muat file itu; tutup →
// kembali ke Explorer (kalau memang dibuka dari sana).
//
// Build: notepad.o + userlib.o + libgui.o + libui.o + png.o
// ============================================================
#include "userlib.h"
#include "libui.h"

static ui_window_t* g_win;
static ui_widget_t* g_name;
static ui_widget_t* g_edit;
static int g_from_fileman = 0;

static void menu_new(void* userdata) {
    (void)userdata;
    ui_textbox_set_text(g_name, "catatan.txt");
    ui_textedit_clear(g_edit);
}
static void menu_open(void* userdata) {
    (void)userdata;
    const char* fname = ui_textbox_text(g_name);
    if (!fname[0]) return;
    if (!sys_file_exists((char*)fname)) return;
    uint32_t sz = sys_file_size((char*)fname);
    if (sz > 4096) sz = 4096;
    char* buf = (char*)sys_alloc(sz + 1);
    if (!buf) return;
    sys_read_file_to_buffer((char*)fname, buf, sz + 1);
    buf[sz] = '\0';
    ui_textedit_set_text(g_edit, buf);
    sys_free(buf);
}
static void menu_save(void* userdata) {
    (void)userdata;
    const char* fname = ui_textbox_text(g_name);
    if (!fname[0]) return;
    const char* txt = ui_textedit_text(g_edit);
    int len = 0; while (txt[len]) len++;
    if (sys_file_exists((char*)fname)) fs_delete((char*)fname);
    sys_create_file((char*)fname, (char*)txt, len);
}

void main(void) {
    char fname[64] = "catatan.txt";
    if (sys_file_exists("edit.tmp")) {
        uint32_t ts = sys_file_size("edit.tmp");
        if (ts > 0 && ts < 63) {
            sys_read_file_to_buffer("edit.tmp", fname, sizeof(fname));
            fname[ts] = '\0';
            g_from_fileman = 1;
        }
        fs_delete("edit.tmp");
    }

    g_win = ui_window_create(420, 340);
    if (!g_win) { sys_exit(); }
    ui_window_set_title(g_win, "Text Editor");

    ui_widget_t* mb = ui_menubar_create(g_win);
    ui_widget_t* m = ui_menubar_add_menu(mb, "File");
    ui_menu_add_item(m, "Baru", menu_new, 0);
    ui_menu_add_item(m, "Buka", menu_open, 0);
    ui_menu_add_item(m, "Simpan", menu_save, 0);
    ui_window_add_bar(g_win, mb);

    ui_widget_t* box = ui_vbox_create(g_win, 6);
    g_name = ui_textbox_create(g_win, 400);
    ui_layout_add(box, g_name);
    g_edit = ui_textedit_create(g_win, 400, 250);
    ui_layout_add(box, g_edit);
    ui_window_add(g_win, box);

    ui_textbox_set_text(g_name, fname);
    menu_open(0);   // muat isi file kalau ada

    ui_window_run(g_win);   // blocking; keluar via X / ESC
    ui_window_destroy(g_win);
    if (g_from_fileman) {
        char p[32];
        build_app_path(p, sizeof(p), "fileman.elf");
        sys_exec(p);
    } else {
        sys_exit();
    }
}
