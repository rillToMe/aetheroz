#include "kernel.h"
#include "../drivers/vga/vga.h"

void printk_init(void) {
    vga_init();
}

void printk_clear(void) {
    vga_clear();
}

void printk_write(const char* text) {
    vga_print(text);
}
