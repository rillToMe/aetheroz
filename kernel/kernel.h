#pragma once

#include "../include/types.h"

void kernel_main(void);

void printk_init(void);
void printk_clear(void);
void printk_write(const char* text);

void panic(const char* message);
