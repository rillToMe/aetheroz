// ============================================================
// KYUZEN OS — CALCULATOR APP
// Kalkulator GUI dengan operasi dasar: +, -, ×, ÷
// Window: 320×420 px
// ============================================================
#include "userlib.h"
#define FONT8x16_IMPLEMENTATION
#include "font8x16.h"

// --- Ukuran & posisi window ---
static int win_id  = -1;
static int win_w   = 320;
static int win_h   = 420;
static uint32_t* canvas = 0;

// --- State Kalkulator ---
static double  accumulator = 0;   // hasil operasi sebelumnya
static double  input       = 0;   // angka yang sedang diketik
static int     op          = 0;   // 0=none, 1=+, 2=-, 3=*, 4=/
static int     has_input   = 0;   // apakah user sedang mengetik angka baru
static int     decimal     = 0;   // apakah sedang mengetik desimal
static int     dec_place   = 1;   // 10, 100, 1000, ...
static int     just_result = 0;   // flag: baru saja tekan =

// --- Warna Tema Dark ---
#define COL_BG       0xFF1A1A2E   // background gelap (navy-black)
#define COL_DISPLAY  0xFF16213E   // display area
#define COL_BTN_NUM  0xFF0F3460   // tombol angka (biru tua)
#define COL_BTN_OP   0xFFE94560   // tombol operator (merah-pink)
#define COL_BTN_EQ   0xFF533483   // tombol = (ungu)
#define COL_BTN_CLR  0xFF1A1A2E   // tombol C/CE (gelap)
#define COL_BTN_SPEC 0xFF24305E   // tombol khusus (+/-, .)
#define COL_TEXT     0xFFE0E0E0   // teks terang
#define COL_TEXT_OP  0xFFFFFFFF   // teks operator
#define COL_HOVER    0xFF2A4A7E   // hover effect

// ────────────────────────────────────────────────────────────
// Helpers draw
// ────────────────────────────────────────────────────────────
static void fill_rect(int x, int y, int w, int h, uint32_t c) {
    for (int py = y; py < y+h; py++)
        for (int px = x; px < x+w; px++)
            if (px>=0 && px<win_w && py>=0 && py<win_h)
                canvas[py*win_w+px] = c;
}

static void draw_char(char ch, int x, int y, uint32_t col) {
    if (ch < 0 || ch > 127) return;
    const unsigned char* bm = font8x16[(int)ch];
    for (int row = 0; row < 16; row++)
        for (int col2 = 0; col2 < 8; col2++)
            if (bm[row] & (0x80 >> col2)) {
                int px = x+col2, py = y+row;
                if (px>=0 && px<win_w && py>=0 && py<win_h)
                    canvas[py*win_w+px] = col;
            }
}

static void draw_string_r(const char* s, int x, int y, uint32_t col) {
    // right-aligned: mulai dari x, ke kiri
    int len = 0;
    while (s[len]) len++;
    int sx = x - len*8;
    for (int i = 0; i < len; i++)
        draw_char(s[i], sx + i*8, y, col);
}

static void draw_string_c(const char* s, int x, int y, int w, uint32_t col) {
    // center-aligned dalam kotak lebar w, mulai x
    int len = 0;
    while (s[len]) len++;
    int sx = x + (w - len*8)/2;
    for (int i = 0; i < len; i++)
        draw_char(s[i], sx + i*8, y, col);
}

