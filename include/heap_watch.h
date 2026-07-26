#ifndef HEAP_WATCH_H
#define HEAP_WATCH_H
#ifdef HEAP_WATCH_DEBUG
#include "task.h"
#include <stdint.h>
extern uint64_t heap_watch_target_addr;
void heap_watch_set(uint64_t addr, int len_bytes);
void serial_print_hex(uint64_t v);
void heap_watch_db_handler(registers_t *r);
#endif
#endif
