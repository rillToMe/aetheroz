#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include "fs.h"

// ============================================================
// VFS fd layer (Fase 5) — per-task file descriptors over KyuzenFS.
//
// KyuzenFS is whole-file (create writes all, read reads all). The fd layer
// buffers an open file in the heap: open loads it, read/write/lseek work on
// the buffer, close flushes back if dirty. Descriptors are per task (keyed by
// smp_current_task_id), so different tasks get independent fd namespaces.
// ============================================================

#define VFS_MAX_FDS      16     // per task
#define VFS_MAX_PATH     23     // matches kfs filename limit (22 + null)

// open flags
#define VFS_O_RDONLY  0x0
#define VFS_O_WRONLY  0x1
#define VFS_O_RDWR    0x2
#define VFS_O_CREAT   0x4
#define VFS_O_TRUNC   0x8
#define VFS_O_APPEND  0x10

// lseek whence
#define VFS_SEEK_SET  0
#define VFS_SEEK_CUR  1
#define VFS_SEEK_END  2

void vfs_init(void);

// Return fd (>= 0) or negative on error. Each fd is valid only for the task
// that opened it.
int  vfs_open(const char* path, uint32_t flags);
int  vfs_read(int fd, void* buf, uint32_t count);
int  vfs_write(int fd, const void* buf, uint32_t count);
int  vfs_lseek(int fd, int32_t offset, int whence);
int  vfs_close(int fd);

// Release every fd owned by a task (called on task_exit to avoid leaks).
void vfs_close_all(int task_id);

#endif // VFS_H