// ────────────────────────────────────────────────────────────
// Konversi double → string
// Mendukung: integer, desimal, negatif
// ────────────────────────────────────────────────────────────
static void double_to_str(double v, char* buf, int maxlen) {
    int i = 0;
    if (maxlen < 2) { buf[0]='\0'; return; }

    // Handle negative
    if (v < 0) { buf[i++]='-'; v=-v; }

    // Clamp besar
    if (v > 99999999.0) {
        // Tampilkan sebagai integer besar (overflow)
        buf[0]='E'; buf[1]='R'; buf[2]='R'; buf[3]='\0';
        return;
    }

    // Bagian integer
    int int_part = (int)v;
    double frac  = v - (double)int_part;

    // integer ke string
    char tmp[16]; int ti = 0;
    if (int_part == 0) { tmp[ti++]='0'; }
    else {
        int n = int_part;
        while (n > 0 && ti < 15) { tmp[ti++] = '0' + (n%10); n/=10; }
        // reverse
        for (int a=0,b=ti-1; a<b; a++,b--) { char t=tmp[a]; tmp[a]=tmp[b]; tmp[b]=t; }
    }
    for (int k=0; k<ti && i<maxlen-1; k++) buf[i++]=tmp[k];

    // Bagian desimal (hanya jika ada)
    if (frac > 0.0001) {
        if (i < maxlen-1) buf[i++]='.';
        int places = 4;
        while (places-- > 0 && i < maxlen-1) {
            frac *= 10.0;
            int d = (int)frac;
            buf[i++] = '0' + d;
            frac -= d;
        }
        // Hapus trailing zeros
        while (i > 1 && buf[i-1]=='0') i--;
        if (i > 1 && buf[i-1]=='.') i--; // hapus '.' kalau tidak ada desimal
    }

    buf[i] = '\0';
}

// ────────────────────────────────────────────────────────────
// Layout tombol: 4 kolom × 5 baris
// ────────────────────────────────────────────────────────────
#define BTN_COLS   4
#define BTN_ROWS   5
#define BTN_W      70
#define BTN_H      60
#define BTN_PAD    8
#define BTN_OFF_X  10
#define BTN_OFF_Y  150   // mulai dari bawah display

// Label tombol [baris][kolom]
static const char* btn_labels[BTN_ROWS][BTN_COLS] = {
    { "C",  "+/-", "%",  "/" },
    { "7",  "8",   "9",  "*" },
    { "4",  "5",   "6",  "-" },
    { "1",  "2",   "3",  "+" },
    { "0",  ".",   "CE", "=" },
};

// Tipe tombol untuk warna
// 0=num, 1=op, 2=eq, 3=clear, 4=special
static const int btn_type[BTN_ROWS][BTN_COLS] = {
    { 3, 4, 4, 1 },
    { 0, 0, 0, 1 },
    { 0, 0, 0, 1 },
    { 0, 0, 0, 1 },
    { 0, 4, 3, 2 },
};

static uint32_t btn_color(int type) {
    switch(type) {
        case 1: return COL_BTN_OP;
        case 2: return COL_BTN_EQ;
        case 3: return COL_BTN_CLR;
        case 4: return COL_BTN_SPEC;
        default: return COL_BTN_NUM;
    }
}

// Gambar satu tombol (dengan rounded corner simulasi via inner fill)
static void draw_button(int col, int row, int hovered) {
    int x = BTN_OFF_X + col*(BTN_W+BTN_PAD);
    int y = BTN_OFF_Y + row*(BTN_H+BTN_PAD);
    uint32_t c = hovered ? COL_HOVER : btn_color(btn_type[row][col]);

    // Shadow
    fill_rect(x+3, y+3, BTN_W, BTN_H, 0xFF050510);
    // Button body
    fill_rect(x, y, BTN_W, BTN_H, c);
    // Highlight (top edge)
    fill_rect(x, y, BTN_W, 2, c | 0x304040FF);

    // Label
    uint32_t tc = COL_TEXT_OP;
    int ly = y + (BTN_H - 16)/2;
    draw_string_c(btn_labels[row][col], x, ly, BTN_W, tc);
}

