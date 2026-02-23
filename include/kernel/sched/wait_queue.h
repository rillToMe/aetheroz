#pragma once
#include <kernel/sched/task.h>

typedef struct wait_queue {
    task_t *head;
    task_t *tail;
} wait_queue_t;

void wait_queue_init(wait_queue_t *q);
void wait_queue_add(wait_queue_t *q, task_t *task);
void wait_queue_wake_one(wait_queue_t *q);
void wait_queue_wake_all(wait_queue_t *q);
