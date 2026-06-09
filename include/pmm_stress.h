#ifndef PMM_STRESS_H
#define PMM_STRESS_H

// SMP PMM Stress Test — concurrent alloc/free across CPUs.
// Validates PMM lock correctness under heavy parallel load.
//
// Enable: make stress

void stress_start(void);     // Entry: spawns 4 worker tasks
void stress_task(void);      // Worker function (internal)

#endif
