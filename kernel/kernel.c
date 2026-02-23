#include <kernel/arch/paging.h>
#include <kernel/arch/serial.h>
#include <kernel/boot/limine.h>
#include <kernel/fs/fd.h>
#include <kernel/fs/vfs.h>
#include <kernel/memory/heap.h>
#include <kernel/memory/memmap.h>
#include <kernel/memory/pmm.h>
#include <kernel/memory/vmm.h>
#include <kernel/arch/x86_64/idt.h>
#include <kernel/arch/x86_64/gdt.h>
#include <kernel/arch/x86_64/pic.h>
#include <kernel/arch/x86_64/syscall.h>
#include <kernel/arch/x86_64/timer.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/task.h>
#include <kernel/sys/syscall.h>
#include <kernel/video/framebuffer.h>

extern char __kernel_start;
extern char __kernel_end;

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
    return 0;
}

static int console_write(file_t *file, const void *buf, uint64_t size) {
    (void)file;
    const uint8_t *bytes = (const uint8_t *)buf;
    for (uint64_t i = 0; i < size; i++) {
        serial_write((char)bytes[i]);
    }
    return (int)size;
}

static void log_write(const char *text) {
    if (!text) {
        return;
    }
    while (*text) {
        serial_write(*text++);
    }
}

static void log_hex64(uint64_t value) {
    serial_write_hex64(value);
}

static void print_hex_label(const char *label, uint64_t value) {
    log_write(label);
    log_write("0x");
    log_hex64(value);
    log_write(" ");
}

static int vmm_test(void) {
    address_space_t *as1 = address_space_create();
    address_space_t *as2 = address_space_create();
    if (!as1 || !as2) {
        return 0;
    }
    uint64_t virt = 0x00000050000000ULL;
    uint64_t phys1 = (uint64_t)(uintptr_t)pmm_alloc_page();
    uint64_t phys2 = (uint64_t)(uintptr_t)pmm_alloc_page();
    if (!phys1 || !phys2) {
        return 0;
    }
    *(volatile uint64_t *)(uintptr_t)hhdm_phys_to_virt(phys1) = 0x1111111111111111ULL;
    *(volatile uint64_t *)(uintptr_t)hhdm_phys_to_virt(phys2) = 0x2222222222222222ULL;
    if (vmm_map_page(as1, virt, phys1, VMM_PAGE_USER | VMM_PAGE_WRITE) != 0) {
        return 0;
    }
    if (vmm_map_page(as2, virt, phys2, VMM_PAGE_USER | VMM_PAGE_WRITE) != 0) {
        return 0;
    }
    address_space_switch(as1);
    uint64_t v1 = *(volatile uint64_t *)(uintptr_t)virt;
    address_space_switch(as2);
    uint64_t v2 = *(volatile uint64_t *)(uintptr_t)virt;
    address_space_switch(address_space_kernel());
    return (v1 == 0x1111111111111111ULL) && (v2 == 0x2222222222222222ULL);
}

static void task1(void) {
    for (;;) {
        log_write("A");
        task_sleep(5);
    }
}

static void task2(void) {
    for (;;) {
        log_write("B");
        task_sleep(5);
    }
}

