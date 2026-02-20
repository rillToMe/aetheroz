#ifndef KERNEL_VIDEO_FRAMEBUFFER_H
#define KERNEL_VIDEO_FRAMEBUFFER_H

#include <stdint.h>
#include <kernel/boot/limine.h>

void framebuffer_init(struct limine_framebuffer_response *response);
void fb_draw_string(int x, int y, const char *str, uint32_t color);

#endif
