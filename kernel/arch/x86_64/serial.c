#include <kernel/arch/io.h>
#include <kernel/arch/serial.h>

void serial_init(void) {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}

void serial_write(char c) {
    while (!(inb(0x3F8 + 5) & 0x20)) {
    }
    outb(0x3F8, (uint8_t)c);
}

void serial_write_hex64(uint64_t value) {
    for (int shift = 60; shift >= 0; shift -= 4) {
        uint8_t nibble = (uint8_t)((value >> shift) & 0xF);
        if (nibble < 10) {
            serial_write((char)('0' + nibble));
        } else {
            serial_write((char)('A' + (nibble - 10)));
        }
    }
}
