#include <kernel/sys/syscall.h>
#include <kernel/fs/fd.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/task.h>

#define SYS_WRITE 1
#define SYS_EXIT  2
#define SYS_SLEEP 3
#define SYS_RETURN 4

extern volatile uint64_t syscall_return_to_kernel;

static uint64_t sys_write(uint64_t fd,
                          uint64_t buf,
                          uint64_t size,
                          uint64_t unused) {
    (void)unused;
    return (uint64_t)fd_write((int)fd, (const void *)buf, size);
}

static uint64_t sys_exit(uint64_t code,
                         uint64_t unused1,
                         uint64_t unused2,
                         uint64_t unused3) {
    (void)code;
    (void)unused1;
    (void)unused2;
    (void)unused3;
    for (;;) {
        __asm__ __volatile__("hlt");
    }
    return 0;
}

static uint64_t sys_return(uint64_t unused0,
                           uint64_t unused1,
                           uint64_t unused2,
                           uint64_t unused3) {
    (void)unused0;
    (void)unused1;
    (void)unused2;
    (void)unused3;
    syscall_return_to_kernel = 1;
    return 0;
}

static uint64_t sys_sleep(uint64_t ticks,
                          uint64_t unused1,
                          uint64_t unused2,
                          uint64_t unused3) {
    (void)unused1;
    (void)unused2;
    (void)unused3;
    task_sleep(ticks);
    return 0;
}

#define MAX_SYSCALLS 64
static syscall_t syscall_table[MAX_SYSCALLS];

void syscall_init(void) {
    for (int i = 0; i < MAX_SYSCALLS; i++) {
        syscall_table[i] = 0;
    }
    syscall_table[SYS_WRITE] = sys_write;
    syscall_table[SYS_EXIT] = sys_exit;
    syscall_table[SYS_SLEEP] = sys_sleep;
    syscall_table[SYS_RETURN] = sys_return;
}

uint64_t syscall_dispatch(uint64_t num,
                          uint64_t arg1,
                          uint64_t arg2,
                          uint64_t arg3,
                          uint64_t arg4) {
    if (num >= MAX_SYSCALLS) {
        return (uint64_t)-1;
    }
    syscall_t fn = syscall_table[num];
    if (!fn) {
        return (uint64_t)-1;
    }
    return fn(arg1, arg2, arg3, arg4);
}
