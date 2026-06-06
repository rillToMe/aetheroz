#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void init_timer(uint32_t freq);
uint32_t get_uptime(void);
uint32_t get_cpu_usage(void);
#endif