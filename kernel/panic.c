#include <stdint.h>

// ========================================================
// STRUKTUR REGISTER 64-BIT (MURNI)
// Harus cocok dengan PUSHA64 di isr_macro.inc
// ========================================================
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;  
    uint64_t int_num, error_code;                
    uint64_t rip, cs, rflags, rsp, ss;           
} __attribute__((packed)) registers_t;

extern uint32_t* fb_ptr;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;
extern const unsigned char font8x16[256][16];

// Impor fungsi cek rute fisik (dari paging.c)
extern int paging_is_mapped(uint64_t vaddr);

// =======================================================================
// MESIN GAMBAR DARURAT (TANPA MALLOC)
// =======================================================================

void panic_draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb_ptr || x >= fb_width || y >= fb_height) return;
    fb_ptr[(y * (fb_pitch / 4)) + x] = color;
}

void panic_draw_char(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    if ((unsigned char)c > 127) c = '?';
    const unsigned char* bmp = font8x16[(unsigned char)c];
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 8; col++) {
            panic_draw_pixel(x + col, y + row,
                (bmp[row] & (0x80 >> col)) ? fg : bg);
        }
    }
}

void panic_draw_string(const char* str, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    for (int i = 0; str[i]; i++) {
        panic_draw_char(str[i], x + (i * 8), y, fg, bg);
    }
}

// FORMAT HEX 64-BIT TERBARU (Mendukung alamat memori raksasa)
void panic_draw_hex(uint64_t num, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    const char* digits = "0123456789ABCDEF";
    char buf[19] = "0x0000000000000000";
    for (int i = 17; i >= 2; i--) {
        buf[i] = digits[num & 0xF];
        num >>= 4;
    }
    panic_draw_string(buf, x, y, fg, bg);
}

void panic_draw_dec(uint64_t num, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    char buf[22];
    int i = 20;
    buf[21] = '\0';
    if (num == 0) { panic_draw_char('0', x, y, fg, bg); return; }
    while (num > 0 && i >= 0) {
        buf[i--] = '0' + (num % 10);
        num /= 10;
    }
    panic_draw_string(&buf[i + 1], x, y, fg, bg);
}

static void fill_screen(uint32_t color) {
    for (uint32_t y = 0; y < fb_height; y++)
        for (uint32_t x = 0; x < fb_width; x++)
            panic_draw_pixel(x, y, color);
}

// =======================================================================
// NAMA-NAMA EXCEPTION x86
// =======================================================================
static const char* exception_names[] = {
    "0x00 Divide by Zero",
    "0x01 Debug",
    "0x02 Non-Maskable Interrupt",
    "0x03 Breakpoint",
    "0x04 Overflow",
    "0x05 Bound Range Exceeded",
    "0x06 INVALID OPCODE",
    "0x07 Device Not Available",
    "0x08 DOUBLE FAULT",
    "0x09 Coprocessor Segment Overrun",
    "0x0A Invalid TSS",
    "0x0B Segment Not Present",
    "0x0C STACK-SEGMENT FAULT",
    "0x0D GENERAL PROTECTION FAULT",
    "0x0E PAGE FAULT",
};

// =======================================================================
// BSOD GENERIK
// =======================================================================
void kernel_panic(const char* title, const char* desc, uint64_t code) {
    __asm__ volatile("cli");
    if (!fb_ptr) { while(1) { __asm__ volatile("hlt"); } }

    uint32_t BG    = 0x0000AA;
    uint32_t FG    = 0xFFFFFF;
    uint32_t HDR   = 0xAA0000;
    uint32_t WARN  = 0xFFFF00;

    fill_screen(BG);

    panic_draw_string("=====================================================", 50,  40, FG, BG);
    panic_draw_string("   *** KYUZEN OS FATAL KERNEL PANIC *** ", 50,  60, FG, HDR);
    panic_draw_string("=====================================================", 50,  80, FG, BG);

    panic_draw_string("EXCEPTION:", 50, 110, FG, BG);
    panic_draw_string(title,       160, 110, WARN, BG);

    panic_draw_string("DETAILS:  ", 50, 135, FG, BG);
    panic_draw_string(desc,        160, 135, FG, BG);

    panic_draw_string("ERR CODE: ", 50, 160, FG, BG);
    panic_draw_hex(code,           160, 160, WARN, BG);

    panic_draw_string("Sistem dibekukan demi mencegah kerusakan data.", 50, 220, FG, BG);
    while (1) { __asm__ volatile("hlt"); }
}

