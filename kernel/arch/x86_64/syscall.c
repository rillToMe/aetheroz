#include <stdint.h>
#include <kernel/arch/x86_64/msr.h>
#include <kernel/arch/x86_64/syscall.h>

#define MSR_EFER 0xC0000080
#define MSR_STAR 0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_SFMASK 0xC0000084

extern void syscall_entry(void);

uint64_t syscall_user_rsp = 0;
uint64_t syscall_kernel_rsp = 0;
uint64_t syscall_kernel_return_rip = 0;
uint64_t syscall_kernel_return_rsp = 0;
volatile uint64_t syscall_return_to_kernel = 0;

static uint8_t syscall_stack[4096] __attribute__((aligned(16)));

void syscall_arch_init(void) {
    syscall_kernel_rsp = (uint64_t)(uintptr_t)(syscall_stack + sizeof(syscall_stack));
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= 1;
    wrmsr(MSR_EFER, efer);
    uint64_t kernel_cs = 0x08;
    uint64_t star_user = 0x1B;
    uint64_t star = (star_user << 48) | (kernel_cs << 32);
    wrmsr(MSR_STAR, star);
    wrmsr(MSR_LSTAR, (uint64_t)(uintptr_t)syscall_entry);
    wrmsr(MSR_SFMASK, 0);
}

void syscall_set_kernel_return(uint64_t rip, uint64_t rsp) {
    syscall_kernel_return_rip = rip;
    syscall_kernel_return_rsp = rsp;
}
