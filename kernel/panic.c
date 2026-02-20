#include "kernel.h"

void panic(const char* message) {
    if (message) {
        printk_write(message);
    }
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
