// test/heap_stress_test.c — Heap regression + canary harness (make heap-stress).
// Guarded by HEAP_STRESS_TEST; absent from normal myos.bin.
//
// Verifies:
//   1. kfs_read_to_buffer/syscall 13 capacity guard — undersized buffer fails
//      cleanly and does NOT smash the neighbouring heap_block_t.
//   2. heap_block_t magic canary — corrupting a header is caught at that block,
//      via a test override of kernel_panic() that sets a flag instead of halting.
#ifdef HEAP_STRESS_TEST

#include <stdint.h>
#include <stddef.h>
#include "heap.h"

extern void     kprint(const char *str);
extern void     kprint_num(uint64_t num);
extern uint64_t pmm_get_used_pages(void);

extern int      kfs_create_file(char* filename, char* data, uint32_t size);
extern int      kfs_read_to_buffer(char* filename, char* out_buffer, uint32_t buffer_capacity);
extern int      kfs_exists(char* filename);
extern void     kfs_delete_file(char* filename);

extern int      kwm_create_window(int x, int y, uint32_t width, uint32_t height);
extern void     kwm_destroy_all_windows(void);

#define HEAP_MAGIC 0xDEADC0DE

// ---- kernel_panic test override -------------------------------------------
// panic.c declares kernel_panic weak; this strong definition wins in this build
// only. Records the last panic title instead of freezing so the canary test can
// assert it fired and continue. Corruption in the canary test is magic-only, so
// kmalloc/kfree resume safely over intact size/is_free/next fields.
volatile int         g_panic_fired = 0;
volatile const char* g_panic_title = 0;

void kernel_panic(const char* title, const char* desc, uint64_t code) {
    (void)desc; (void)code;
    g_panic_fired = 1;
    g_panic_title = title;
}

// ---- tiny helpers ----------------------------------------------------------
static uint32_t rng_state = 0x1234567u;
static uint32_t rng(void) {                 // LCG; no Date/rand in freestanding
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 8) & 0x7FFFFFFF;
}

static int str_eq(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

static heap_block_t* header_of(void* ptr) {
    return (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));
}

static void report(const char *name, int pass) {
    kprint("[HEAP] ");
    kprint(name);
    kprint(pass ? " ... PASS\n" : " ... FAIL\n");
}

// ---- a. overflow guard ------------------------------------------------------
static int test_heap_overflow_guard(void) {
    const char* fname = "heaptst.bin";
    if (kfs_exists((char*)fname)) kfs_delete_file((char*)fname);

    char big[100];
    for (int i = 0; i < 100; i++) big[i] = (char)('A' + (i % 26));
    if (!kfs_create_file((char*)fname, big, 100)) return 0;   // 100-byte file

    // Undersized buffer: capacity 16 < file 100.
    char* small = (char*)kmalloc(16);
    if (!small) { kfs_delete_file((char*)fname); return 0; }

    int rc = kfs_read_to_buffer((char*)fname, small, 16);

    // Must fail — NOT partial-copy-then-success.
    int ok = (rc == 0);

    // Neighbour intact: a fresh allocation still has a valid canary.
    void* probe = kmalloc(64);
    ok &= (probe != 0) && (header_of(probe)->magic == HEAP_MAGIC);

    kfree(probe);
    kfree(small);
    kfs_delete_file((char*)fname);
    return ok;
}

// ---- c. alloc/free cycles + leak regression --------------------------------
// Heap is grow-only (pages never return to PMM). ponytail: warm the heap to the
// loop's peak first, THEN baseline; the real regression is "no NEW growth after
// warm-up". Upgrade path: if kfree ever unmaps pages, drop the warm-up.
#define CYC_ITERS   4000
#define CYC_HOLD    64
#define CYC_MAXSZ   4096

