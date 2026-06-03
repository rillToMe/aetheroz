#ifndef KYUZENFS_H
#define KYUZENFS_H

#include <stdint.h>

#define FAT_FREE 0x0000 // Tanda Sektor Kosong
#define FAT_EOF  0xFFFF // Tanda Akhir File (End of File)

// 1 Entri File = 24 Byte
typedef struct {
    char filename[16];          // Nama file (maksimal 15 huruf + null)
    uint32_t start_sector;      // Lokasi file ini di Hard Disk (Sektor ke berapa?)
    uint32_t size_bytes;   // Ukuran file
} __attribute__((packed)) kfs_file_entry_t;

// Header Master (Daftar Isi) = 512 Byte (Pas 1 Sektor ATA!)
// 4 byte (magic) + 4 byte (count) + (21 * 24 byte) = 512 byte.
typedef struct {
    char magic[4];              // Penanda "KZFS" (Kyuzen File System)
    uint32_t file_count;        // Jumlah file yang ada di disk saat ini
    kfs_file_entry_t files[21]; // Tabel indeks untuk 21 file maksimal
} __attribute__((packed)) kfs_header_t;

void kfs_init(void);
void kfs_format(void);
void kfs_list_files(void);
int kfs_create_file(char* filename, char* data);
void kfs_read_file(char* filename);
void kfs_delete_file(char* filename); // Fitur Baru: Hapus File!

int kfs_exists(char* filename);
int kfs_read_to_buffer(char* filename, char* out_buffer);
#endif