#include "fs.h"
#include <stdint.h>

extern fs_node_t tty_node;
extern uint32_t string_length(const char* str);
extern uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
extern void tty_clear(void);

// Struktur pelacak register (urutan terbalik dari cara pushad bekerja)
typedef struct {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
} registers_t;

// Fungsi ini menerima pointer dari assembly dan mengembalikan nilai ke EAX
uint32_t syscall_handler(registers_t *r) {
    // r->eax berisi Nomor System Call!
    
    if (r->eax == 1) { 
        // --- SYSCALL 1: PRINT TEKS ---
        // Parameter EBX berisi alamat memori teksnya
        char* text = (char*)r->ebx;
        write_fs(&tty_node, 0, string_length(text), (uint8_t*)text);
        return 0; // Kembalikan 0 (Sukses)
    } 
    else if (r->eax == 2) {
        // --- SYSCALL 2: CLEAR SCREEN ---
        tty_clear();
        return 0;
    }

    return (uint32_t)-1; // Kembalikan error jika nomor syscall tidak dikenali
}