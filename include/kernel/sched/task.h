#pragma once
#include <stdint.h>
#include <kernel/fs/vfs.h>
#include <kernel/memory/vmm.h>

#define MAX_FDS 32
#define TASK_STACK_SIZE 0x4000
#define TASK_DEFAULT_QUANTUM 5

struct wait_queue;

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_WAITING,
    TASK_DEAD
} task_state_t;

typedef struct task {
    uint64_t rsp;
    uint8_t *stack;
    int pid;
    task_state_t state;
    uint64_t wakeup_tick;
    uint32_t quantum;
    struct task *next;
    struct task *next_wait;
    struct wait_queue *waiting_on;
    file_t *fd_table[MAX_FDS];
    address_space_t *as;
} task_t;

task_t *task_create(void (*entry)(void));
task_t *task_current(void);
void task_set_current(task_t *t);
