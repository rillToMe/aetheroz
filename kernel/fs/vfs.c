#include <kernel/fs/vfs.h>
#include <kernel/memory/heap.h>

static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return (*a == 0 && *b == 0);
}

file_t *vfs_open(vnode_t *node) {
    if (!node) {
        return 0;
    }
    file_t *f = kmalloc(sizeof(file_t));
    if (!f) {
        return 0;
    }
    f->node = node;
    f->position = 0;
    return f;
}

int vfs_read(file_t *file, void *buf, uint64_t size) {
    if (!file || !file->node || !file->node->ops || !file->node->ops->read) {
        return -1;
    }
    return file->node->ops->read(file, buf, size);
}

int vfs_write(file_t *file, const void *buf, uint64_t size) {
    if (!file || !file->node || !file->node->ops || !file->node->ops->write) {
        return -1;
    }
    return file->node->ops->write(file, buf, size);
}

void vfs_close(file_t *file) {
    if (!file) {
        return;
    }
    if (file->node && file->node->ops && file->node->ops->close) {
        file->node->ops->close(file);
    }
}

void vfs_add_child(vnode_t *parent, vnode_t *child) {
    if (!parent || !child) {
        return;
    }
    child->parent = parent;
    child->next = parent->child;
    parent->child = child;
}

vnode_t *vfs_lookup(vnode_t *parent, const char *name) {
    if (!parent || !name) {
        return 0;
    }
    vnode_t *cur = parent->child;
    while (cur) {
        if (str_eq(cur->name, name)) {
            return cur;
        }
        cur = cur->next;
    }
    return 0;
}

vnode_t *vfs_resolve(vnode_t *root, const char *path) {
    if (!root || !path || path[0] != '/') {
        return 0;
    }
    vnode_t *current = root;
    path++;
    char name[64];
    int idx = 0;
    while (1) {
        if (*path == '/' || *path == 0) {
            name[idx] = 0;
            if (idx > 0) {
                current = vfs_lookup(current, name);
                if (!current) {
                    return 0;
                }
            }
            idx = 0;
            if (*path == 0) {
                break;
            }
        } else {
            if (idx < 63) {
                name[idx++] = *path;
            }
        }
        path++;
    }
    return current;
}
