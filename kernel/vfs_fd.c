// kernel/vfs_fd.c — Per-task file descriptor layer over KyuzenFS (Fase 5).
// Buffered model: open loads the whole file into a heap buffer; read/write/lseek
// operate on it; close flushes back via kfs_create_file if dirty. One global
// lock guards the table. Device nodes (tty) are unaffected — they keep using
// the fs_node_t path in fs.c.

#include "vfs.h"
#include "task.h"
#include "smp.h"
#include "spinlock.h"
#include "kyuzenfs.h"
#include <stddef.h>

extern void* kmalloc(uint32_t size);
extern void  kfree(void* ptr);
extern void* krealloc(void* ptr, uint32_t old_size, uint32_t new_size);
extern int   kfs_create_file(char* filename, char* data, uint32_t size);
extern void  kfs_delete_file(char* filename);
extern int   kfs_exists(char* filename);
extern uint32_t kfs_get_file_size(char* filename);
extern int   kfs_read_to_buffer(char* filename, char* out_buffer, uint32_t buffer_capacity);

typedef struct {
    int      used;
    int      owner;                  // task id
    char     path[VFS_MAX_PATH];
    uint8_t* buf;
    uint32_t size;                   // valid bytes in buf
    uint32_t cap;                    // allocated bytes
    uint32_t pos;
    uint32_t flags;
    int      dirty;
} vfs_file_t;

static vfs_file_t fds[MAX_TASKS * VFS_MAX_FDS];  // pool; index = task_id*VFS_MAX_FDS + fd
static spinlock_t vfs_lock = SPINLOCK_INIT;

void vfs_init(void) {
    uint64_t f = spinlock_lock_irqsave(&vfs_lock);
    for (int i = 0; i < (int)(sizeof(fds) / sizeof(fds[0])); i++) {
        fds[i].used = 0;
        fds[i].buf  = NULL;
    }
    spinlock_unlock_irqrestore(&vfs_lock, f);
}

