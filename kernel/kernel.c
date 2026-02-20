#include <limine.h>

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[3] = LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_requests_start_marker[4] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request fb_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_requests_end_marker[2] = LIMINE_REQUESTS_END_MARKER;

static const unsigned char font[128][8] = {
    ['A'] = {0x18,0x24,0x42,0x7E,0x42,0x42,0x42,0x00},
    ['B'] = {0x7C,0x42,0x42,0x7C,0x42,0x42,0x7C,0x00},
    ['O'] = {0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00},
    ['K'] = {0x42,0x44,0x48,0x70,0x48,0x44,0x42,0x00},
    [' '] = {0,0,0,0,0,0,0,0}
};

static void putpixel(struct limine_framebuffer *fb, int x, int y, uint32_t color) {
    uint32_t *pix = (uint32_t *)fb->address;
    pix[y * (int)(fb->pitch / 4) + x] = color;
}

static void draw_char(struct limine_framebuffer *fb, int x, int y, char c, uint32_t color) {
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (font[(int)c][row] & (1 << (7 - col))) {
                putpixel(fb, x + col, y + row, color);
            }
        }
    }
}

static void draw_string(struct limine_framebuffer *fb, int x, int y, const char *str, uint32_t color) {
    int offset = 0;
    while (*str) {
        draw_char(fb, x + offset, y, *str, color);
        offset += 8;
        str++;
    }
}

void kernel_main(void) {
    if (!fb_request.response || fb_request.response->framebuffer_count < 1) {
        for (;;) {
        }
    }

    struct limine_framebuffer *fb = fb_request.response->framebuffers[0];
    draw_string(fb, 100, 100, "OK", 0x00FF0000);

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
