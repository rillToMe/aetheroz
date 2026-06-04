#ifndef KYUZENFS_H
#define KYUZENFS_H

#include <stdint.h>

#define FAT_FREE 0x0000 // Tanda Sektor Kosong
#define FAT_EOF  0xFFFF // Tanda Akhir File/Rantai

#define FLAG_EMPTY  0x00 // Slot kosong yang bisa ditimpa
#define FLAG_FILE   0x01 // Ini adalah File biasa
#define FLAG_FOLDER 0x02 // Ini adalah Folder / Direktori

// 1 Entri File = TEPAT 32 Byte (1 Sektor ATA 512 Byte bisa muat 16 entri)
typedef struct {
    char filename[23];          // Nama file lebih panjang (22 huruf + null)
    uint8_t flags;              // File, Folder, atau Kosong?
    uint32_t start_sector;      // Lokasi data file (atau isi folder) dimulai
    uint32_t size_bytes;        // Ukuran file
} __attribute__((packed)) kfs_file_entry_t;

// Header Master (Sektor 0) = 512 Byte
// Sekarang hanya menyimpan info sistem, TIDAK ADA LAGI daftar file di sini!
typedef struct {
    char magic[4];              // "KZFS"
    uint32_t root_dir_sector;   // Di sektor berapa Root Directory ("/") dimulai?
    uint32_t total_files;       // Total keseluruhan file di disk
    uint8_t padding[500];       // Sisa ruang kosong sektor 0
} __attribute__((packed)) kfs_header_t;

void kfs_init(void);
void kfs_format(void);
void kfs_list_files(void);
int kfs_create_file(char* filename, char* data, uint32_t size);
void kfs_read_file(char* filename);
void kfs_delete_file(char* filename);
int kfs_exists(char* filename);
uint32_t kfs_get_file_size(char* filename);
int kfs_read_to_buffer(char* filename, char* out_buffer);

#endif