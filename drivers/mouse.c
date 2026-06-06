#include "mouse.h"
#include "io.h"
#include <stdint.h>

// Impor kanvas dan resolusi dari kernel.c
extern uint32_t* fb_ptr;
extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t fb_pitch;
// Posisi awal kursor (Tengah layar)
int32_t mouse_x = 512; 
int32_t mouse_y = 384;


// Cetak biru bentuk kursor (0 = Tembus Pandang, 1 = Garis Putih, 2 = Isi Hitam)
const uint8_t cursor_bitmap[16][12] = {
    {1,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,2,2,2,2,1,0,0,0},
    {1,2,2,2,2,2,2,2,2,1,0,0},
    {1,2,2,2,2,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,1,1,1,1,1,1},
    {1,2,2,1,2,2,1,0,0,0,0,0},
    {1,2,1,0,1,2,2,1,0,0,0,0},
    {1,1,0,0,1,2,2,1,0,0,0,0},
    {1,0,0,0,0,1,2,2,1,0,0,0},
    {0,0,0,0,0,0,1,1,0,0,0,0}
};

// --- LOGIKA HARDWARE PS/2 ---
void mouse_wait(uint8_t a_type) {
    uint32_t timeout = 100000;
    if (a_type == 0) {
        while (timeout--) if ((inb(0x64) & 1) == 1) return; // Tunggu data siap
    } else {
        while (timeout--) if ((inb(0x64) & 2) == 0) return; // Tunggu siap ditulis
    }
}

void mouse_write(uint8_t a_write) {
    mouse_wait(1);
    outb(0x64, 0xD4); // Peringatkan controller: data ini untuk mouse!
    mouse_wait(1);
    outb(0x60, a_write);
}

uint8_t mouse_read() {
    mouse_wait(0);
    return inb(0x60);
}

void init_mouse() {
    uint8_t status;
    mouse_wait(1); outb(0x64, 0xA8); 
    mouse_wait(1); outb(0x64, 0x20); 
    mouse_wait(0); status = (inb(0x60) | 2); 
    mouse_wait(1); outb(0x64, 0x60); 
    mouse_wait(1); outb(0x60, status);
    mouse_write(0xF6); mouse_read(); 
    mouse_write(0xF4); mouse_read(); 
    
    // (HAPUS PEMANGGILAN draw_mouse() DARI SINI KARENA AKAN DITANGANI COMPOSITOR)
}

uint8_t mouse_cycle = 0;
int8_t mouse_byte[3];

// Definisi global — di-extern oleh syscall.c untuk polling sederhana
uint8_t mouse_left_clicked = 0;

extern void push_event(uint32_t type, int32_t p1, int32_t p2, int32_t p3);
// KWM V2: intercept mouse events untuk drag & z-index sebelum dikirim ke app
extern int kwm_process_mouse(int32_t mx, int32_t my, uint8_t left_down, uint8_t left_up);

// Pelacak status memori tombol (agar tidak spam klik)
static uint8_t last_left_click  = 0;
static uint8_t last_right_click = 0;

void mouse_handler() {
    uint8_t status = inb(0x64);
    if ((status & 0x01) && (status & 0x20)) {
        mouse_byte[mouse_cycle++] = inb(0x60);
        if (mouse_cycle == 3) {
            mouse_cycle = 0;
            if ((mouse_byte[0] & 0x80) || (mouse_byte[0] & 0x40)) goto end_mouse_irq;

            mouse_x += mouse_byte[1];
            mouse_y -= mouse_byte[2];

            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x > (int32_t)(fb_width - 12))  mouse_x = fb_width - 12;
            if (mouse_y > (int32_t)(fb_height - 16)) mouse_y = fb_height - 16;

            // --- DETEKSI KLIK (edge detect) ---
            uint8_t left_click  = mouse_byte[0] & 0x01;
            uint8_t right_click = (mouse_byte[0] & 0x02) >> 1;

            uint8_t left_down = ( left_click && !last_left_click);  // baru ditekan
            uint8_t left_up   = (!left_click &&  last_left_click);  // baru dilepas

            // --- KWM V2 INTERCEPTION ---
            // kwm_process_mouse menangani drag + Z-index.
            // Return 1 = event "dimakan" KWM, jangan push ke app.
            // Return 0 = teruskan event ke app seperti biasa.
            int kwm_consumed = kwm_process_mouse(mouse_x, mouse_y, left_down, left_up);

            if (!kwm_consumed) {
                // Klik kiri — kirim ke app jika KWM tidak mengonsumsinya
                if (left_click != last_left_click) {
                    // EVENT_MOUSE_CLICK (3) → P1: 0 (kiri), P2: 1=ditekan / 0=dilepas
                    push_event(3, 0, left_click, 0);
                    if (left_click) mouse_left_clicked = 1;
                }
            } else if (left_up) {
                // Selalu kirim mouse-up ke app agar state tombol tidak terjebak "pressed"
                push_event(3, 0, 0, 0);
                mouse_left_clicked = 0;
            }

            // Klik kanan — selalu teruskan ke app (KWM tidak menggunakannya)
            if (right_click != last_right_click) {
                push_event(3, 1, right_click, 0);
                last_right_click = right_click;
            }

            last_left_click = left_click;

            // Pergerakan mouse selalu dikirim agar Window Manager bisa melacaknya
            push_event(2, mouse_x, mouse_y, 0); // EVENT_MOUSE_MOVE (2)
        }
    } else if (status & 0x01) {
        inb(0x60);
    }

end_mouse_irq:
    outb(0xA0, 0x20); outb(0x20, 0x20);
}