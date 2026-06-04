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

void sys_draw_pixel(int x, int y, uint32_t color);

void sys_draw_image(int x, int y, int width, int height, uint32_t* buffer);

typedef struct {
    char filename[24];
    uint32_t size;
    uint8_t is_folder;
} file_info_t;

int sys_get_file_list(file_info_t* buffer, int max_entries);

uint32_t sys_load_elf(char* filename);

void sys_draw_string(const char* str, int x, int y, uint32_t color);

void sys_set_uid(uint32_t uid);
uint32_t sys_get_uid();

#endif
