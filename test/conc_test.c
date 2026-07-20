// test/conc_test.c — Runtime tests for Fase 1-3 (sleep, mutex, semaphore, condvar).
// Runs from kmain (task 0) before user mode. Enable: make test.

#include "task.h"
#include "sync.h"
#include "smp.h"
#include "timer.h"
#include "conc_test.h"

extern void kprint(const char *str);
extern void kprint_num(uint64_t num);

#define MUTEX_WORKERS   4
#define MUTEX_INCS      50000ULL

static void report(const char *name, int pass) {
    kprint("[TEST] ");
    kprint(name);
    kprint(pass ? " ... PASS\n" : " ... FAIL\n");
}

// ---------------- sleep ----------------

static int test_sleep(void) {
    uint64_t t0 = timer_get_ms();
    task_sleep_ms(200);
    uint64_t elapsed = timer_get_ms() - t0;
    // Woke no earlier than requested; generous upper bound for scheduler latency.
    return elapsed >= 200 && elapsed < 400;
}

// ---------------- mutex (SMP race) ----------------

static mutex_t     mtx = MUTEX_INIT;
static volatile uint64_t counter = 0;
static semaphore_t done = SEMAPHORE_INIT(0);

static void mutex_worker(void) {
    for (uint64_t i = 0; i < MUTEX_INCS; i++) {
        mutex_lock(&mtx);
        counter++;              // non-atomic on purpose: mutex must serialize
        mutex_unlock(&mtx);
    }
    semaphore_post(&done);
    task_exit();
}

static int test_mutex(void) {
    counter = 0;
    for (int i = 0; i < MUTEX_WORKERS; i++) create_task(mutex_worker, "mtxw");
    for (int i = 0; i < MUTEX_WORKERS; i++) semaphore_wait(&done);
    return counter == (uint64_t)MUTEX_WORKERS * MUTEX_INCS;
}

// ---------------- semaphore (count) ----------------

static int test_semaphore(void) {
    semaphore_t s;
    semaphore_init(&s, 2);
    int ok = semaphore_trywait(&s);      // 2 -> 1
    ok &= semaphore_trywait(&s);         // 1 -> 0
    ok &= !semaphore_trywait(&s);        // 0 -> would block
    semaphore_post(&s);                  // 0 -> 1
    ok &= semaphore_trywait(&s);         // 1 -> 0
    return ok;
}

// ---------------- condvar ----------------

static mutex_t    cv_mtx  = MUTEX_INIT;
static condvar_t  cv      = CONDVAR_INIT;
static volatile int ready = 0;
static semaphore_t cv_done = SEMAPHORE_INIT(0);

static void cv_waiter(void) {
    mutex_lock(&cv_mtx);
    while (!ready) condvar_wait(&cv, &cv_mtx);
    mutex_unlock(&cv_mtx);
    semaphore_post(&cv_done);
    task_exit();
}

static int test_condvar(void) {
    ready = 0;
    create_task(cv_waiter, "cvw");
    task_sleep_ms(50);                   // let waiter reach condvar_wait
    mutex_lock(&cv_mtx);
    ready = 1;
    condvar_signal(&cv);
    mutex_unlock(&cv_mtx);
    semaphore_wait(&cv_done);            // hangs if signal was lost
    return ready == 1;
}

void conc_test_run(void) {
    kprint("\n[TEST] ===== Concurrency tests (Fase 1-3) =====\n");
    kprint("[TEST] Online CPUs: ");
    kprint_num(smp_online_cpu_count());
    kprint("\n");

    report("sleep queue", test_sleep());
    report("semaphore",   test_semaphore());
    report("mutex SMP",   test_mutex());
    report("condvar",     test_condvar());

    kprint("[TEST] ===== done =====\n");
}
