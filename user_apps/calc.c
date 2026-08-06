// ============================================================
// calc.c — Kyuzen Calculator App (libgui standard)
// Kalkulator GUI dengan operasi: +, -, ×, ÷, %, +/-
// ============================================================
#include "userlib.h"
#include "libgui.h"

// --- Ukuran ---
#define WIN_W  320
#define WIN_H  450

// --- Warna ---
#define COL_BG      0x1A1A2E
#define COL_DISPLAY 0x16213E
#define COL_BTN_NUM 0x0F3460
#define COL_BTN_OP  0xE94560
#define COL_BTN_EQ  0x533483
#define COL_BTN_CLR 0x111122
#define COL_BTN_SPC 0x24305E
#define COL_TEXT    0xE0E0E0
#define COL_HOVER   0x2A4A7E

// --- State kalkulator ---
static double accumulator = 0;
static double input_val   = 0;
static int    op          = 0;   // 0=none 1=+ 2=- 3=* 4=/
static int    has_input   = 0;
static int    decimal     = 0;
static int    dec_place   = 1;
static int    just_result = 0;

// --- Layout tombol ---
#define BTN_COLS  4
#define BTN_ROWS  5
#define BTN_W     68
#define BTN_H     58
#define BTN_PAD   8
#define BTN_OFF_X 10
#define BTN_OFF_Y 156  // relatif ke area isi (di bawah title bar)

static const char* btn_labels[BTN_ROWS][BTN_COLS] = {
    { "C",  "+/-", "%",  "/" },
    { "7",  "8",   "9",  "*" },
    { "4",  "5",   "6",  "-" },
    { "1",  "2",   "3",  "+" },
    { "0",  ".",   "CE", "=" },
};
static const int btn_type[BTN_ROWS][BTN_COLS] = {
    { 3, 4, 4, 1 }, { 0, 0, 0, 1 }, { 0, 0, 0, 1 },
    { 0, 0, 0, 1 }, { 0, 4, 3, 2 },
};
static uint32_t btn_bg(int type) {
    switch(type) {
        case 1: return COL_BTN_OP;
        case 2: return COL_BTN_EQ;
        case 3: return COL_BTN_CLR;
        case 4: return COL_BTN_SPC;
        default: return COL_BTN_NUM;
    }
}

// --- Helpers ---
static int _str_len(const char* s) { int n=0; while(s[n]) n++; return n; }

static void double_to_str(double v, char* buf, int maxlen) {
    int i = 0;
    if (maxlen < 2) { buf[0]='\0'; return; }
    if (v < 0) { buf[i++]='-'; v=-v; }
    if (v > 99999999.0) { buf[0]='E'; buf[1]='R'; buf[2]='R'; buf[3]='\0'; return; }
    int int_part = (int)v;
    double frac = v - (double)int_part;
    char tmp[16]; int ti=0;
    if (int_part == 0) tmp[ti++]='0';
    else { int n=int_part; while(n>0&&ti<15){tmp[ti++]='0'+(n%10);n/=10;} }
    for (int a=0,b=ti-1;a<b;a++,b--){char t=tmp[a];tmp[a]=tmp[b];tmp[b]=t;}
    for (int k=0;k<ti&&i<maxlen-1;k++) buf[i++]=tmp[k];
    if (frac > 0.0001) {
        if (i<maxlen-1) buf[i++]='.';
        int places=4;
        while (places-->0&&i<maxlen-1){frac*=10.0;int d=(int)frac;buf[i++]='0'+d;frac-=d;}
        while (i>1&&buf[i-1]=='0') i--;
        if (i>1&&buf[i-1]=='.') i--;
    }
    buf[i]='\0';
}

