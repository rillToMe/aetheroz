#include <kernel/video/framebuffer.h>

static struct limine_framebuffer *fb = 0;

static const unsigned char font[128][8] = {
    ['A'] = {0x18,0x24,0x42,0x7E,0x42,0x42,0x42,0x00},
    ['B'] = {0x7C,0x42,0x42,0x7C,0x42,0x42,0x7C,0x00},
    ['O'] = {0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00},
    ['K'] = {0x42,0x44,0x48,0x70,0x48,0x44,0x42,0x00},
    [' '] = {0,0,0,0,0,0,0,0}
};

void framebuffer_init(struct limine_framebuffer_response *response) {
    if (!response || response->framebuffer_count == 0) {
        fb = 0;
        return;
    }
    fb = response->framebuffers[0];
}

static void putpixel(int x, int y, uint32_t color) {
    if (!fb) {
        return;
    }
    uint32_t *pix = (uint32_t *)fb->address;
    pix[y * (int)(fb->pitch / 4) + x] = color;
}

static void draw_char(int x, int y, char c, uint32_t color) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (font[(int)c][row] & (1 << (7 - col))) {
                putpixel(x + col, y + row, color);
            }
        }
    }
}

void fb_draw_string(int x, int y, const char *str, uint32_t color) {
    if (!fb || !str) {
        return;
    }
    int offset = 0;
    while (*str) {
        draw_char(x + offset, y, *str, color);
        offset += 8;
        str++;
    }
}