// ────────────────────────────────────────────────────────────
// Render seluruh UI
// ────────────────────────────────────────────────────────────
static void render_all(int hover_col, int hover_row) {
    // Background
    fill_rect(0, 0, win_w, win_h, COL_BG);

    // Title bar
    fill_rect(0, 0, win_w, 30, 0xFF0D0D1A);
    draw_string_c("KALKULATOR", 0, 7, win_w, 0xFFAAAAAA);

    // Close button
    fill_rect(win_w-36, 2, 32, 26, COL_BTN_OP);
    draw_string_c("X", win_w-36, 7, 32, COL_TEXT_OP);

    // Display area
    fill_rect(0, 30, win_w, 120, COL_DISPLAY);
    // Border bawah display
    fill_rect(0, 149, win_w, 2, 0xFF0A0A20);

    // Op indicator (kiri atas display)
    const char* op_str = "";
    if (op==1) op_str="[+]";
    else if (op==2) op_str="[-]";
    else if (op==3) op_str="[*]";
    else if (op==4) op_str="[/]";

    // Tampilkan accumulator kecil (operand pertama)
    if (op != 0 && !just_result) {
        char acc_str[24];
        double_to_str(accumulator, acc_str, 24);
        draw_string_r(acc_str, win_w-12, 50, 0xFF6688AA);
        // op indicator
        int olen = 0; while(op_str[olen]) olen++;
        int osx = 12;
        for (int i=0; i<olen; i++) draw_char(op_str[i], osx+i*8, 50, 0xFFE94560);
    }

    // Angka utama (besar, right-aligned)
    char disp_str[24];
    double_to_str(input, disp_str, 24);

    // Gambar angka besar (2× scale via loop pixel)
    // Scale = 2: setiap pixel jadi 2x2 blok
    int len = 0; while(disp_str[len]) len++;
    int scale = (len > 8) ? 1 : 2;  // kalau panjang, turunkan skala
    int total_w = len * 8 * scale;
    int sx = win_w - 12 - total_w;
    int sy = 90;

    for (int ci = 0; ci < len; ci++) {
        char ch = disp_str[ci];
        if (ch < 0 || ch > 127) continue;
        const unsigned char* bm = font8x16[(int)ch];
        for (int row = 0; row < 16; row++)
            for (int col2 = 0; col2 < 8; col2++)
                if (bm[row] & (0x80 >> col2))
                    for (int dy=0; dy<scale; dy++)
                        for (int dx=0; dx<scale; dx++) {
                            int px = sx + ci*8*scale + col2*scale + dx;
                            int py = sy + row*scale + dy;
                            if (px>=0 && px<win_w && py>=0 && py<win_h)
                                canvas[py*win_w+px] = COL_TEXT_OP;
                        }
    }

    // Tombol-tombol
    for (int r = 0; r < BTN_ROWS; r++)
        for (int c = 0; c < BTN_COLS; c++)
            draw_button(c, r, (c==hover_col && r==hover_row));

    sys_update_window(win_id, canvas);
}

// ────────────────────────────────────────────────────────────
// Hit test: klik (rx,ry) ke tombol mana?
// ────────────────────────────────────────────────────────────
static int hit_col(int rx) {
    for (int c=0; c<BTN_COLS; c++) {
        int x = BTN_OFF_X + c*(BTN_W+BTN_PAD);
        if (rx >= x && rx < x+BTN_W) return c;
    }
    return -1;
}
static int hit_row(int ry) {
    for (int r=0; r<BTN_ROWS; r++) {
        int y = BTN_OFF_Y + r*(BTN_H+BTN_PAD);
        if (ry >= y && ry < y+BTN_H) return r;
    }
    return -1;
}

