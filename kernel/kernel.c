#include <stdint.h>
#include "fs.h"
#include "tty.h"
#include "pmm.h"
#include "paging.h"
#include "heap.h"
#include "string.h"
#include "ata.h"
#include "kyuzenfs.h"
#include "task.h"
#include "timer.h"
#include "shell.h" // Import Shell kita

extern void init_gdt();
extern void init_idt();
extern void pic_remap();
extern fs_node_t tty_node;
extern void set_kernel_stack(uint32_t stack);

uint32_t string_length(const char* str) {
    uint32_t len = 0;
    while (str[len]) len++;
    return len;
}

void print_hex(uint32_t num) {
    char hex_str[11] = "0x00000000";
    char hex_chars[] = "0123456789ABCDEF";
    for (int i = 9; i >= 2; i--) { hex_str[i] = hex_chars[num & 0xF]; num >>= 4; }
    write_fs(&tty_node, 0, 10, (uint8_t*)hex_str);
    write_fs(&tty_node, 0, 1, (uint8_t*)"\n");
}

void background_task() {
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
    int counter = 0;
    char spinner[] = {'|', '/', '-', '\\'};
    while(1) {
        vga[78] = (uint16_t)spinner[counter % 4] | (0x0E << 8); 
        counter++;
        for(volatile int i = 0; i < 500000; i++); 
        yield(); 
    }
}

// --- LOGIKA LOMPATAN RING 3 ---
void switch_to_user_mode(void (*user_func)()) {
    uint32_t user_stack = (uint32_t)kmalloc(4096) + 4096;
    uint32_t kernel_landing_stack = (uint32_t)kmalloc(4096) + 4096;
    set_kernel_stack(kernel_landing_stack);

    __asm__ volatile(
        "cli \n" "mov $0x23, %%ax \n" "mov %%ax, %%ds \n" "mov %%ax, %%es \n"
        "mov %%ax, %%fs \n" "mov %%ax, %%gs \n" "pushl $0x23 \n" "pushl %0 \n"            
        "pushfl \n" "popl %%eax \n" "orl $0x200, %%eax \n" "pushl %%eax \n"         
        "pushl $0x1B \n" "pushl %1 \n" "iret \n"                
        : : "r"(user_stack), "r"(user_func) : "%eax"
    );
}

void kernel_main(void) {
    fs_node_t* tty0 = init_tty();
    init_gdt(); init_idt(); pmm_init(); init_paging(); init_heap();

    char* msg1 = "========================================\n";
    write_fs(tty0, 0, string_length(msg1), (uint8_t*)msg1);
    char* msg2 = "   Kyuzen OS - Kernel Initialized       \n";
    write_fs(tty0, 0, string_length(msg2), (uint8_t*)msg2);
    write_fs(tty0, 0, string_length(msg1), (uint8_t*)msg1);

    pic_remap();
    __asm__ volatile("sti");

    kfs_init();
    tasking_init();
    init_timer(100);
    create_task(background_task);

    // KERNEL SELESAI BEKERJA, SERAHKAN KE USER SPACE SHELL!
    switch_to_user_mode(user_shell);
}