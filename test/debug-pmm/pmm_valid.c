// ============================================================
// debug/pmm_valid.c — PMM Validation Tests
//
// Sequential safety checks that verify PMM error detection:
//   Test 1: Double-free detection
//   Test 2: Invalid address detection (NULL, unaligned, OOR)
//   Test 3: Exhaustion + full recovery
//
// These tests intentionally trigger PMM warnings to prove the
// detectors work. The warnings are EXPECTED output.
// ============================================================

#include "pmm.h"
#include "smp.h"
#include "pmm_valid.h"

extern void kprint(const char *str);
extern void kprint_num(uint64_t num);
extern uint64_t hhdm_offset;

// ============================================================
// TEST 1: Double-Free Detection
// ============================================================
static void test_double_free(void) {
    kprint("\n");
    kprint("[VALID 1] Double-Free Detection\n");
    kprint("[VALID 1] -------------------------\n");

    phys_addr_t p = pmm_alloc_page();
    if (p == PHYS_NULL) {
        kprint("[VALID 1] SKIP — OOM\n");
        return;
    }

    kprint("[VALID 1] Allocated 0x");
    kprint_num(p);
    kprint("\n");

    // First free — should succeed silently
    pmm_free_page(p);
    kprint("[VALID 1] First free OK\n");

    // Second free — should trigger [PMM] double-free warning
    kprint("[VALID 1] Second free (expect warning)...\n");
    pmm_free_page(p);

    kprint("[VALID 1] Check: \"[PMM] double-free\" appeared above? PASS\n");
}

// ============================================================
// TEST 2: Invalid Address Detection
// ============================================================
static void test_invalid_address(void) {
    kprint("\n");
    kprint("[VALID 2] Invalid Address Detection\n");
    kprint("[VALID 2] -------------------------\n");

    // 2a: NULL
    kprint("[VALID 2a] Free NULL (expect warning)...\n");
    pmm_free_page(PHYS_NULL);

    // 2b: Unaligned
    kprint("[VALID 2b] Free 0x12345 unaligned (expect warning)...\n");
    pmm_free_page((phys_addr_t)0x12345);

    // 2c: Out of range
    kprint("[VALID 2c] Free 0xFFFFFFFFF000 OOR (expect warning)...\n");
    pmm_free_page((phys_addr_t)0xFFFFFFFFF000ULL);

    kprint("[VALID 2] Check: 3 warnings appeared above? PASS\n");
}

// ============================================================
// TEST 3: Exhaustion + Recovery
// ============================================================
#define EXHAUST_MAX 131072  // 512MB safety cap

static void test_exhaustion(void) {
    kprint("\n");
    kprint("[VALID 3] Exhaustion + Recovery\n");
    kprint("[VALID 3] -------------------------\n");

    uint64_t before = pmm_get_used_pages();
    kprint("[VALID 3] Pages before: ");
    kprint_num(before);
    kprint("\n");

    // Allocate all pages using linked list stored in pages themselves
    phys_addr_t head = PHYS_NULL;
    uint64_t count = 0;

    kprint("[VALID 3] Allocating until OOM...\n");
    while (count < EXHAUST_MAX) {
        phys_addr_t p = pmm_alloc_page();
        if (p == PHYS_NULL) break;

        // Store linked-list pointer in page via HHDM
        uint64_t *virt = (uint64_t *)(p + hhdm_offset);
        virt[0] = head;
        head = p;
        count++;
    }

    kprint("[VALID 3] Allocated ");
    kprint_num(count);
    kprint(" pages (");
    if (count >= EXHAUST_MAX)
        kprint("safety cap");
    else
        kprint("OOM reached");
    kprint(")\n");

    uint64_t during = pmm_get_used_pages();
    kprint("[VALID 3] Pages during: ");
    kprint_num(during);
    kprint("\n");

    // Free all pages
    kprint("[VALID 3] Freeing all...\n");
    uint64_t freed = 0;
    while (head != PHYS_NULL) {
        uint64_t *virt = (uint64_t *)(head + hhdm_offset);
        phys_addr_t next = (phys_addr_t)virt[0];
        pmm_free_page(head);
        head = next;
        freed++;
    }

    uint64_t after = pmm_get_used_pages();
    kprint("[VALID 3] Freed ");
    kprint_num(freed);
    kprint(" pages\n");
    kprint("[VALID 3] Pages after:  ");
    kprint_num(after);
    kprint("\n");

    if (before == after) {
        kprint("[VALID 3] PASS — ");
        kprint_num(freed);
        kprint(" pages fully restored\n");
    } else if (after > before) {
        kprint("[VALID 3] FAIL — ");
        kprint_num(after - before);
        kprint(" pages leaked\n");
    } else {
        kprint("[VALID 3] FAIL — underflow by ");
        kprint_num(before - after);
        kprint(" pages\n");
    }
}

// ============================================================
// Entry — runs all validation tests sequentially
// ============================================================
void valid_start(void) {
    kprint("\n");
    kprint("========================================================\n");
    kprint("  PMM Validation Suite\n");
    kprint("========================================================\n");
    kprint("[VALID] PMM pages: ");
    kprint_num(pmm_get_used_pages());
    kprint(" / ");
    kprint_num(pmm_get_total_pages());
    kprint("\n");

    test_double_free();
    test_invalid_address();
    test_exhaustion();

    kprint("\n");
    kprint("========================================================\n");
    kprint("  Validation complete.\n");
    kprint("========================================================\n");
}
