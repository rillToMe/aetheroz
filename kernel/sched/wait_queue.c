#include <kernel/sched/wait_queue.h>
#include <kernel/sched/sched.h>

void wait_queue_init(wait_queue_t *q) {
    if (!q) {
        return;
    }
    q->head = 0;
    q->tail = 0;
}

void wait_queue_add(wait_queue_t *q, task_t *task) {
    if (!q || !task) {
        return;
    }
    if (task->state == TASK_WAITING || task->next_wait) {
        return;
    }
    task->next_wait = 0;
    if (!q->head) {
        q->head = task;
        q->tail = task;
    } else {
        q->tail->next_wait = task;
        q->tail = task;
    }
    task->state = TASK_WAITING;
}

void wait_queue_wake_one(wait_queue_t *q) {
    if (!q || !q->head) {
        return;
    }
    task_t *t = q->head;
    q->head = t->next_wait;
    if (!q->head) {
        q->tail = 0;
    }
    t->next_wait = 0;
    t->waiting_on = 0;
    t->state = TASK_READY;
}

void wait_queue_wake_all(wait_queue_t *q) {
    if (!q) {
        return;
    }
    while (q->head) {
        wait_queue_wake_one(q);
    }
}
