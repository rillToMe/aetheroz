#ifndef DEBUG_PMM_STRESS_H
#define DEBUG_PMM_STRESS_H

// SMP PMM stress test — validates alloc/free under concurrent CPU load.
// Enable via Makefile: make stress
//   Adds -DSTRESS_TEST to CFLAGS, includes debug/ in build.

void stress_start(void);     // Entry: spawns worker tasks
void stress_task(void);      // Worker: runs alloc/free cycles

#endif
