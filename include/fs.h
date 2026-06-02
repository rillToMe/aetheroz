#ifndef FS_H
#define FS_H

#include <stdint.h>

// Tipe-tipe node
#define FS_FILE        0x01
#define FS_DIRECTORY   0x02
#define FS_CHARDEVICE  0x03 // Keyboard & Layar masuk ke sini (Character Device)
#define FS_BLOCKDEVICE 0x04 // Hard disk masuk ke sini
#define FS_PIPE        0x05

struct fs_node;

// Definisi Function Pointer (Mirip interface/kontrak)
// Parameter: (node_yang_dituju, offset_baca, ukuran_baca, buffer_data)
typedef uint32_t (*read_type_t)(struct fs_node*, uint32_t, uint32_t, uint8_t*);
typedef uint32_t (*write_type_t)(struct fs_node*, uint32_t, uint32_t, uint8_t*);
typedef void (*open_type_t)(struct fs_node*);
typedef void (*close_type_t)(struct fs_node*);

// Struktur utama VFS (Setiap hardware/file akan dibungkus pakai ini)
typedef struct fs_node {
    char name[128];     // Nama file/device (contoh: "tty0" atau "keyboard")
    uint32_t flags;     // Tipe node (FS_FILE, FS_CHARDEVICE, dll)
    uint32_t length;    // Ukuran file
    uint32_t inode;     // ID unik node
    
    // Pointers ke fungsi spesifik milik driver
    read_type_t read;   
    write_type_t write; 
    open_type_t open;   
    close_type_t close; 
    
    struct fs_node *ptr; // Pointer tambahan jika driver butuh data kustom
} fs_node_t;

// Deklarasi fungsi global untuk dipakai kernel
uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
void open_fs(fs_node_t *node);
void close_fs(fs_node_t *node);

#endif