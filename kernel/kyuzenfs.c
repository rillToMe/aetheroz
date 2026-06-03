#include "kyuzenfs.h"
#include "ata.h"
#include "heap.h"
#include "string.h"
#include "fs.h"

kfs_header_t current_fs; 
uint16_t fat_table[256]; // Tabel FAT di RAM (Peta Rantai)

extern fs_node_t tty_node;
extern void print_hex(uint32_t num); // <--- TAMBAHKAN BARIS INI!

static uint32_t slen(const char* str) {
    uint32_t l = 0;
    while(str[l]) l++;
    return l;
}
static void kprint(const char* str) {
    write_fs(&tty_node, 0, slen(str), (uint8_t*)str);
}

// Cari sektor kosong di Tabel FAT
static uint16_t find_free_sector() {
    // Mulai dari 2, karena Sektor 0 (Header) & Sektor 1 (FAT) dilarang dipakai data!
    for (int i = 2; i < 256; i++) {
        if (fat_table[i] == FAT_FREE) return i;
    }
    return 0; // Disk penuh
}

void kfs_init(void) {
    uint8_t* buffer = (uint8_t*)kmalloc(512);
    // Baca Sektor 0 (Daftar Isi)
    ata_read_sector(0, buffer);
    memcpy(&current_fs, buffer, sizeof(kfs_header_t));
    
    // Baca Sektor 1 (Tabel FAT)
    ata_read_sector(1, buffer);
    memcpy(fat_table, buffer, 512);
    kfree(buffer);

    if (current_fs.magic[0] != 'K' || current_fs.magic[1] != 'Z') {
        current_fs.file_count = 0; 
    }
}

void kfs_format(void) {
    current_fs.magic[0] = 'K'; current_fs.magic[1] = 'Z';
    current_fs.magic[2] = 'F'; current_fs.magic[3] = 'S';
    current_fs.file_count = 0;
    
    // Kosongkan Tabel FAT
    for (int i = 0; i < 256; i++) fat_table[i] = FAT_FREE;
    fat_table[0] = FAT_EOF; // Kunci Sektor 0
    fat_table[1] = FAT_EOF; // Kunci Sektor 1
    
    uint8_t* buffer = (uint8_t*)kmalloc(512);
    // Tulis Header ke Sektor 0
    memset(buffer, 0, 512);
    memcpy(buffer, &current_fs, sizeof(kfs_header_t));
    ata_write_sector(0, buffer);
    
    // Tulis Tabel FAT ke Sektor 1
    memset(buffer, 0, 512);
    memcpy(buffer, fat_table, 512);
    ata_write_sector(1, buffer);
    
    kfree(buffer);
    kprint("[OK] Hard Disk berhasil diformat ke KyuzenFS (FAT Mode)!\n");
}

void kfs_list_files(void) {
    if (current_fs.magic[0] != 'K') { kprint("Disk belum diformat!\n"); return; }
    
    kprint("--- Isi Hard Disk ---\n");
    if (current_fs.file_count == 0) kprint("(Kosong)\n");
    else {
        for(uint32_t i = 0; i < current_fs.file_count; i++) {
            kprint("- "); kprint(current_fs.files[i].filename);
            kprint(" ("); print_hex(current_fs.files[i].size_bytes); kprint(" Bytes)\n");
        }
    }
}

// LOGIKA SAKTI: Memecah string panjang menjadi rantai sektor
int kfs_create_file(char* filename, char* data) {
    if (current_fs.file_count >= 21) { kprint("Error: Limit Daftar Isi Penuh!\n"); return 0; }
    
    uint32_t total_size = slen(data);
    // Hitung butuh berapa blok 512 byte
    uint32_t sectors_needed = (total_size == 0) ? 1 : ((total_size + 511) / 512);
    
    uint16_t start_sector = 0;
    uint16_t prev_sector = 0;
    uint32_t data_offset = 0;
    uint32_t remaining = total_size;

    uint8_t* buffer = (uint8_t*)kmalloc(512);

    for (uint32_t i = 0; i < sectors_needed; i++) {
        uint16_t free_sec = find_free_sector();
        if (free_sec == 0) { kprint("Error: Disk Penuh!\n"); kfree(buffer); return 0; }

        fat_table[free_sec] = FAT_EOF; // Booking sementara

        if (start_sector == 0) start_sector = free_sec;
        else fat_table[prev_sector] = free_sec; // Tautkan sektor sebelumnya ke sektor ini!
        
        prev_sector = free_sec;

        // Potong data maksimal 512 byte
        uint32_t chunk_size = (remaining > 512) ? 512 : remaining;
        memset(buffer, 0, 512);
        memcpy(buffer, data + data_offset, chunk_size);
        ata_write_sector(free_sec, buffer);
        
        data_offset += chunk_size;
        remaining -= chunk_size;
    }
    
    kfree(buffer);

    // Daftarkan ke Daftar Isi
    kfs_file_entry_t* new_file = &current_fs.files[current_fs.file_count];
    for(int i = 0; i < 15; i++) {
        new_file->filename[i] = filename[i];
        if(filename[i] == '\0') break;
    }
    new_file->filename[15] = '\0';
    new_file->start_sector = start_sector;
    new_file->size_bytes = total_size;
    
    current_fs.file_count++;
    
    // Simpan Header & FAT ke Hard Disk
    buffer = (uint8_t*)kmalloc(512);
    memset(buffer, 0, 512); memcpy(buffer, &current_fs, 512); ata_write_sector(0, buffer);
    memset(buffer, 0, 512); memcpy(buffer, fat_table, 512); ata_write_sector(1, buffer);
    kfree(buffer);
    
    kprint("[OK] File disimpan berantai!\n");
    return 1;
}

