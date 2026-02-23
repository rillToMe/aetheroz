#include <kernel/fs/fd.h>
#include <kernel/sched/task.h>
#include <kernel/sched/sched.h>

int fd_open(vnode_t *node) {
    task_t *t = task_current();
    if (!t) {
        return -1;
    }
    for (int i = 0; i < MAX_FDS; i++) {
        if (!t->fd_table[i]) {
            t->fd_table[i] = vfs_open(node);
            if (!t->fd_table[i]) {
                return -1;
            }
            return i;
        }
    }
    return -1;
}

int fd_read(int fd, void *buf, uint64_t size) {
    task_t *t = task_current();
    if (!t || fd < 0 || fd >= MAX_FDS || !t->fd_table[fd]) {
        return -1;
    }
    int result = vfs_read(t->fd_table[fd], buf, size);
    scheduler_post_interrupt();
    return result;
}

int fd_write(int fd, const void *buf, uint64_t size) {
    task_t *t = task_current();
    if (!t || fd < 0 || fd >= MAX_FDS || !t->fd_table[fd]) {
        return -1;
    }
    int result = vfs_write(t->fd_table[fd], buf, size);
    scheduler_post_interrupt();
    return result;
}

void fd_close(int fd) {
    task_t *t = task_current();
    if (!t || fd < 0 || fd >= MAX_FDS || !t->fd_table[fd]) {
        return;
    }
    vfs_close(t->fd_table[fd]);
    t->fd_table[fd] = 0;
}

int fd_set(int fd, vnode_t *node) {
    task_t *t = task_current();
    if (!t || fd < 0 || fd >= MAX_FDS) {
        return -1;
    }
    if (t->fd_table[fd]) {
        vfs_close(t->fd_table[fd]);
        t->fd_table[fd] = 0;
    }
    if (!node) {
        return -1;
    }
    t->fd_table[fd] = vfs_open(node);
    if (!t->fd_table[fd]) {
        return -1;
    }
    return fd;
}
