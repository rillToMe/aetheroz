#include <kernel/arch/paging.h>
#include <kernel/arch/serial.h>
#include <kernel/boot/limine.h>
#include <kernel/fs/vfs.h>
#include <kernel/memory/heap.h>
#include <kernel/memory/memmap.h>
#include <kernel/memory/pmm.h>
#include <kernel/video/framebuffer.h>

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[3] = LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_requests_start_marker[4] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request fb_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request exec_addr_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_requests_end_marker[2] = LIMINE_REQUESTS_END_MARKER;
static int dummy_read(file_t *file, void *buf, uint64_t size) {
    (void)file;
    (void)buf;
    (void)size;
    serial_write('R');
    return 0;
}

static int null_read(file_t *file, void *buf, uint64_t size) {
    (void)file;
    (void)buf;
    (void)size;
    return 0;
}

static int null_write(file_t *file, const void *buf, uint64_t size) {
    (void)file;
    (void)buf;
    return (int)size;
}

static file_ops_t dummy_ops = {
    .read = dummy_read,
    .write = 0,
    .close = 0
};

static file_ops_t null_ops = {
    .read = null_read,
    .write = null_write,
    .close = 0
};

static vnode_t root_node = {
    .name = "/",
    .ops = 0,
    .internal = 0,
    .parent = 0,
    .child = 0,
    .next = 0
};

static vnode_t dev_node = {
    .name = "dev",
    .ops = 0,
    .internal = 0,
    .parent = 0,
    .child = 0,
    .next = 0
};

static vnode_t serial_node = {
    .name = "serial",
    .ops = &dummy_ops,
    .internal = 0,
    .parent = 0,
    .child = 0,
    .next = 0
};

static vnode_t null_node = {
    .name = "null",
    .ops = &null_ops,
    .internal = 0,
    .parent = 0,
    .child = 0,
    .next = 0
};

void kernel_main(void) {
    serial_init();
    if (!memmap_request.response) {
        for (;;) {
            __asm__ __volatile__("hlt");
        }
    }
    if (!hhdm_request.response || !exec_addr_request.response) {
        for (;;) {
            __asm__ __volatile__("hlt");
        }
    }
    memmap_init(memmap_request.response);
    pmm_init(memmap_total_usable(), memmap_usable_regions(), memmap_usable_region_count());
    paging_init(hhdm_request.response->offset,
                exec_addr_request.response->physical_base,
                exec_addr_request.response->virtual_base);
    heap_init();
    void *t = kmalloc(64);
    serial_write(' ');
    serial_write('H');
    serial_write('=');
    serial_write('0');
    serial_write('x');
    serial_write_hex64((uint64_t)(uintptr_t)t);
    vfs_add_child(&root_node, &dev_node);
    vfs_add_child(&dev_node, &serial_node);
    vfs_add_child(&dev_node, &null_node);
    vnode_t *found = vfs_resolve(&root_node, "/dev/serial");
    if (found) {
        file_t *f = vfs_open(found);
        vfs_read(f, 0, 0);
    }
    vnode_t *n = vfs_resolve(&root_node, "/dev/null");
    if (n) {
        file_t *f = vfs_open(n);
        char buffer[16];
        int r = vfs_read(f, buffer, 16);
        serial_write(' ');
        serial_write('N');
        serial_write('=');
        serial_write_hex64((uint64_t)r);
        serial_write(' ');
        serial_write('W');
        serial_write('=');
        int w = vfs_write(f, buffer, 16);
        serial_write_hex64((uint64_t)w);
    }
    framebuffer_init(fb_request.response);
    fb_draw_string(100, 100, "OK", 0x00FF0000);
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