static void idle_task(void) {
    for (;;) {
        scheduler_post_interrupt();
        __asm__ __volatile__("hlt");
    }
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

static int zero_read(file_t *file, void *buf, uint64_t size) {
    (void)file;
    if (!buf) {
        return -1;
    }
    uint8_t *p = (uint8_t *)buf;
    for (uint64_t i = 0; i < size; i++) {
        p[i] = 0;
    }
    return (int)size;
}

static int zero_write(file_t *file, const void *buf, uint64_t size) {
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

static file_ops_t console_ops = {
    .read = 0,
    .write = console_write,
    .close = 0
};

static file_ops_t zero_ops = {
    .read = zero_read,
    .write = zero_write,
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

static vnode_t console_node = {
    .name = "console",
    .ops = &console_ops,
    .internal = 0,
    .parent = 0,
    .child = 0,
    .next = 0
};

static vnode_t zero_node = {
    .name = "zero",
    .ops = &zero_ops,
    .internal = 0,
    .parent = 0,
    .child = 0,
    .next = 0
};

static task_t *g_t1 = 0;
static task_t *g_t2 = 0;
static task_t *g_idle = 0;
static task_t bootstrap_task;

static void kernel_after_user(void) {
    __asm__ __volatile__("cli");
    task_set_current(g_t2);
    fd_set(0, &null_node);
    fd_set(1, &console_node);
    fd_set(2, &console_node);
    task_set_current(g_idle);
    fd_set(0, &null_node);
    fd_set(1, &console_node);
    fd_set(2, &console_node);
    task_set_current(g_t1);
    fd_set(0, &null_node);
    fd_set(1, &console_node);
    fd_set(2, &console_node);
    task_set_current(g_t1);
    g_t1->state = TASK_RUNNING;
    g_t2->state = TASK_READY;
    g_idle->state = TASK_READY;
    __asm__ __volatile__("cli");
    task_set_current(&bootstrap_task);
    sched_force_switch(g_t1);
    scheduler_post_interrupt();
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

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
    paging_set_hhdm_offset(hhdm_request.response->offset);
    memmap_init(memmap_request.response);
    pmm_init(memmap_total_usable(), memmap_usable_regions(), memmap_usable_region_count());
    log_write("[MM] ");
    print_hex_label("T=", memmap_total_usable());
    print_hex_label("P=", pmm_total_pages());
    print_hex_label("B=", pmm_bitmap_size());
    print_hex_label("F=", pmm_free_pages());
    log_write("\n");
    void *test_page = pmm_alloc_page();
    log_write("[PMM] ");
    print_hex_label("A=", (uint64_t)(uintptr_t)test_page);
    log_write("\n");
    pmm_free_page(test_page);
    uint64_t kernel_start_phys = exec_addr_request.response->physical_base +
                                 ((uint64_t)(uintptr_t)&__kernel_start -
                                  exec_addr_request.response->virtual_base);
    uint64_t kernel_end_phys = exec_addr_request.response->physical_base +
                               ((uint64_t)(uintptr_t)&__kernel_end -
                                exec_addr_request.response->virtual_base);
    if (kernel_end_phys > kernel_start_phys) {
        pmm_reserve_range(kernel_start_phys, kernel_end_phys - kernel_start_phys);
    }
    paging_init(hhdm_request.response->offset,
                exec_addr_request.response->physical_base,
                exec_addr_request.response->virtual_base);
    vmm_init(paging_kernel_pml4());
    heap_init();
    void *h = kmalloc(64);
    gdt_init();
    idt_init();
    pic_remap(0x20, 0x28);
    timer_init(100);
    sched_init();
    __asm__ __volatile__("sti");
    vfs_add_child(&root_node, &dev_node);
    vfs_add_child(&dev_node, &serial_node);
    vfs_add_child(&dev_node, &null_node);
    vfs_add_child(&dev_node, &console_node);
    vfs_add_child(&dev_node, &zero_node);
    g_t1 = task_create(task1);
    g_t2 = task_create(task2);
    g_idle = task_create(idle_task);
    if (!g_t1 || !g_t2 || !g_idle) {
        for (;;) {
            __asm__ __volatile__("hlt");
        }
    }
    sched_add(g_t1);
    sched_add(g_t2);
    sched_set_idle(g_idle);
    for (int i = 0; i < MAX_FDS; i++) {
        bootstrap_task.fd_table[i] = 0;
    }
    bootstrap_task.rsp = 0;
    bootstrap_task.stack = 0;
    bootstrap_task.pid = 0;
    bootstrap_task.state = TASK_RUNNING;
    bootstrap_task.wakeup_tick = 0;
    bootstrap_task.quantum = TASK_DEFAULT_QUANTUM;
    bootstrap_task.next = 0;
    bootstrap_task.next_wait = 0;
    bootstrap_task.waiting_on = 0;
    bootstrap_task.as = address_space_kernel();
    task_set_current(&bootstrap_task);
    fd_set(0, &null_node);
    fd_set(1, &console_node);
    fd_set(2, &console_node);
    int vmm_ok = vmm_test();
    vnode_t *found = vfs_resolve(&root_node, "/dev/serial");
    int vfs_ok = 0;
    if (found) {
        file_t *f = vfs_open(found);
        vfs_ok = vfs_read(f, 0, 0) == 0;
    }
    log_write("[HEAP] ");
    print_hex_label("H=", (uint64_t)(uintptr_t)h);
    log_write("\n");
    log_write("[VFS] R=");
    log_write(vfs_ok ? "OK\n" : "FAIL\n");
    char buffer[16];
    for (int i = 0; i < 16; i++) {
        buffer[i] = 0;
    }
    int r = fd_read(0, buffer, 16);
    int w = fd_write(2, buffer, 16);
    log_write("[FD] ");
    print_hex_label("r=", (uint64_t)r);
    print_hex_label("w=", (uint64_t)w);
    log_write("\n");
    vnode_t *z = vfs_resolve(&root_node, "/dev/zero");
    uint64_t z_result = 0;
    if (z) {
        int fd = fd_open(z);
        uint8_t zbuf[8];
        int zr = fd_read(fd, zbuf, 8);
        z_result = (uint64_t)zr;
        fd_close(fd);
    }
    log_write("[DEV] ");
    print_hex_label("Z=", z_result);
    log_write("\n");
    log_write("[VMM] = ");
    log_write(vmm_ok ? "OK\n\n" : "FAIL\n\n");
    log_write("===============================\n\n");
    log_write("[BOOT COMPLETE]\n");
    log_write("[SCHED] S\n");
    log_write("[TASK]\n");
    syscall_init();
    syscall_arch_init();
    framebuffer_init(fb_request.response);
    fb_draw_string(100, 100, "OK", 0x00FF0000);
    uint64_t user_code_phys = (uint64_t)(uintptr_t)pmm_alloc_page();
    uint64_t user_stack_phys = (uint64_t)(uintptr_t)pmm_alloc_page();
    if (!user_code_phys || !user_stack_phys) {
        for (;;) {
            __asm__ __volatile__("hlt");
        }
    }
    uint8_t *user_code = (uint8_t *)(uintptr_t)hhdm_phys_to_virt(user_code_phys);
    uint8_t *user_stack = (uint8_t *)(uintptr_t)hhdm_phys_to_virt(user_stack_phys);
    uint64_t user_stack_virt = 0x00000040000000ULL;
    uint64_t user_code_virt = 0x00000040001000ULL;
    if (vmm_map_page(address_space_kernel(), user_code_virt, user_code_phys,
                     VMM_PAGE_USER | VMM_PAGE_WRITE) != 0) {
        for (;;) {
            __asm__ __volatile__("hlt");
        }
    }
    if (vmm_map_page(address_space_kernel(), user_stack_virt, user_stack_phys,
                     VMM_PAGE_USER | VMM_PAGE_WRITE) != 0) {
        for (;;) {
            __asm__ __volatile__("hlt");
        }
    }
    for (int i = 0; i < 0x1000; i++) {
        user_code[i] = 0;
    }
    uint64_t msg_addr = user_code_virt + 0x100;
    user_code[0x100] = 0;
    int idx = 0;
    user_code[idx++] = 0x48;
    user_code[idx++] = 0xC7;
    user_code[idx++] = 0xC0;
    user_code[idx++] = 0x00;
    user_code[idx++] = 0x00;
    user_code[idx++] = 0x00;
    user_code[idx++] = 0x00;
    user_code[idx++] = 0x48;
    user_code[idx++] = 0xC7;
    user_code[idx++] = 0xC7;
    user_code[idx++] = 0x01;
    user_code[idx++] = 0x00;
    user_code[idx++] = 0x00;
    user_code[idx++] = 0x00;
    user_code[idx++] = 0x48;
    user_code[idx++] = 0xBE;
    for (int i = 0; i < 8; i++) {
        user_code[idx++] = (uint8_t)((msg_addr >> (i * 8)) & 0xFF);
    }
    user_code[idx++] = 0x48;
    user_code[idx++] = 0xC7;
    user_code[idx++] = 0xC2;
    user_code[idx++] = 0x01;
    user_code[idx++] = 0x00;
    user_code[idx++] = 0x00;
    user_code[idx++] = 0x00;
    user_code[idx++] = 0x4D;
    user_code[idx++] = 0x31;
    user_code[idx++] = 0xD2;
    user_code[idx++] = 0x0F;
    user_code[idx++] = 0x05;
    user_code[idx++] = 0x48;
    user_code[idx++] = 0xC7;
    user_code[idx++] = 0xC0;
    user_code[idx++] = 0x04;
    user_code[idx++] = 0x00;
    user_code[idx++] = 0x00;
    user_code[idx++] = 0x00;
    user_code[idx++] = 0x0F;
    user_code[idx++] = 0x05;
    user_code[idx++] = 0xEB;
    user_code[idx++] = 0xFE;
    uint64_t user_rsp = user_stack_virt + 0x1000 - 16;
    uint64_t kernel_rsp = 0;
    __asm__ __volatile__("mov %%rsp, %0" : "=r"(kernel_rsp));
    syscall_set_kernel_return((uint64_t)(uintptr_t)kernel_after_user, kernel_rsp);
    __asm__ __volatile__("cli");
    enter_user_mode(user_code_virt, user_rsp);
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
