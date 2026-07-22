#ifndef HEAP_STRESS_TEST_H
#define HEAP_STRESS_TEST_H

// Heap stress + regression harness. Verifies two fixes:
//   1. syscall 13 / kfs_read_to_buffer capacity guard (no neighbor overflow)
//   2. heap_block_t magic canary detects corruption at its source
// Enable: make heap-stress. Compiled out of normal build (guarded HEAP_STRESS_TEST).
void test_heap_stress_run_all(void);

#endif
