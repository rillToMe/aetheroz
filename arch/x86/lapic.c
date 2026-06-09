#include <stdint.h>
#include <stddef.h>
#include "lapic.h"
#include "task.h"
#include "smp.h"

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

#define LAPIC_REG_ICR_LOW       0x300
#define LAPIC_REG_ICR_HIGH      0x310

#define LAPIC_ICR_FIXED         0x00000000U
#define LAPIC_ICR_DELIVERY_PEND (1U << 12)

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

void lapic_send_reschedule(uint32_t cpu_index) {
    percpu_t *target = smp_get_cpu(cpu_index);
    if (target == NULL || !target->online) return;
    if (lapic_mmio == 0) return;

    uint32_t dest_lapic = target->lapic_id;

    // Wait for previous IPI to be delivered
    uint32_t timeout = 100000;
    while ((lapic_read(LAPIC_REG_ICR_LOW) & LAPIC_ICR_DELIVERY_PEND) && timeout > 0) {
        __asm__ volatile("pause");
        timeout--;
    }

    // Set destination LAPIC ID in ICR high
    lapic_write(LAPIC_REG_ICR_HIGH, dest_lapic << 24);
    // Send fixed IPI with reschedule vector
    lapic_write(LAPIC_REG_ICR_LOW, LAPIC_ICR_FIXED | LAPIC_RESCHEDULE_VECTOR);
}

registers_t* lapic_timer_handler(registers_t* regs) {
    __asm__ volatile(
        "lock incq %0"
        : "+m"(lapic_ticks)
        :
        : "memory"
    );
    lapic_eoi();

    uint32_t cpu_id = smp_current_cpu_index();
    // BSP uses PIT timer (timer_handler) for scheduling; skip here.
    if (cpu_id == 0) {
        return regs;
    }

    return schedule_on_cpu(cpu_id, regs);
}

registers_t* lapic_reschedule_handler(registers_t* regs) {
    lapic_eoi();

    uint32_t cpu_id = smp_current_cpu_index();
    if (!smp_reschedule_pending(cpu_id)) {
        return regs;
    }

    return schedule_on_cpu(cpu_id, regs);
}