// Gambar teks rata kanan di canvas (koordinat absolut)
static void draw_r(gui_window_t* win, const char* s, int rx, int y, uint32_t col) {
    extern const unsigned char font8x16[256][16];
    int len = _str_len(s);
    int sx = rx - len*8;
    int W = (int)win->width, H = (int)win->height;
    uint32_t solid = col | 0xFF000000;
    for (int ci=0; ci<len; ci++) {
        char ch = s[ci]; if (ch<0||ch>127) continue;
        const unsigned char* bm = font8x16[(int)(unsigned char)ch];
        for (int row=0;row<16;row++)
            for (int bit=0;bit<8;bit++)
                if (bm[row]&(0x80>>bit)) {
                    int px=sx+ci*8+bit, py=y+row;
                    if (px>=0&&px<W&&py>=0&&py<H) win->canvas[py*W+px]=solid;
                }
    }
}

// --- Hit test ---
static int hit_col(int rx) {
    for (int c=0;c<BTN_COLS;c++) {
        int x = BTN_OFF_X + c*(BTN_W+BTN_PAD);
        if (rx>=x && rx<x+BTN_W) return c;
    }
    return -1;
}
static int hit_row(int ry) {
    for (int r=0;r<BTN_ROWS;r++) {
        int y = BTN_OFF_Y + r*(BTN_H+BTN_PAD);
        if (ry>=y && ry<y+BTN_H) return r;
    }
    return -1;
}

// --- Proses tombol ---
static void press(int col, int row) {
    const char* lbl = btn_labels[row][col];
    if (lbl[0]>='0'&&lbl[0]<='9'&&lbl[1]=='\0') {
        int d=lbl[0]-'0';
        if (just_result){input_val=0;accumulator=0;op=0;just_result=0;decimal=0;dec_place=1;}
        if (!has_input){input_val=0;decimal=0;dec_place=1;has_input=1;}
        if (!decimal) input_val=input_val*10.0+d;
        else { dec_place*=10; input_val+=((double)d/(double)dec_place); }
        return;
    }
    if (lbl[0]=='.'&&lbl[1]=='\0'){if(!has_input)has_input=1;if(!decimal){decimal=1;dec_place=1;}return;}
    if (lbl[0]=='C'&&lbl[1]=='E'){input_val=0;decimal=0;dec_place=1;has_input=0;return;}
    if (lbl[0]=='C'&&lbl[1]=='\0'){input_val=0;accumulator=0;op=0;decimal=0;dec_place=1;has_input=0;just_result=0;return;}
    if (lbl[0]=='+'&&lbl[1]=='/'){input_val=-input_val;return;}
    if (lbl[0]=='%'){input_val=(op!=0)?accumulator*input_val/100.0:input_val/100.0;has_input=1;return;}
    if (lbl[0]=='=') {
        if (op==0){just_result=1;return;}
        double r=0;
        switch(op){case 1:r=accumulator+input_val;break;case 2:r=accumulator-input_val;break;
                   case 3:r=accumulator*input_val;break;case 4:r=(input_val==0)?0:accumulator/input_val;break;}
        accumulator=r;input_val=r;op=0;has_input=0;decimal=0;dec_place=1;just_result=1;return;
    }
    int new_op=0;
    if (lbl[0]=='+'&&lbl[1]=='\0') new_op=1;
    else if (lbl[0]=='-') new_op=2;
    else if (lbl[0]=='*') new_op=3;
    else if (lbl[0]=='/') new_op=4;
    if (new_op) {
        if (op&&has_input){
            double r=0;
            switch(op){case 1:r=accumulator+input_val;break;case 2:r=accumulator-input_val;break;
                       case 3:r=accumulator*input_val;break;case 4:r=(input_val==0)?0:accumulator/input_val;break;}
            accumulator=r;input_val=r;
        } else accumulator=input_val;
        op=new_op;has_input=0;decimal=0;dec_place=1;just_result=0;
    }
}

// --- Render ---
static int hover_col = -1, hover_row = -1;

