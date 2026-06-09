#ifndef PMM_VALID_H
#define PMM_VALID_H

// PMM Validation Tests — sequential safety checks:
//   Test 1: Double-free detection
//   Test 2: Invalid address detection
//   Test 3: Exhaustion + recovery
//
// Enable: make stress

void valid_start(void);      // Entry: runs all 3 validation tests

#endif
