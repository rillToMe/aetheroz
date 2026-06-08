#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>

typedef struct {
    volatile uint32_t locked;
} spinlock_t;

#define SPINLOCK_INIT {0}

static inline uint64_t spinlock_irq_save(void) {
    uint64_t flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

static inline void spinlock_irq_restore(uint64_t flags) {
    if (flags & (1ULL << 9)) {
        __asm__ volatile("sti" ::: "memory");
    }
}

static inline void spinlock_lock(spinlock_t *lock) {
    for (;;) {
        uint32_t taken = 1;
        __asm__ volatile(
            "lock xchg %0, %1"
            : "+r"(taken), "+m"(lock->locked)
            :
            : "memory"
        );
        if (taken == 0) return;
        while (lock->locked) {
            __asm__ volatile("pause");
        }
    }
}

static inline void spinlock_unlock(spinlock_t *lock) {
    __asm__ volatile("" ::: "memory");
    lock->locked = 0;
}

static inline uint64_t spinlock_lock_irqsave(spinlock_t *lock) {
    uint64_t flags = spinlock_irq_save();
    spinlock_lock(lock);
    return flags;
}

static inline void spinlock_unlock_irqrestore(spinlock_t *lock, uint64_t flags) {
    spinlock_unlock(lock);
    spinlock_irq_restore(flags);
}

extern spinlock_t g_kernel_lock;

#endif
