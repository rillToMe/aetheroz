#ifndef USERLIB_H
#define USERLIB_H

#include <stdint.h>

void print(char* text);
void clear_screen(void);
uint32_t read_keyboard(char* buffer, uint32_t size);
void sys_yield(void);

void fs_format(void);
void fs_list(void);
void fs_read(char* filename);
void fs_delete(char* filename);

void* sys_alloc(uint32_t size);
void sys_free(void* ptr);
void* sys_realloc(void* ptr, uint32_t old_size, uint32_t new_size);
int sys_file_exists(char* filename);
uint32_t sys_file_size(char* filename);
int sys_read_file_to_buffer(char* filename, char* buffer);
int sys_create_file(char* filename, char* data, uint32_t size);

uint32_t sys_uptime(void);
uint32_t sys_total_ram(void);
uint32_t sys_used_ram(void);

void print_num(uint32_t num);
extern int strcmp(const char *s1, const char *s2);

void get_cpu_string(char* buffer);


void sys_get_time(uint32_t* time_array);
#endif