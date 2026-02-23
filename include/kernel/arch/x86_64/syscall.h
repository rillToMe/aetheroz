#pragma once
#include <stdint.h>

void syscall_arch_init(void);
void enter_user_mode(uint64_t rip, uint64_t rsp);
void syscall_set_kernel_return(uint64_t rip, uint64_t rsp);
