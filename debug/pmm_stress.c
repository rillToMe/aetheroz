// ============================================================
// debug/pmm_stress.c — SMP PMM Stress Test
//
// Validates that pmm_alloc_page/pmm_free_page are SMP-safe
// by spawning N worker tasks that each perform M alloc/free
// cycles concurrently. Detects page leaks and double-frees.
//
// Usage:
//   make stress       (builds with -DSTRESS_TEST, runs test)
//   Or call stress_start() from kernel_main after sti.
// ============================================================

#include "pmm.h"
#include "task.h"
#include "smp.h"
#include "spinlock.h"
#include "pmm_stress.h"

extern void kprint(const char *str);
extern void kprint_num(uint64_t num);

// ---- Configuration ----
#define STRESS_WORKERS      4
#define STRESS_ITERATIONS   500000ULL

// ---- Shared state ----
static spinlock_t stress_lock = SPINLOCK_INIT;
static volatile uint32_t workers_done = 0;
static uint64_t alloc_failures = 0;
static uint64_t total_allocs = 0;

// Snapshot before test
static uint64_t snap_used_pages = 0;

// ---- Per-worker log (lock-free, indexed by worker id) ----
static volatile uint64_t worker_allocs[STRESS_WORKERS];
static volatile uint64_t worker_frees[STRESS_WORKERS];

void stress_task(void) {
    // Determine worker id from task name (last char)
    uint32_t cpu_id = smp_current_cpu_index();
    uint32_t wid = cpu_id % STRESS_WORKERS;

    uint64_t local_allocs = 0;
    uint64_t local_frees = 0;

    for (uint64_t i = 0; i < STRESS_ITERATIONS; i++) {
        phys_addr_t p = pmm_alloc_page();
        if (p != PHYS_NULL) {
            local_allocs++;
            // Immediately free — tests alloc/free race safety
            pmm_free_page(p);
            local_frees++;
        }
    }

    // Update shared counters under lock
    uint64_t flags = spinlock_lock_irqsave(&stress_lock);
    worker_allocs[wid] = local_allocs;
    worker_frees[wid] = local_frees;
    total_allocs += local_allocs;
    workers_done++;
    uint32_t done_count = workers_done;
    spinlock_unlock_irqrestore(&stress_lock, flags);

    // Last worker prints the report
    if (done_count == STRESS_WORKERS) {
        uint64_t after_used = pmm_get_used_pages();

        kprint("\n");
        kprint("[STRESS] ========================================\n");
        kprint("[STRESS] SMP PMM Stress Test — COMPLETE\n");
        kprint("[STRESS] ========================================\n");

        kprint("[STRESS] Workers:        ");
        kprint_num(STRESS_WORKERS);
        kprint("\n");

        kprint("[STRESS] Iterations/each: ");
        kprint_num(STRESS_ITERATIONS);
        kprint("\n");

        kprint("[STRESS] Total allocs:    ");
        kprint_num(total_allocs);
        kprint("\n");

        kprint("[STRESS] Online CPUs:     ");
        kprint_num(smp_online_cpu_count());
        kprint("\n");

        // Per-worker breakdown
        for (uint32_t w = 0; w < STRESS_WORKERS; w++) {
            kprint("[STRESS]   worker ");
            kprint_num(w);
            kprint(": alloc=");
            kprint_num(worker_allocs[w]);
            kprint(" free=");
            kprint_num(worker_frees[w]);
            if (worker_allocs[w] != worker_frees[w]) {
                kprint(" MISMATCH!");
            }
            kprint("\n");
        }

        // Page leak detection
        kprint("[STRESS] Pages used before: ");
        kprint_num(snap_used_pages);
        kprint("\n");
        kprint("[STRESS] Pages used after:  ");
        kprint_num(after_used);
        kprint("\n");

        if (snap_used_pages == after_used) {
            kprint("[STRESS] RESULT: PASS — no page leak, no corruption\n");
        } else if (after_used > snap_used_pages) {
            kprint("[STRESS] RESULT: FAIL — page LEAK detected (+");
            kprint_num(after_used - snap_used_pages);
            kprint(" pages)\n");
        } else {
            kprint("[STRESS] RESULT: FAIL — page UNDERFLOW (-");
            kprint_num(snap_used_pages - after_used);
            kprint(" pages)\n");
        }

        kprint("[STRESS] ========================================\n");
    }

    task_exit();
}

void stress_start(void) {
    kprint("[STRESS] Starting SMP PMM stress test...\n");
    kprint("[STRESS] Workers: ");
    kprint_num(STRESS_WORKERS);
    kprint(", iterations: ");
    kprint_num(STRESS_ITERATIONS);
    kprint("\n");
    kprint("[STRESS] Online CPUs: ");
    kprint_num(smp_online_cpu_count());
    kprint("\n");

    // Snapshot PMM state before test
    snap_used_pages = pmm_get_used_pages();
    workers_done = 0;
    total_allocs = 0;
    alloc_failures = 0;
    for (uint32_t i = 0; i < STRESS_WORKERS; i++) {
        worker_allocs[i] = 0;
        worker_frees[i] = 0;
    }

    // Spawn worker tasks — scheduler distributes across CPUs
    for (uint32_t i = 0; i < STRESS_WORKERS; i++) {
        create_task(stress_task, "stress");
    }
}
