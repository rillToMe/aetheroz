#pragma once
#include <stdint.h>
#include <kernel/fs/vfs.h>

#ifndef MAX_FDS
#define MAX_FDS 32
#endif

int fd_open(vnode_t *node);
int fd_read(int fd, void *buf, uint64_t size);
int fd_write(int fd, const void *buf, uint64_t size);
void fd_close(int fd);
int fd_set(int fd, vnode_t *node);
