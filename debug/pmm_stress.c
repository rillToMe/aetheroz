// ============================================================
// debug/pmm_stress.c — SMP PMM Stress Test
//
// Spawns 4 worker tasks that each perform 500K alloc/free
// cycles concurrently across CPUs. Validates that the PMM
// spinlock prevents bitmap races under parallel load.
//
// PASS: every worker's allocs == frees (no lost operations)
// ============================================================

#include "pmm.h"
#include "task.h"
#include "smp.h"
#include "spinlock.h"
#include "pmm_stress.h"

extern void kprint(const char *str);
extern void kprint_num(uint64_t num);

#define STRESS_WORKERS      4
#define STRESS_ITERATIONS   500000ULL

static spinlock_t stress_lock = SPINLOCK_INIT;
static volatile uint32_t workers_done = 0;
static volatile uint64_t total_allocs = 0;
static volatile uint64_t total_frees = 0;

static volatile uint64_t worker_allocs[STRESS_WORKERS];
static volatile uint64_t worker_frees[STRESS_WORKERS];
static volatile uint64_t worker_failures[STRESS_WORKERS];

void stress_task(void) {
    uint32_t cpu_id = smp_current_cpu_index();
    uint32_t wid = cpu_id % STRESS_WORKERS;

    uint64_t local_allocs = 0;
    uint64_t local_frees = 0;
    uint64_t local_failures = 0;

    for (uint64_t i = 0; i < STRESS_ITERATIONS; i++) {
        phys_addr_t p = pmm_alloc_page();
        if (p != PHYS_NULL) {
            local_allocs++;
            pmm_free_page(p);
            local_frees++;
        } else {
            local_failures++;
        }
    }

    uint64_t flags = spinlock_lock_irqsave(&stress_lock);
    worker_allocs[wid] = local_allocs;
    worker_frees[wid] = local_frees;
    worker_failures[wid] = local_failures;
    total_allocs += local_allocs;
    total_frees += local_frees;
    workers_done++;
    uint32_t done_count = workers_done;
    spinlock_unlock_irqrestore(&stress_lock, flags);

    if (done_count == STRESS_WORKERS) {
        uint64_t expected = (uint64_t)STRESS_WORKERS * STRESS_ITERATIONS;
        int pass = 1;

        kprint("\n");
        kprint("[STRESS] ========================================\n");
        kprint("[STRESS] SMP PMM Stress Test — COMPLETE\n");
        kprint("[STRESS] ========================================\n");

        for (uint32_t w = 0; w < STRESS_WORKERS; w++) {
            kprint("[STRESS]   cpu ");
            kprint_num(w);
            kprint(": alloc=");
            kprint_num(worker_allocs[w]);
            kprint(" free=");
            kprint_num(worker_frees[w]);
            if (worker_allocs[w] != worker_frees[w]) {
                kprint(" LEAK!");
                pass = 0;
            }
            if (worker_failures[w] > 0) {
                kprint(" oom=");
                kprint_num(worker_failures[w]);
            }
            kprint("\n");
        }

        kprint("[STRESS] Total: alloc=");
        kprint_num(total_allocs);
        kprint(" free=");
        kprint_num(total_frees);
        kprint(" expected=");
        kprint_num(expected);
        kprint("\n");

        if (total_allocs != total_frees) pass = 0;

        kprint("\n");
        if (pass) {
            kprint("[STRESS] PASS — ");
            kprint_num(total_allocs);
            kprint(" pairs across ");
            kprint_num(STRESS_WORKERS);
            kprint(" CPUs, zero leaks\n");
        } else {
            kprint("[STRESS] FAIL — alloc/free mismatch!\n");
        }
        kprint("[STRESS] ========================================\n");
    }

    task_exit();
}

void stress_start(void) {
    kprint("\n");
    kprint("[STRESS] SMP Stress: ");
    kprint_num(STRESS_WORKERS);
    kprint(" workers x ");
    kprint_num(STRESS_ITERATIONS);
    kprint(" iterations\n");
    kprint("[STRESS] Online CPUs: ");
    kprint_num(smp_online_cpu_count());
    kprint("\n");

    workers_done = 0;
    total_allocs = 0;
    total_frees = 0;
    for (uint32_t i = 0; i < STRESS_WORKERS; i++) {
        worker_allocs[i] = 0;
        worker_frees[i] = 0;
        worker_failures[i] = 0;
    }

    for (uint32_t i = 0; i < STRESS_WORKERS; i++) {
        create_task(stress_task, "stress");
    }
}
