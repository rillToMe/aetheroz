#ifndef LAPIC_H
#define LAPIC_H

#include <stdint.h>
#include "task.h"

#define LAPIC_TIMER_VECTOR      0xF0
#define LAPIC_RESCHEDULE_VECTOR 0xFD

void lapic_init_bsp(void);
void lapic_init_ap(void);
registers_t* lapic_timer_handler(registers_t* regs);
registers_t* lapic_reschedule_handler(registers_t* regs);
void lapic_eoi(void);

uint32_t lapic_id(void);
uint64_t lapic_timer_ticks(void);

void lapic_send_reschedule(uint32_t cpu_index);

#endif