static int test_heap_stress_alloc_free_cycles(void) {
    void* held[CYC_HOLD] = {0};

    // Warm-up: force heap to grow to well above the loop's live footprint.
    void* warm[CYC_HOLD];
    for (int i = 0; i < CYC_HOLD; i++) warm[i] = kmalloc(CYC_MAXSZ);
    for (int i = 0; i < CYC_HOLD; i++) kfree(warm[i]);

    uint64_t baseline = pmm_get_used_pages();

    for (int i = 0; i < CYC_ITERS; i++) {
        uint32_t sz = 8 + (rng() % CYC_MAXSZ);
        int slot = rng() % CYC_HOLD;

        if (held[slot]) {           // long-lifetime slot occupied -> free it now
            kfree(held[slot]);
            held[slot] = 0;
        }

        void* p = kmalloc(sz);
        if (!p) return 0;
        if (header_of(p)->magic != HEAP_MAGIC) return 0;

        if (rng() & 1) kfree(p);    // short lifetime
        else           held[slot] = p;   // long lifetime, freed a later iter

        if ((i % 1000) == 0) {
            kprint("[HEAP]   cycle "); kprint_num((uint64_t)i);
            kprint(" used_pages="); kprint_num(pmm_get_used_pages());
            kprint("\n");
        }
    }

    for (int i = 0; i < CYC_HOLD; i++) if (held[i]) kfree(held[i]);

    uint64_t after = pmm_get_used_pages();
    kprint("[HEAP]   baseline="); kprint_num(baseline);
    kprint(" after="); kprint_num(after); kprint("\n");
    return after <= baseline;       // no new leak beyond warmed capacity
}

// ---- d. (optional) app open/close simulation -------------------------------
#define SIM_ROUNDS   200
static int test_heap_stress_app_open_close_simulation(void) {
    const char* fname = "simfile.bin";
    if (kfs_exists((char*)fname)) kfs_delete_file((char*)fname);
    char blob[256];
    for (int i = 0; i < 256; i++) blob[i] = (char)(i & 0xFF);
    kfs_create_file((char*)fname, blob, 256);

    // Warm-up one full round so heap grows to peak before baselining.
    for (int w = 0; w < 2; w++) {
        int win = kwm_create_window(0, 0, 100, 80);
        (void)win;
        char* buf = (char*)kmalloc(256);
        if (buf) { kfs_read_to_buffer((char*)fname, buf, 256); kfree(buf); }
        kwm_destroy_all_windows();
    }

    uint64_t baseline = pmm_get_used_pages();

    for (int r = 0; r < SIM_ROUNDS; r++) {
        kwm_create_window(10, 10, 120, 90);      // allocates canvas
        char* buf = (char*)kmalloc(256);         // tight buffer == file size
        if (buf) {
            kfs_read_to_buffer((char*)fname, buf, 256);
            if (header_of(buf)->magic != HEAP_MAGIC) { kfree(buf); return 0; }
            kfree(buf);
        }
        kwm_destroy_all_windows();
    }

    uint64_t after = pmm_get_used_pages();
    kfs_delete_file((char*)fname);
    kprint("[HEAP]   sim baseline="); kprint_num(baseline);
    kprint(" after="); kprint_num(after); kprint("\n");
    return after <= baseline;
}

// ---- b. canary detection (runs LAST — corrupts the heap on purpose) ---------
static int test_heap_canary_detection(void) {
    g_panic_fired = 0;
    g_panic_title = 0;

    void* a = kmalloc(32);
    void* b = kmalloc(32);
    if (!a || !b) return 0;

    // Overflow past `a` by 4 bytes -> lands on b's header magic (first field).
    volatile uint32_t* smash = (volatile uint32_t*)((uint8_t*)a + 32);
    *smash = 0;                     // corrupt b->magic

    // kfree walks the whole list coalescing -> must visit b, see bad magic, panic.
    kfree(a);

    int ok = g_panic_fired && g_panic_title && str_eq((const char*)g_panic_title, "HEAP CORRUPTION");
    return ok;
}

// ---- entry ------------------------------------------------------------------
void test_heap_stress_run_all(void) {
    kprint("\n[HEAP] ===== Heap stress + canary tests =====\n");

    report("overflow guard (fix #1)",      test_heap_overflow_guard());
    report("alloc/free leak regression",   test_heap_stress_alloc_free_cycles());
    report("app open/close simulation",    test_heap_stress_app_open_close_simulation());
    // Destructive: intentionally corrupts a header. Keep last.
    report("magic canary detect (fix #2)", test_heap_canary_detection());

    kprint("[HEAP] ===== done =====\n");
}

#endif // HEAP_STRESS_TEST