// ────────────────────────────────────────────────────────────
// Proses klik tombol
// ────────────────────────────────────────────────────────────
static void press_button(int col, int row) {
    const char* lbl = btn_labels[row][col];

    // Angka 0-9
    if (lbl[0] >= '0' && lbl[0] <= '9' && lbl[1] == '\0') {
        int digit = lbl[0] - '0';
        if (just_result) {
            // Setelah tekan =, mulai angka baru
            input = 0; accumulator = 0; op = 0;
            just_result = 0; decimal = 0; dec_place = 1;
        }
        if (!has_input) { input = 0; decimal = 0; dec_place = 1; has_input = 1; }
        if (!decimal) {
            input = input * 10.0 + digit;
        } else {
            dec_place *= 10;
            input = input + (double)digit / (double)dec_place;
        }
        return;
    }

    // Titik desimal
    if (lbl[0]=='.' && lbl[1]=='\0') {
        if (!has_input) has_input = 1;
        if (!decimal) { decimal = 1; dec_place = 1; }
        return;
    }

    // CE: hapus angka terakhir / reset input
    if (lbl[0]=='C' && lbl[1]=='E') {
        input = 0; decimal = 0; dec_place = 1; has_input = 0;
        return;
    }

    // C: clear all
    if (lbl[0]=='C' && lbl[1]=='\0') {
        input = 0; accumulator = 0; op = 0;
        decimal = 0; dec_place = 1; has_input = 0; just_result = 0;
        return;
    }

    // +/- toggle
    if (lbl[0]=='+' && lbl[1]=='/') {
        input = -input;
        return;
    }

    // % (persen dari accumulator)
    if (lbl[0]=='%') {
        if (op != 0) input = accumulator * input / 100.0;
        else input = input / 100.0;
        has_input = 1;
        return;
    }

    // = (hitung)
    if (lbl[0]=='=') {
        if (op == 0) { just_result = 1; return; }
        double result = 0;
        switch(op) {
            case 1: result = accumulator + input; break;
            case 2: result = accumulator - input; break;
            case 3: result = accumulator * input; break;
            case 4:
                if (input == 0.0) { result = 0; } // div by zero → 0
                else result = accumulator / input;
                break;
        }
        accumulator = result;
        input = result;
        op = 0;
        has_input = 0;
        decimal = 0; dec_place = 1;
        just_result = 1;
        return;
    }

    // Operator: + - * /
    int new_op = 0;
    if (lbl[0]=='+') new_op=1;
    else if (lbl[0]=='-') new_op=2;
    else if (lbl[0]=='*') new_op=3;
    else if (lbl[0]=='/') new_op=4;

    if (new_op != 0) {
        if (op != 0 && has_input) {
            // Hitung dulu operasi sebelumnya (chaining)
            double result = 0;
            switch(op) {
                case 1: result = accumulator + input; break;
                case 2: result = accumulator - input; break;
                case 3: result = accumulator * input; break;
                case 4:
                    if (input == 0.0) result = 0;
                    else result = accumulator / input;
                    break;
            }
            accumulator = result;
            input = result;
        } else {
            accumulator = input;
        }
        op = new_op;
        has_input = 0;
        decimal = 0; dec_place = 1;
        just_result = 0;
        return;
    }
}

// ────────────────────────────────────────────────────────────
// MAIN
// ────────────────────────────────────────────────────────────
void main() {
    win_id = sys_create_window(180, 80, win_w, win_h);
    if (win_id < 0) { sys_exit(); }

    canvas = (uint32_t*)sys_alloc(win_w * win_h * 4);
    if (!canvas) { sys_destroy_window(win_id); sys_exit(); }

    int hover_col = -1, hover_row = -1;
    int mouse_x = 0, mouse_y = 0;

    render_all(-1, -1);

    kyuzen_event_t ev;
    while (1) {
        if (sys_get_event(&ev)) {
            // Update mouse position
            if (ev.type == EVENT_MOUSE_MOVE) {
                mouse_x = ev.param1;
                mouse_y = ev.param2;

                // Window di posisi (180, 80) layar
                int rx = mouse_x - 180;
                int ry = mouse_y - 80;

                int nc = hit_col(rx), nr = hit_row(ry);
                if (nc != hover_col || nr != hover_row) {
                    hover_col = nc; hover_row = nr;
                    render_all(hover_col, hover_row);
                }
            }

            if (ev.type == EVENT_MOUSE_CLICK && ev.param1 == 0 && ev.param2 == 1) {
                if (ev.param3 != 0) mouse_x = ev.param3;

                int rx = mouse_x - 180;
                int ry = mouse_y - 80;

                // Tombol Close (X)
                if (rx >= (win_w-36) && rx <= win_w && ry >= 2 && ry <= 28) {
                    break;
                }

                int cc = hit_col(rx), cr = hit_row(ry);
                if (cc >= 0 && cr >= 0) {
                    press_button(cc, cr);
                    render_all(hover_col, hover_row);
                }
            }

            if (ev.type == EVENT_KEY_PRESS && ev.param1 == 27) break; // ESC
        }
        sys_yield();
    }

    sys_destroy_window(win_id);
    sys_free(canvas);
    sys_exit();
}
