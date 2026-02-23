#pragma once
#include <stdint.h>

typedef uint64_t (*syscall_t)(uint64_t, uint64_t, uint64_t, uint64_t);

void syscall_init(void);
uint64_t syscall_dispatch(uint64_t num,
                          uint64_t arg1,
                          uint64_t arg2,
                          uint64_t arg3,
                          uint64_t arg4);
