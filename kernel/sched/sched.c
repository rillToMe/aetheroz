#include <kernel/sched/sched.h>
#include <kernel/arch/serial.h>
#include <kernel/arch/x86_64/timer.h>
#include <kernel/sched/wait_queue.h>

static task_t *task_list = 0;
static task_t *idle_task = 0;
static task_t *next_task = 0;
static uint64_t switch_count = 0;
volatile int need_resched = 0;

static void sched_log(const char *text) {
    if (!text) {
        return;
    }
    while (*text) {
        serial_write(*text++);
    }
}

static char sched_task_marker(task_t *t) {
    if (!t) {
        return '?';
    }
    if (t->pid == 1) {
        return 'A';
    }
    if (t->pid == 2) {
        return 'B';
    }
    return '?';
}

void sched_init(void) {
    task_list = 0;
    idle_task = 0;
    next_task = 0;
}

void sched_add(task_t *t) {
    if (!t) {
        return;
    }
    t->quantum = TASK_DEFAULT_QUANTUM;
    if (!task_list) {
        task_list = t;
        t->next = t;
    } else {
        t->next = task_list->next;
        task_list->next = t;
    }
}

void sched_set_idle(task_t *t) {
    idle_task = t;
    if (idle_task) {
        idle_task->state = TASK_READY;
    }
}

void sched_force_switch(task_t *t) {
    if (!t) {
        return;
    }
    next_task = t;
    need_resched = 1;
}

static task_t *sched_pick_next(task_t *current) {
    task_t *start = (current && current->next) ? current->next : task_list;
    if (start) {
        task_t *iter = start;
        do {
            if (iter->state == TASK_READY) {
                return iter;
            }
            iter = iter->next;
        } while (iter && iter != start);
    }
    if (idle_task && idle_task->state == TASK_READY) {
        return idle_task;
    }
    if (current && current->state == TASK_READY) {
        return current;
    }
    return idle_task ? idle_task : current;
}

void sched_save_rsp(uint64_t rsp) {
    task_t *t = task_current();
    if (t) {
        t->rsp = rsp;
    }
}

uint64_t sched_take_next_rsp(void) {
    if (!next_task) {
        return 0;
    }
    uint64_t rsp = next_task->rsp;
    if (next_task->stack) {
        uint64_t stack_base = (uint64_t)(uintptr_t)next_task->stack;
        uint64_t stack_top = stack_base + TASK_STACK_SIZE;
        if (rsp < stack_base || rsp >= stack_top) {
            rsp = stack_top - (19 * 8);
            next_task->rsp = rsp;
        }
    }
    task_set_current(next_task);
    next_task = 0;
    return rsp;
}

void schedule(void) {
    task_t *current = task_current();
    if (!current) {
        return;
    }
    if (current->pid == 0 && !next_task) {
        return;
    }
    task_t *next = next_task ? next_task : sched_pick_next(current);
    next_task = 0;
    if (!next || next == current) {
        return;
    }
    switch_count++;
    if (current->state == TASK_RUNNING) {
        current->state = TASK_READY;
    }
    next->state = TASK_RUNNING;
    if (next->quantum == 0) {
        next->quantum = TASK_DEFAULT_QUANTUM;
    }
    task_set_current(next);
    context_switch(&current->rsp, &next->rsp);
}

void sched_yield(void) {
    need_resched = 1;
    scheduler_post_interrupt();
}

void scheduler_post_interrupt(void) {
    if (!need_resched) {
        return;
    }
    need_resched = 0;
    __asm__ __volatile__("cli");
    schedule();
    __asm__ __volatile__("sti");
}

void task_sleep(uint64_t ticks) {
    if (ticks == 0) {
        return;
    }
    task_t *current = task_current();
    if (!current) {
        return;
    }
    current->wakeup_tick = system_ticks + ticks;
    current->waiting_on = &timer_wait_queue;
    wait_queue_add(&timer_wait_queue, current);
    sched_log("[SLEEP] ");
    serial_write(sched_task_marker(current));
    serial_write('\n');
    schedule();
}

task_t *sched_task_list(void) {
    return task_list;
}
