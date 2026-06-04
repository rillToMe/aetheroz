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

void mouse_handler() {
    uint8_t status = inb(0x64);
    if ((status & 0x01) && (status & 0x20)) {
        mouse_byte[mouse_cycle++] = inb(0x60);
        if (mouse_cycle == 3) {
            mouse_cycle = 0;
            if ((mouse_byte[0] & 0x80) || (mouse_byte[0] & 0x40)) goto end_mouse_irq; 
            
            // HANYA UPDATE KOORDINAT. TANPA MENGHAPUS/MENGGAMBAR!
            mouse_x += mouse_byte[1];
            mouse_y -= mouse_byte[2]; 
            
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x > (int32_t)(fb_width - 12)) mouse_x = fb_width - 12;
            if (mouse_y > (int32_t)(fb_height - 16)) mouse_y = fb_height - 16;
        }
    } else if (status & 0x01) {
        inb(0x60);
    }
end_mouse_irq:
    outb(0xA0, 0x20); outb(0x20, 0x20);
}