void calc_render(gui_window_t* win) {
    int W = (int)win->width;

    // Background
    gui_draw_rect(win, 0, 0, W, (int)win->inner_h, COL_BG);

    // Display area (relatif ke area isi)
    gui_draw_rect(win, 0, 0, W, 120, COL_DISPLAY);
    gui_draw_rect(win, 0, 119, W, 2, 0x0A0A20);

    // Op indicator + accumulator
    const char* op_str = "";
    if (op==1) op_str="[+]"; else if(op==2) op_str="[-]";
    else if(op==3) op_str="[*]"; else if(op==4) op_str="[/]";
    if (op!=0&&!just_result) {
        char acc_s[24]; double_to_str(accumulator, acc_s, 24);
        draw_r(win, acc_s, W-12, 20, 0x6688AA);
        gui_draw_text(win, op_str, 12, 20, 0xE94560);
    }

    // Angka utama (2x scale, kanan-rata)
    char disp[24]; double_to_str(input_val, disp, 24);
    int len=_str_len(disp), scale=(len>8)?1:2;
    int total_w=len*8*scale, sx=W-12-total_w, sy=60;
    extern const unsigned char font8x16[256][16];
    int H = (int)win->height;
    for (int ci=0;ci<len;ci++) {
        char ch=disp[ci]; if(ch<0||ch>127) continue;
        const unsigned char* bm=font8x16[(int)(unsigned char)ch];
        for(int row=0;row<16;row++)
            for(int bit=0;bit<8;bit++)
                if(bm[row]&(0x80>>bit))
                    for(int dy=0;dy<scale;dy++)
                        for(int dx=0;dx<scale;dx++){
                            int px=sx+ci*8*scale+bit*scale+dx;
                            int py=sy+row*scale+dy;
                            if(px>=0&&px<W&&py>=0&&py<H)
                                win->canvas[py*W+px]=0xFFFFFFFF;
                        }
    }

    // Tombol-tombol
    for (int r=0;r<BTN_ROWS;r++) {
        for (int c=0;c<BTN_COLS;c++) {
            int bx = BTN_OFF_X + c*(BTN_W+BTN_PAD);
            int by = BTN_OFF_Y + r*(BTN_H+BTN_PAD);
            uint32_t bc = (c==hover_col&&r==hover_row) ? COL_HOVER : btn_bg(btn_type[r][c]);
            // Shadow
            gui_draw_rect(win, bx+3, by+3, BTN_W, BTN_H, 0x050510);
            // Body
            gui_draw_rect(win, bx, by, BTN_W, BTN_H, bc);
            // Label (tengah tombol)
            const char* lbl = btn_labels[r][c];
            int llen = _str_len(lbl);
            int lx = bx + (BTN_W - llen*8)/2;
            int ly = by + (BTN_H - 16)/2;
            gui_draw_text(win, lbl, lx, ly, 0xFFFFFF);
        }
    }
}

void main(void) {
    gui_window_t* app = gui_create_window(WIN_W, WIN_H);
    if (!app) { sys_exit(); return; }

    gui_set_render(app, calc_render);

    kyuzen_event_t ev;
    while (app->is_running) {
        if (sys_get_event(&ev)) {
            if (ev.type == EVENT_MOUSE_MOVE) {
                app->mouse_x = ev.param1;
                app->mouse_y = ev.param2;
                // Phase 5C: koordinat window-local konten (dari KWM)
                int rel_x = app->mouse_x;
                int rel_y = app->mouse_y;
                int nc = hit_col(rel_x), nr = hit_row(rel_y);
                if (nc!=hover_col||nr!=hover_row) {
                    hover_col=nc; hover_row=nr;
                    calc_render(app); gui_flush(app);
                }
            }

            if (ev.type == EVENT_MOUSE_CLICK && ev.param1==0 && ev.param2==1) {
                if (ev.param3!=0) app->mouse_x=ev.param3;
                // Koordinat window-local konten
                int rel_x = app->mouse_x;
                int rel_y = app->mouse_y;
                int cc=hit_col(rel_x), cr=hit_row(rel_y);
                if (cc>=0&&cr>=0) { press(cc,cr); calc_render(app); gui_flush(app); }
            }

            if (ev.type==EVENT_WIN_CLOSE) { app->is_running=0; break; }
            if (ev.type==EVENT_KEY_PRESS&&ev.param1==27) { app->is_running=0; break; }
        }
        sys_yield();
    }

    gui_destroy(app);
    sys_exit();
}
