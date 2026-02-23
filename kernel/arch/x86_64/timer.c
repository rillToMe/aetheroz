#include <kernel/arch/io.h>
#include <kernel/arch/x86_64/timer.h>
#include <kernel/sched/sched.h>
#include <kernel/arch/serial.h>

volatile uint64_t system_ticks = 0;
wait_queue_t timer_wait_queue = { 0 };

static void timer_log(const char *text) {
    if (!text) {
        return;
    }
    while (*text) {
        serial_write(*text++);
    }
}

static char timer_task_marker(task_t *t) {
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

void timer_init(uint32_t frequency) {
    if (frequency == 0) {
        return;
    }
    uint32_t divisor = 1193182u / frequency;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
    system_ticks = 0;
    wait_queue_init(&timer_wait_queue);
}

void timer_handler(void) {
    system_ticks++;
    int woke = 0;
    task_t *prev = 0;
    task_t *iter = timer_wait_queue.head;
    while (iter) {
        if (iter->wakeup_tick <= system_ticks) {
            task_t *ready = iter;
            iter = iter->next_wait;
            if (prev) {
                prev->next_wait = iter;
            } else {
                timer_wait_queue.head = iter;
            }
            if (ready == timer_wait_queue.tail) {
                timer_wait_queue.tail = prev;
            }
            ready->next_wait = 0;
            ready->waiting_on = 0;
            ready->state = TASK_READY;
            timer_log("[WAKE] ");
            serial_write(timer_task_marker(ready));
            serial_write('\n');
            woke = 1;
        } else {
            prev = iter;
            iter = iter->next_wait;
        }
    }
    if (woke) {
        need_resched = 1;
    }
    task_t *current = task_current();
    if (current) {
        if (current->quantum > 0) {
            current->quantum--;
        }
        if (current->quantum == 0) {
            current->quantum = TASK_DEFAULT_QUANTUM;
            need_resched = 1;
        }
    }
}

uint64_t timer_ticks(void) {
    return system_ticks;
}
