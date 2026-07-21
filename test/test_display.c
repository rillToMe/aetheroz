// Host self-check for Phase 3 display math (rect + dirty region).
// Pure arithmetic — no kernel target needed. Stubs cover the heap calls that
// the buffer create/destroy paths use.
//
// Build & run:
//   clang -I../include -DHOST_TEST test_display.c ../kernel/display.c -o test_display && ./test_display

#include <assert.h>
#include <stdio.h>
#include "display.h"

// Bump arena — avoids libc so the freestanding-style build stays simple.
static unsigned char arena[1 << 20];
static size_t arena_used = 0;
void* kmalloc(size_t n) { void* p = arena + arena_used; arena_used += (n + 15) & ~15u; return p; }
void  kfree(void* p) { (void)p; }

static void test_rect_intersect(void) {
    Rect a = { 0, 0, 100, 100 };
    Rect b = { 50, 50, 100, 100 };
    Rect out;
    assert(rect_intersect(a, b, &out));
    assert(out.x == 50 && out.y == 50 && out.width == 50 && out.height == 50);

    Rect c = { 200, 200, 10, 10 };
    assert(!rect_intersect(a, c, &out)); // disjoint

    Rect edge = { 100, 0, 10, 10 };
    assert(!rect_intersect(a, edge, &out)); // touching edge = empty
}

static void test_rect_union(void) {
    Rect a = { 0, 0, 10, 10 };
    Rect b = { 90, 90, 10, 10 };
    Rect u = rect_union(a, b);
    assert(u.x == 0 && u.y == 0 && u.width == 100 && u.height == 100);
}

static void test_dirty_collapse(void) {
    DirtyRegionList list;
    dirty_region_clear(&list);

    // Zero-area marks are ignored.
    Rect empty = { 5, 5, 0, 10 };
    dirty_region_mark(&list, empty);
    assert(list.count == 0);

    for (int i = 0; i < MAX_DIRTY_REGIONS; i++) {
        Rect r = { i, 0, 1, 1 };
        dirty_region_mark(&list, r);
    }
    assert(list.count == MAX_DIRTY_REGIONS && !list.collapsed);

    // One past capacity collapses to a bounding box covering all marks.
    Rect overflow = { MAX_DIRTY_REGIONS, 0, 1, 1 };
    dirty_region_mark(&list, overflow);
    assert(list.collapsed && list.count == 1);
    assert(list.regions[0].x == 0 && list.regions[0].width == (uint32_t)MAX_DIRTY_REGIONS + 1);

    // Further marks keep growing the box, never the count.
    Rect far = { 0, 500, 1, 1 };
    dirty_region_mark(&list, far);
    assert(list.count == 1 && list.regions[0].height == 501);
}

static void test_buffer_ownership(void) {
    DisplayBuffer* owned = display_buffer_create(4, 4, COLOR_FORMAT_XRGB8888);
    assert(owned && owned->owns_pixels && owned->stride == 4);
    display_buffer_write_pixel(owned, 1, 1, 0xFF00FF00);
    assert(owned->pixels[1 * 4 + 1] == 0xFF00FF00);
    display_buffer_write_pixel(owned, -1, 0, 0x1); // out of bounds = no-op
    display_buffer_destroy(owned);

    uint32_t backing[16] = {0};
    DisplayBuffer* borrowed = display_buffer_wrap(backing, 4, 4, 4, COLOR_FORMAT_XRGB8888);
    assert(borrowed && !borrowed->owns_pixels);
    display_buffer_destroy(borrowed); // must not free `backing`
    assert(borrowed == borrowed); // backing still valid below
    backing[0] = 0xDEAD; // would fault if freed via bad path
}

int main(void) {
    test_rect_intersect();
    test_rect_union();
    test_dirty_collapse();
    test_buffer_ownership();
    printf("OK test_display\n");
    return 0;
}
