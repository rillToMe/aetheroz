#ifndef TTY_H
#define TTY_H

#include <stdint.h>
#include "fs.h"

// Fungsi untuk menginisialisasi layar dan mengembalikan objek VFS-nya
fs_node_t* init_tty(void);
void tty_clear(void);

// Scrollback: geser jendela tampilan `delta` baris (positif = ke riwayat lama,
// negatif = kembali ke output terbaru). Dipanggil Phase 4 dari mouse wheel.
void tty_scroll_view(int32_t delta_lines);

#endif