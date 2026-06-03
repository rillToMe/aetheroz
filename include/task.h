#ifndef TASK_H
#define TASK_H

#include <stdint.h>

// Struktur Data untuk 1 Program/Tugas
typedef struct {
    uint32_t esp;     // Menyimpan posisi stack memori program
    uint8_t active;   // Status aktif atau mati
} task_t;

void tasking_init(void);
void create_task(void (*func)());
void yield(void);     // Fungsi sakti untuk "mengoper bola" ke program lain

#endif