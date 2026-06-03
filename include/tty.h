#ifndef TTY_H
#define TTY_H

#include "fs.h"

// Fungsi untuk menginisialisasi layar dan mengembalikan objek VFS-nya
fs_node_t* init_tty(void);
void tty_clear(void);

#endif