#include <kernel/lib/util.h>

uint64_t align_up(uint64_t val) {
    return (val + 0xFFF) & ~0xFFFULL;
}

uint64_t align_down(uint64_t val) {
    return val & ~0xFFFULL;
}

void mem_set(uint8_t *dst, uint8_t value, uint64_t count) {
    for (uint64_t i = 0; i < count; i++) {
        dst[i] = value;
    }
}
