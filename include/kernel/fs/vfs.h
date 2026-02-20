#pragma once
#include <stdint.h>

typedef struct vnode vnode_t;
typedef struct file file_t;

typedef struct file_ops {
    int (*read)(file_t *file, void *buf, uint64_t size);
    int (*write)(file_t *file, const void *buf, uint64_t size);
    void (*close)(file_t *file);
} file_ops_t;

struct vnode {
    const char *name;
    file_ops_t *ops;
    void *internal;
    struct vnode *parent;
    struct vnode *child;
    struct vnode *next;
};

struct file {
    vnode_t *node;
    uint64_t position;
};

file_t *vfs_open(vnode_t *node);
int vfs_read(file_t *file, void *buf, uint64_t size);
int vfs_write(file_t *file, const void *buf, uint64_t size);
void vfs_close(file_t *file);
void vfs_add_child(vnode_t *parent, vnode_t *child);
vnode_t *vfs_lookup(vnode_t *parent, const char *name);
vnode_t *vfs_resolve(vnode_t *root, const char *path);
