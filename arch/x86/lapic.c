#include <stdint.h>
#include "lapic.h"

extern uint64_t hhdm_offset;

#define IA32_APIC_BASE_MSR      0x1B
#define IA32_APIC_BASE_ENABLE   (1ULL << 11)

#define LAPIC_REG_ID            0x020
#define LAPIC_REG_EOI           0x0B0
#define LAPIC_REG_SVR           0x0F0
#define LAPIC_REG_LVT_TIMER     0x320
#define LAPIC_REG_TIMER_INIT    0x380
#define LAPIC_REG_TIMER_CURRENT 0x390
#define LAPIC_REG_TIMER_DIVIDE  0x3E0

#define LAPIC_SVR_ENABLE        0x100
#define LAPIC_TIMER_PERIODIC    (1U << 17)
#define LAPIC_TIMER_MASKED      (1U << 16)

#define LAPIC_SPURIOUS_VECTOR   0xFF
#define LAPIC_TIMER_INIT_COUNT  1000000U

static volatile uint32_t *lapic_mmio = 0;
static volatile uint64_t lapic_ticks = 0;

static uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t lo = (uint32_t)value;
    uint32_t hi = (uint32_t)(value >> 32);
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi) : "memory");
}

static uint32_t lapic_read(uint32_t reg) {
    return lapic_mmio[reg >> 2];
}

static void lapic_write(uint32_t reg, uint32_t value) {
    lapic_mmio[reg >> 2] = value;
    (void)lapic_read(LAPIC_REG_ID);
}

static void lapic_enable(void) {
    uint64_t apic_base = rdmsr(IA32_APIC_BASE_MSR);
    uint64_t phys = apic_base & 0xFFFFFFFFFFFFF000ULL;

    wrmsr(IA32_APIC_BASE_MSR, apic_base | IA32_APIC_BASE_ENABLE);

    if (lapic_mmio == 0) {
        lapic_mmio = (volatile uint32_t *)(phys + hhdm_offset);
    }

    lapic_write(LAPIC_REG_SVR, LAPIC_SVR_ENABLE | LAPIC_SPURIOUS_VECTOR);
}

static void lapic_timer_start(void) {
    lapic_write(LAPIC_REG_TIMER_DIVIDE, 0x3); // divide by 16
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_PERIODIC | LAPIC_TIMER_VECTOR);
    lapic_write(LAPIC_REG_TIMER_INIT, LAPIC_TIMER_INIT_COUNT);
}

void lapic_init_bsp(void) {
    lapic_enable();
    lapic_timer_start();
}

void lapic_init_ap(void) {
    lapic_enable();
    lapic_timer_start();
}

void lapic_eoi(void) {
    if (lapic_mmio != 0) {
        lapic_write(LAPIC_REG_EOI, 0);
    }
}

uint32_t lapic_id(void) {
    if (lapic_mmio == 0) return 0;
    return lapic_read(LAPIC_REG_ID) >> 24;
}

uint64_t lapic_timer_ticks(void) {
    return lapic_ticks;
}

void lapic_timer_handler(void) {
    __asm__ volatile(
        "lock incq %0"
        : "+m"(lapic_ticks)
        :
        : "memory"
    );
    lapic_eoi();
}
