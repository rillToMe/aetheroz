#pragma once
#include <stdint.h>
#include <kernel/sched/wait_queue.h>

extern volatile uint64_t system_ticks;
extern wait_queue_t timer_wait_queue;

void timer_init(uint32_t frequency);
void timer_handler(void);
uint64_t timer_ticks(void);