static void path_copy(char* dst, const char* src) {
    int i = 0;
    while (i < VFS_MAX_PATH - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

// Map a per-task fd number to a pool slot. Each task owns a contiguous window
// [task_id*VFS_MAX_FDS, +VFS_MAX_FDS). Caller holds vfs_lock.
static int slot_of(int task_id, int fd) {
    if (task_id < 0 || task_id >= MAX_TASKS) return -1;
    if (fd < 0 || fd >= VFS_MAX_FDS) return -1;
    return task_id * VFS_MAX_FDS + fd;
}

static vfs_file_t* resolve(int fd) {
    int task_id = smp_current_task_id();
    int s = slot_of(task_id, fd);
    if (s < 0) return NULL;
    vfs_file_t* vf = &fds[s];
    if (!vf->used || vf->owner != task_id) return NULL;
    return vf;
}

int vfs_open(const char* path, uint32_t flags) {
    if (path == NULL || path[0] == '\0') return -1;

    int task_id = smp_current_task_id();
    if (task_id < 0 || task_id >= MAX_TASKS) return -1;

    int exists = kfs_exists((char*)path);
    if (!exists && !(flags & VFS_O_CREAT)) return -1;

    uint32_t fsize = exists ? kfs_get_file_size((char*)path) : 0;
    uint32_t cap   = (fsize < 64) ? 64 : fsize;

    uint8_t* buf = (uint8_t*)kmalloc(cap);
    if (!buf) return -1;

    if (exists && !(flags & VFS_O_TRUNC) && fsize > 0) {
        if (!kfs_read_to_buffer((char*)path, (char*)buf, cap)) { kfree(buf); return -1; }
    } else {
        fsize = 0;   // O_TRUNC or brand-new file starts empty
    }

    uint64_t f = spinlock_lock_irqsave(&vfs_lock);
    int base = task_id * VFS_MAX_FDS;
    int fd = -1;
    for (int i = 0; i < VFS_MAX_FDS; i++) {
        if (!fds[base + i].used) { fd = i; break; }
    }
    if (fd < 0) {
        spinlock_unlock_irqrestore(&vfs_lock, f);
        kfree(buf);
        return -1;   // fd table full
    }

    vfs_file_t* vf = &fds[base + fd];
    vf->used  = 1;
    vf->owner = task_id;
    path_copy(vf->path, path);
    vf->buf   = buf;
    vf->size  = fsize;
    vf->cap   = cap;
    vf->pos   = (flags & VFS_O_APPEND) ? fsize : 0;
    vf->flags = flags;
    vf->dirty = (!exists || (flags & VFS_O_TRUNC)) ? 1 : 0;
    spinlock_unlock_irqrestore(&vfs_lock, f);
    return fd;
}

int vfs_read(int fd, void* buf, uint32_t count) {
    if (buf == NULL) return -1;
    uint64_t f = spinlock_lock_irqsave(&vfs_lock);
    vfs_file_t* vf = resolve(fd);
    if (!vf || (vf->flags & VFS_O_WRONLY)) {
        spinlock_unlock_irqrestore(&vfs_lock, f);
        return -1;
    }
    uint32_t avail = (vf->pos < vf->size) ? (vf->size - vf->pos) : 0;
    uint32_t n = (count < avail) ? count : avail;
    for (uint32_t i = 0; i < n; i++) ((uint8_t*)buf)[i] = vf->buf[vf->pos + i];
    vf->pos += n;
    spinlock_unlock_irqrestore(&vfs_lock, f);
    return (int)n;
}

// Grow the buffer to hold at least `need` bytes. Caller holds vfs_lock.
static int ensure_cap(vfs_file_t* vf, uint32_t need) {
    if (need <= vf->cap) return 0;
    uint32_t newcap = vf->cap ? vf->cap : 64;
    while (newcap < need) newcap *= 2;
    uint8_t* nb = (uint8_t*)krealloc(vf->buf, vf->cap, newcap);
    if (!nb) return -1;
    vf->buf = nb;
    vf->cap = newcap;
    return 0;
}

int vfs_write(int fd, const void* buf, uint32_t count) {
    if (buf == NULL) return -1;
    uint64_t f = spinlock_lock_irqsave(&vfs_lock);
    vfs_file_t* vf = resolve(fd);
    if (!vf || !(vf->flags & (VFS_O_WRONLY | VFS_O_RDWR))) {
        spinlock_unlock_irqrestore(&vfs_lock, f);
        return -1;
    }
    if (vf->flags & VFS_O_APPEND) vf->pos = vf->size;
    if (ensure_cap(vf, vf->pos + count) != 0) {
        spinlock_unlock_irqrestore(&vfs_lock, f);
        return -1;
    }
    for (uint32_t i = 0; i < count; i++) vf->buf[vf->pos + i] = ((const uint8_t*)buf)[i];
    vf->pos += count;
    if (vf->pos > vf->size) vf->size = vf->pos;
    vf->dirty = 1;
    spinlock_unlock_irqrestore(&vfs_lock, f);
    return (int)count;
}

int vfs_lseek(int fd, int32_t offset, int whence) {
    uint64_t f = spinlock_lock_irqsave(&vfs_lock);
    vfs_file_t* vf = resolve(fd);
    if (!vf) { spinlock_unlock_irqrestore(&vfs_lock, f); return -1; }

    int64_t base = (whence == VFS_SEEK_CUR) ? (int64_t)vf->pos
                 : (whence == VFS_SEEK_END) ? (int64_t)vf->size
                 : 0;
    int64_t np = base + offset;
    if (np < 0) { spinlock_unlock_irqrestore(&vfs_lock, f); return -1; }
    vf->pos = (uint32_t)np;
    spinlock_unlock_irqrestore(&vfs_lock, f);
    return (int)vf->pos;
}

// Flush a dirty buffer to disk. Caller holds vfs_lock; kfs has its own lock and
// the two are never nested in the reverse order.
static void flush_locked(vfs_file_t* vf) {
    if (vf->dirty) {
        // kfs_create_file refuses an existing name, so replace: delete then
        // recreate with the current buffer contents.
        if (kfs_exists(vf->path)) kfs_delete_file(vf->path);
        kfs_create_file(vf->path, (char*)vf->buf, vf->size);
        vf->dirty = 0;
    }
}

int vfs_close(int fd) {
    uint64_t f = spinlock_lock_irqsave(&vfs_lock);
    vfs_file_t* vf = resolve(fd);
    if (!vf) { spinlock_unlock_irqrestore(&vfs_lock, f); return -1; }
    flush_locked(vf);
    kfree(vf->buf);
    vf->buf  = NULL;
    vf->used = 0;
    spinlock_unlock_irqrestore(&vfs_lock, f);
    return 0;
}

void vfs_close_all(int task_id) {
    if (task_id < 0 || task_id >= MAX_TASKS) return;
    uint64_t f = spinlock_lock_irqsave(&vfs_lock);
    int base = task_id * VFS_MAX_FDS;
    for (int i = 0; i < VFS_MAX_FDS; i++) {
        vfs_file_t* vf = &fds[base + i];
        if (vf->used) {
            flush_locked(vf);
            kfree(vf->buf);
            vf->buf  = NULL;
            vf->used = 0;
        }
    }
    spinlock_unlock_irqrestore(&vfs_lock, f);
}
