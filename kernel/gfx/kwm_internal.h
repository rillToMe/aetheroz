#ifndef KWM_INTERNAL_H
#define KWM_INTERNAL_H

// State internal KWM yang dibagi antara kwm.c dan compositor.c.
// JANGAN di-include dari luar kernel/gfx/ — akses publik lewat include/kwm.h.

#include <stdint.h>
#include "spinlock.h"

#define MAX_WINDOWS 16

typedef struct {
    uint8_t active;
    int32_t x, y;
    uint32_t width, height;
    uint32_t* canvas;
    uint32_t z_index;
    int32_t owner_task;   // FIX_004: task pemilik window (-1 = tidak ada)
} kwm_window_t;

extern kwm_window_t kwm_windows[MAX_WINDOWS];
extern uint32_t next_z_index;
extern spinlock_t kwm_lock;

#endif
