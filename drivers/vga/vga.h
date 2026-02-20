#pragma once

#include "../../include/types.h"

void vga_init(void);
void vga_clear(void);
void vga_putc(char c);
void vga_print(const char* str);
void vga_set_color(uint8_t fg, uint8_t bg);