// LOGIKA SAKTI: Membaca menyusuri rantai
void kfs_read_file(char* filename) {
    for(uint32_t i = 0; i < current_fs.file_count; i++) {
        if (strcmp(current_fs.files[i].filename, filename) == 0) {
            uint16_t curr_sector = current_fs.files[i].start_sector;
            
            // Sewa 513 byte agar pastikan ada Null Terminator di ujung
            uint8_t* buffer = (uint8_t*)kmalloc(513); 
            kprint("Isi file:\n");
            
            // Susuri rantai FAT sampai ketemu EOF (0xFFFF)!
            while(curr_sector != FAT_EOF && curr_sector != FAT_FREE) {
                memset(buffer, 0, 513);
                ata_read_sector(curr_sector, buffer);
                kprint((char*)buffer);
                
                curr_sector = fat_table[curr_sector]; // Loncat ke rantai berikutnya
            }
            kprint("\n[EOF]\n");
            kfree(buffer);
            return;
        }
    }
    kprint("Error: File tidak ditemukan!\n");
}

void kfs_delete_file(char* filename) {
    int found = -1;
    for(uint32_t i = 0; i < current_fs.file_count; i++) {
        if (strcmp(current_fs.files[i].filename, filename) == 0) {
            found = i; break;
        }
    }
    if (found == -1) { kprint("Error: Tidak ada file itu!\n"); return; }

    // Hapus rantai sektor di FAT
    uint16_t curr = current_fs.files[found].start_sector;
    while(curr != FAT_EOF && curr != FAT_FREE) {
        uint16_t next = fat_table[curr];
        fat_table[curr] = FAT_FREE; // Kosongkan
        curr = next;
    }

    // Geser Daftar Isi
    for (uint32_t i = found; i < current_fs.file_count - 1; i++) {
        current_fs.files[i] = current_fs.files[i + 1];
    }
    current_fs.file_count--;

    // Simpan Header & FAT ke Hard Disk
    uint8_t* buffer = (uint8_t*)kmalloc(512);
    memset(buffer, 0, 512); memcpy(buffer, &current_fs, 512); ata_write_sector(0, buffer);
    memset(buffer, 0, 512); memcpy(buffer, fat_table, 512); ata_write_sector(1, buffer);
    kfree(buffer);

    kprint("[OK] File & Rantai Sektornya dihapus!\n");
}

// Mengecek apakah file ada di disk
int kfs_exists(char* filename) {
    for(uint32_t i = 0; i < current_fs.file_count; i++) {
        if (strcmp(current_fs.files[i].filename, filename) == 0) return 1;
    }
    return 0;
}

// Menyalin isi file secara presisi agar tidak menghancurkan Heap!
int kfs_read_to_buffer(char* filename, char* out_buffer) {
    for(uint32_t i = 0; i < current_fs.file_count; i++) {
        if (strcmp(current_fs.files[i].filename, filename) == 0) {
            uint16_t curr = current_fs.files[i].start_sector;
            uint32_t file_size = current_fs.files[i].size_bytes; // Ambil ukuran asli file
            uint32_t offset = 0;
            uint32_t remaining = file_size;
            
            uint8_t* temp = (uint8_t*)kmalloc(512);
            
            // Looping selama masih ada sisa byte yang harus dibaca
            while(curr != FAT_EOF && curr != FAT_FREE && remaining > 0) {
                memset(temp, 0, 512);
                ata_read_sector(curr, temp);
                
                // Hitung berapa byte yang boleh di-copy (Max 512, atau seadanya sisa)
                uint32_t chunk_size = (remaining > 512) ? 512 : remaining;
                memcpy(out_buffer + offset, (char*)temp, chunk_size);
                
                offset += chunk_size;
                remaining -= chunk_size;
                curr = fat_table[curr];
            }
            kfree(temp);
            return 1;
        }
    }
    return 0; // File tidak ditemukan
}

// Ambil ukuran file asli dari Daftar Isi
uint32_t kfs_get_file_size(char* filename) {
    for(uint32_t i = 0; i < current_fs.file_count; i++) {
        if (strcmp(current_fs.files[i].filename, filename) == 0) {
            return current_fs.files[i].size_bytes;
        }
    }
    return 0; // File tidak ditemukan
}