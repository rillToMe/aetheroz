#include <kernel/sched/task.h>
#include <kernel/memory/heap.h>
#include <kernel/memory/vmm.h>

void task_trampoline(void);

static task_t *current_task = 0;
static int next_pid = 1;

task_t *task_create(void (*entry)(void)) {
    task_t *t = kmalloc(sizeof(task_t));
    if (!t) {
        return 0;
    }
    t->as = address_space_kernel();
    if (!t->as) {
        return 0;
    }
    t->pid = next_pid++;
    t->state = TASK_READY;
    t->wakeup_tick = 0;
    t->next = 0;
    t->next_wait = 0;
    t->waiting_on = 0;
    for (int i = 0; i < MAX_FDS; i++) {
        t->fd_table[i] = 0;
    }
    t->stack = kmalloc(TASK_STACK_SIZE);
    if (!t->stack) {
        return 0;
    }
    uint64_t *sp = (uint64_t *)(t->stack + TASK_STACK_SIZE);
    *(--sp) = (uint64_t)(uintptr_t)entry;
    *(--sp) = (uint64_t)(uintptr_t)task_trampoline;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    *(--sp) = 0;
    t->rsp = (uint64_t)(uintptr_t)sp;
    return t;
}

task_t *task_current(void) {
    return current_task;
}

void task_set_current(task_t *t) {
    current_task = t;
    if (t && t->as) {
        address_space_switch(t->as);
    }
}
