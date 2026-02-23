#pragma once
#include <kernel/sched/task.h>

void sched_init(void);
void sched_add(task_t *t);
void sched_set_idle(task_t *t);
void sched_force_switch(task_t *t);
void sched_yield(void);
void sched_save_rsp(uint64_t rsp);
uint64_t sched_take_next_rsp(void);
void schedule(void);
void scheduler_post_interrupt(void);
void context_switch(uint64_t *old_rsp, uint64_t *new_rsp);
void task_start(uint64_t rsp);
void task_sleep(uint64_t ticks);
task_t *sched_task_list(void);
extern volatile int need_resched;