// =======================================================================
// EXCEPTION HANDLER UTAMA 64-BIT
// (Dipanggil oleh semua Exception ASM melalui Call RDI)
// =======================================================================
void exception_handler(registers_t *r) {
    __asm__ volatile("cli");
    if (!fb_ptr) { while(1) { __asm__ volatile("hlt"); } }

    // Ambil data langsung dari struct register (RDI)
    uint64_t int_num = r->int_num;
    uint64_t error_code = r->error_code;

    // BACA CR2 MENGGUNAKAN REGISTER 64-BIT!
    uint64_t cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

    uint32_t BG    = 0x0000AA;  
    uint32_t FG    = 0xFFFFFF;  
    uint32_t HDR   = 0xAA0000;  
    uint32_t WARN  = 0xFFFF00;  
    uint32_t ERR   = 0xFF4444;  

    fill_screen(BG);

    // --- Header ---
    panic_draw_string("====================================================", 50, 30, FG, BG);
    panic_draw_string("  *** KYUZEN OS — FATAL EXCEPTION / KERNEL PANIC ***", 50, 50, FG, HDR);
    panic_draw_string("====================================================", 50, 70, FG, BG);

    // --- Nomor & Nama Exception ---
    panic_draw_string("INT NUM : ", 50, 100, FG, BG);
    panic_draw_hex(int_num, 170, 100, WARN, BG);

    panic_draw_string("EXCEPTION:", 50, 120, FG, BG);
    const char* exc_name = "UNKNOWN EXCEPTION";
    if (int_num < 15) exc_name = exception_names[int_num];
    panic_draw_string(exc_name, 170, 120, ERR, BG);

    // --- Error Code ---
    panic_draw_string("ERR CODE: ", 50, 145, FG, BG);
    panic_draw_hex(error_code, 170, 145, WARN, BG);

    // --- RIP (Instruksi yang Bikin Crash, OTOMATIS DIDAPAT DARI 64-BIT STACK) ---
    panic_draw_string("CRASH RIP:", 50, 165, FG, BG);
    panic_draw_hex(r->rip, 170, 165, WARN, BG);

    // --- Analisis Page Fault ---
    if (int_num == 14) {
        panic_draw_string("CR2 ADDR: ", 50, 190, FG, BG);
        panic_draw_hex(cr2, 170, 190, WARN, BG);

        panic_draw_string("PG STATUS:", 50, 210, FG, BG);
        // Cek langsung menggunakan 4-Level Paging Limine
        if (paging_is_mapped(cr2))
            panic_draw_string("PRESENT (Ada rute fisik, hak akses ditolak!)", 170, 210, 0xFF8800, BG);
        else
            panic_draw_string("NOT PRESENT (Alamat 64-bit ini belum dipetakan!)", 170, 210, ERR, BG);

        panic_draw_string("PF BITS:  ", 50, 230, FG, BG);
        if (error_code & 1)  panic_draw_string("PROT ", 170, 230, 0xFF8800, BG);
        else                 panic_draw_string("NONP ", 170, 230, ERR, BG);
        if (error_code & 2)  panic_draw_string("WRITE", 250, 230, 0xFF8800, BG);
        else                 panic_draw_string("READ ", 250, 230, FG, BG);
        if (error_code & 4)  panic_draw_string("USER ", 330, 230, WARN, BG);
        else                 panic_draw_string("KERN ", 330, 230, FG, BG);
    }

    // --- Analisis GPF ---
    if (int_num == 13) {
        panic_draw_string("GPF HINT: ", 50, 190, FG, BG);
        if (error_code == 0)
            panic_draw_string("error_code=0: Akses memori non-canonical atau NULL pointer", 170, 190, WARN, BG);
        else {
            int ext  = (error_code & 1) ? 1 : 0;
            int tbl  = (error_code >> 1) & 3;
            int idx  = (error_code >> 3) & 0x1FFF;
            panic_draw_string("SEG IDX:  ", 170, 190, FG, BG);
            panic_draw_dec(idx, 260, 190, WARN, BG);
            panic_draw_string((tbl == 0 ? "GDT" : (tbl == 2 ? "LDT" : "IDT")), 310, 190, WARN, BG);
        }
    }

    panic_draw_string("====================================================", 50, 280, FG, BG);
    panic_draw_string("Sistem dibekukan. Silakan restart dan cek debug info.", 50, 300, FG, BG);

    while (1) { __asm__ volatile("hlt"); }
}

// =======================================================================
// SHORTCUT HANDLER
// =======================================================================
void page_fault_handler(registers_t *r, uint64_t fault_addr) {
    // Alihkan langsung ke handler utama, CR2 otomatis dibaca ulang di sana
    exception_handler(r);
}