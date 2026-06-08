#ifndef LAPIC_H
#define LAPIC_H

#include <stdint.h>

#define LAPIC_TIMER_VECTOR 0xF0

void lapic_init_bsp(void);
void lapic_init_ap(void);
void lapic_timer_handler(void);
void lapic_eoi(void);

uint32_t lapic_id(void);
uint64_t lapic_timer_ticks(void);

#endif
