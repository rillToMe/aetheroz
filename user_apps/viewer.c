// ============================================================
// viewer.c — Image Viewer (Phase 10): galeri PNG (libui).
// Daftar .png di atas (ListView), gambar di bawah (Image dalam
// ScrollView), zoom ± lewat toolbar. Dari Explorer (view.tmp)
// → muat file itu langsung; tutup → kembali ke Explorer.
//
// Build: viewer.o + userlib.o + libgui.o + libui.o + png.o
// ============================================================
#include "userlib.h"
#include "libui.h"

#define MAX_PNG 32

static ui_widget_t* g_list;
static ui_widget_t* g_img;
static char g_names[MAX_PNG][24];
static int g_npng = 0;
static int g_scale = 100;
static int g_from_fileman = 0;

static int is_png(const char* name) {
    int n = 0; while (name[n]) n++;
    return n > 4 && name[n-4]=='.' && name[n-3]=='p' &&
           name[n-2]=='n' && name[n-1]=='g';
}

static void scan_pngs(void) {
    file_info_t fi[32];
    int total = sys_get_file_list("/", fi, 32);
    g_npng = 0;
    for (int i = 0; i < total && g_npng < MAX_PNG; i++) {
        if (fi[i].is_folder || !is_png(fi[i].filename)) continue;
        int k = 0;
        while (fi[i].filename[k] && k < 23) { g_names[g_npng][k] = fi[i].filename[k]; k++; }
        g_names[g_npng][k] = '\0';
        ui_listview_add_item(g_list, g_names[g_npng]);
        g_npng++;
    }
}

static void show_file(const char* name) {
    g_scale = 100;
    ui_image_set_file(g_img, name);   // reset zoom; ScrollView sesuaikan ukuran
}

static void on_select(void* userdata) {
    (void)userdata;
    int sel = ui_listview_selected(g_list);
    if (sel >= 0 && sel < g_npng) show_file(g_names[sel]);
}
static void zoom_plus(void* userdata) {
    (void)userdata;
    if (g_scale < 400) g_scale += 10;
    ui_image_set_scale(g_img, g_scale);
}
static void zoom_minus(void* userdata) {
    (void)userdata;
    if (g_scale > 10) g_scale -= 10;
    ui_image_set_scale(g_img, g_scale);
}

void main(void) {
    char target[64];
    if (sys_file_exists("view.tmp")) {
        uint32_t ts = sys_file_size("view.tmp");
        if (ts > 0 && ts < 63) {
            sys_read_file_to_buffer("view.tmp", target, sizeof(target));
            target[ts] = '\0';
            g_from_fileman = 1;
        }
        fs_delete("view.tmp");   // konsumsi flag sekali pakai
    }

    ui_window_t* win = ui_window_create(480, 360);
    if (!win) { sys_exit(); }
    ui_window_set_title(win, "Image Viewer");

    ui_widget_t* tb = ui_toolbar_create(win);
    ui_toolbar_add_button(tb, "Zoom -", zoom_minus, 0);
    ui_toolbar_add_button(tb, "Zoom +", zoom_plus, 0);
    ui_window_add_bar(win, tb);

    ui_widget_t* box = ui_vbox_create(win, 4);
    g_list = ui_listview_create(win, 464, 96);
    ui_listview_set_change(g_list, on_select, 0);
    ui_layout_add(box, g_list);

    ui_widget_t* sv = ui_scrollview_create(win, 464, 210);
    g_img = ui_image_create(win, "", 0, 0);   // kosong sampai dipilih
    ui_scrollview_set_child(sv, g_img);
    ui_layout_add(box, sv);

    ui_window_add(win, box);
    scan_pngs();
    if (g_from_fileman) show_file(target);

    ui_window_run(win);   // blocking; keluar via X / ESC
    ui_window_destroy(win);
    if (g_from_fileman) sys_exec("fileman.elf");
    else sys_exit();
}
