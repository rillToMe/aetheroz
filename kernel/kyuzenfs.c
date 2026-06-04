#include "kyuzenfs.h"
#include "ata.h"
#include "heap.h"
#include "string.h"
#include "fs.h"

kfs_header_t current_fs; 
uint16_t fat_table[256]; 

extern fs_node_t tty_node;
extern void print_hex(uint32_t num); 

static uint32_t slen(const char* str) {
    uint32_t l = 0; while(str[l]) l++; return l;
}
void kprint(const char* str) {
    write_fs(&tty_node, 0, slen(str), (uint8_t*)str);
}

static uint16_t find_free_sector() {
    for (int i = 3; i < 256; i++) {
        if (fat_table[i] == FAT_FREE) return i;
    }
    return 0;
}

static int find_file_entry(char* filename, kfs_file_entry_t* out_entry, uint16_t* out_sec, int* out_idx) {
    // FAILSAFE: Jangan biarkan OS baca Sektor 0 sebagai folder!
    if (current_fs.root_dir_sector < 2) return 0;

    uint16_t curr_sec = current_fs.root_dir_sector;
    uint8_t* buffer = (uint8_t*)kmalloc(512);
    
    while (curr_sec != FAT_EOF && curr_sec != FAT_FREE) {
        ata_read_sector(curr_sec, buffer);
        kfs_file_entry_t* entries = (kfs_file_entry_t*)buffer;
        
        for (int i = 0; i < 16; i++) { 
            if (entries[i].flags != FLAG_EMPTY) {
                if (strcmp(entries[i].filename, filename) == 0) {
                    if (out_entry) *out_entry = entries[i];
                    if (out_sec) *out_sec = curr_sec;
                    if (out_idx) *out_idx = i;
                    kfree(buffer);
                    return 1; 
                }
            }
        }
        curr_sec = fat_table[curr_sec]; 
    }
    kfree(buffer);
    return 0; 
}

void kfs_format(void) {
    current_fs.magic[0] = 'K'; current_fs.magic[1] = 'Z';
    current_fs.magic[2] = 'F'; current_fs.magic[3] = 'S';
    current_fs.root_dir_sector = 2; // Booking sektor 2 mutlak!
    current_fs.total_files = 0;
    
    for (int i = 0; i < 256; i++) fat_table[i] = FAT_FREE;
    fat_table[0] = FAT_EOF; 
    fat_table[1] = FAT_EOF; 
    fat_table[2] = FAT_EOF; 
    
    uint8_t* buffer = (uint8_t*)kmalloc(512);
    
    memset(buffer, 0, 512); memcpy(buffer, &current_fs, 512); ata_write_sector(0, buffer);
    memset(buffer, 0, 512); memcpy(buffer, fat_table, 512); ata_write_sector(1, buffer);
    
    memset(buffer, FLAG_EMPTY, 512);
    ata_write_sector(2, buffer);
    
    kfree(buffer);
    kprint("[OK] Hard Disk diformat ke KyuzenFS V2!\n");
}

void kfs_init(void) {
    uint8_t* buffer = (uint8_t*)kmalloc(512);
    ata_read_sector(0, buffer);
    memcpy(&current_fs, buffer, sizeof(kfs_header_t));
    ata_read_sector(1, buffer);
    memcpy(fat_table, buffer, 512);
    kfree(buffer);

    // KUNCI UTAMA: Langsung format jika disk kosong/rusak!
    if (current_fs.magic[0] != 'K' || current_fs.magic[1] != 'Z') {
        kfs_format(); 
    }
}

int kfs_create_file(char* filename, char* data, uint32_t size) {
    // Failsafe format jika belum siap
    if (current_fs.root_dir_sector < 2) kfs_format();

    if (kfs_exists(filename)) { kprint("Error: File sudah ada!\n"); return 0; }
    
    uint32_t total_size = size;
    uint32_t sectors_needed = (total_size == 0) ? 1 : ((total_size + 511) / 512);
    uint16_t start_sector = 0, prev_sector = 0;
    uint32_t data_offset = 0, remaining = total_size;
    uint8_t* buffer = (uint8_t*)kmalloc(512);

    for (uint32_t i = 0; i < sectors_needed; i++) {
        uint16_t free_sec = find_free_sector();
        if (free_sec == 0) { kprint("Error: Disk Penuh!\n"); kfree(buffer); return 0; }

        fat_table[free_sec] = FAT_EOF;
        if (start_sector == 0) start_sector = free_sec;
        else fat_table[prev_sector] = free_sec;
        
        prev_sector = free_sec;

        uint32_t chunk_size = (remaining > 512) ? 512 : remaining;
        memset(buffer, 0, 512); memcpy(buffer, data + data_offset, chunk_size);
        ata_write_sector(free_sec, buffer);
        
        data_offset += chunk_size; remaining -= chunk_size;
    }

    uint16_t dir_sec = current_fs.root_dir_sector;
    uint16_t last_dir_sec = dir_sec;
    int found_idx = -1;
    
    while (dir_sec != FAT_EOF && dir_sec != FAT_FREE) {
        ata_read_sector(dir_sec, buffer);
        kfs_file_entry_t* entries = (kfs_file_entry_t*)buffer;
        
        for (int i = 0; i < 16; i++) {
            if (entries[i].flags == FLAG_EMPTY) { found_idx = i; break; }
        }
        if (found_idx != -1) break; 
        
        last_dir_sec = dir_sec;
        dir_sec = fat_table[dir_sec]; 
    }
    
    if (found_idx == -1) {
        uint16_t new_dir_sec = find_free_sector();
        if (new_dir_sec == 0) { kprint("Error: Gagal ekstensi direktori!\n"); kfree(buffer); return 0; }
        
        fat_table[last_dir_sec] = new_dir_sec;
        fat_table[new_dir_sec] = FAT_EOF;
        dir_sec = new_dir_sec;
        
        memset(buffer, FLAG_EMPTY, 512);
        found_idx = 0; 
    }
    
    kfs_file_entry_t* entries = (kfs_file_entry_t*)buffer;
    for(int i = 0; i < 22; i++) {
        entries[found_idx].filename[i] = filename[i];
        if(filename[i] == '\0') break;
    }
    entries[found_idx].filename[22] = '\0';
    entries[found_idx].flags = FLAG_FILE;
    entries[found_idx].start_sector = start_sector;
    entries[found_idx].size_bytes = total_size;
    ata_write_sector(dir_sec, buffer); 
    
    current_fs.total_files++;
    memset(buffer, 0, 512); memcpy(buffer, &current_fs, 512); ata_write_sector(0, buffer);
    memset(buffer, 0, 512); memcpy(buffer, fat_table, 512); ata_write_sector(1, buffer);
    
    kfree(buffer); return 1;
}

void kfs_list_files(void) {
    // === PROBE DETEKTIF: Bandingkan RAM vs Disk ===
    extern void kprint_num(uint32_t num);
    kprint("[PROBE] RAM: magic=");
    char m1[5] = {current_fs.magic[0], current_fs.magic[1],
                  current_fs.magic[2], current_fs.magic[3], '\0'};
    kprint(m1);
    kprint(" total_files="); kprint_num(current_fs.total_files);
    kprint(" root_dir="); kprint_num(current_fs.root_dir_sector);
    kprint("\n");

    uint8_t* verify_buf = (uint8_t*)kmalloc(512);
    ata_read_sector(0, verify_buf);
    kfs_header_t* disk_hdr = (kfs_header_t*)verify_buf;
    kprint("[PROBE] DISK: magic=");
    char m2[5] = {disk_hdr->magic[0], disk_hdr->magic[1],
                  disk_hdr->magic[2], disk_hdr->magic[3], '\0'};
    kprint(m2);
    kprint(" total_files="); kprint_num(disk_hdr->total_files);
    kprint(" root_dir="); kprint_num(disk_hdr->root_dir_sector);
    kprint("\n");
    kfree(verify_buf);
    // === END PROBE ===

    if (current_fs.magic[0] != 'K') { kprint("Disk belum diformat!\n"); return; }
    kprint("--- Isi Hard Disk ---\n");
    if (current_fs.total_files == 0) { kprint("(Kosong)\n"); return; }

    uint16_t curr_sec = current_fs.root_dir_sector;
    if (curr_sec < 2) return; // Failsafe
    
    uint8_t* buffer = (uint8_t*)kmalloc(512);
    
    while (curr_sec != FAT_EOF && curr_sec != FAT_FREE) {
        ata_read_sector(curr_sec, buffer);
        kfs_file_entry_t* entries = (kfs_file_entry_t*)buffer;
        
        for (int i = 0; i < 16; i++) {
            if (entries[i].flags != FLAG_EMPTY) {
                kprint("- "); kprint(entries[i].filename);
                if (entries[i].flags == FLAG_FOLDER) kprint(" [DIR]");
                kprint(" ("); print_hex(entries[i].size_bytes); kprint(" Bytes)\n");
            }
        }
        curr_sec = fat_table[curr_sec];
    }
    kfree(buffer);
}

void kfs_delete_file(char* filename) {
    kfs_file_entry_t entry; uint16_t dir_sec; int dir_idx;
    if (!find_file_entry(filename, &entry, &dir_sec, &dir_idx)) { kprint("Error: File tidak ada!\n"); return; }

    uint16_t curr = entry.start_sector;
    while(curr != FAT_EOF && curr != FAT_FREE) {
        uint16_t next = fat_table[curr]; fat_table[curr] = FAT_FREE; curr = next;
    }

    uint8_t* buffer = (uint8_t*)kmalloc(512);
    ata_read_sector(dir_sec, buffer);
    kfs_file_entry_t* entries = (kfs_file_entry_t*)buffer;
    entries[dir_idx].flags = FLAG_EMPTY; 
    ata_write_sector(dir_sec, buffer);

    current_fs.total_files--;
    memset(buffer, 0, 512); memcpy(buffer, &current_fs, 512); ata_write_sector(0, buffer);
    memset(buffer, 0, 512); memcpy(buffer, fat_table, 512); ata_write_sector(1, buffer);
    kfree(buffer); kprint("[OK] File dihapus!\n");
}

int kfs_exists(char* filename) { return find_file_entry(filename, NULL, NULL, NULL); }

uint32_t kfs_get_file_size(char* filename) {
    kfs_file_entry_t entry;
    if (find_file_entry(filename, &entry, NULL, NULL)) return entry.size_bytes;
    return 0;
}

int kfs_read_to_buffer(char* filename, char* out_buffer) {
    kfs_file_entry_t entry;
    if (!find_file_entry(filename, &entry, NULL, NULL)) return 0;
    
    uint16_t curr = entry.start_sector;
    uint32_t offset = 0; uint32_t remaining = entry.size_bytes;
    uint8_t* temp = (uint8_t*)kmalloc(512);
    
    while(curr != FAT_EOF && curr != FAT_FREE && remaining > 0) {
        ata_read_sector(curr, temp);
        uint32_t chunk = (remaining > 512) ? 512 : remaining;
        memcpy(out_buffer + offset, (char*)temp, chunk);
        offset += chunk; remaining -= chunk;
        curr = fat_table[curr];
    }
    kfree(temp); return 1;
}

void kfs_read_file(char* filename) {
    kfs_file_entry_t entry;
    if (!find_file_entry(filename, &entry, NULL, NULL)) { kprint("Error: File tidak ada!\n"); return; }
    
    uint16_t curr = entry.start_sector;
    uint8_t* buffer = (uint8_t*)kmalloc(513); 
    kprint("Isi file:\n");
    while(curr != FAT_EOF && curr != FAT_FREE) {
        memset(buffer, 0, 513); ata_read_sector(curr, buffer); kprint((char*)buffer);
        curr = fat_table[curr]; 
    }
    kprint("\n[EOF]\n"); kfree(buffer);
}

// JEMBATAN UNTUK GUI FILE MANAGER
int kfs_get_file_list(void* buffer, int max_entries) {
    typedef struct { char filename[24]; uint32_t size; uint8_t is_folder; } file_info_t;
    file_info_t* list = (file_info_t*)buffer;
    int count = 0;

    uint16_t curr_sec = current_fs.root_dir_sector;
    if (curr_sec < 2) return 0;

    uint8_t* temp = (uint8_t*)kmalloc(512);

    while (curr_sec != FAT_EOF && curr_sec != FAT_FREE && count < max_entries) {
        ata_read_sector(curr_sec, temp);
        kfs_file_entry_t* entries = (kfs_file_entry_t*)temp;
        
        for (int i = 0; i < 16; i++) {
            if (entries[i].flags != FLAG_EMPTY) {
                int j = 0;
                while(j < 22 && entries[i].filename[j] != '\0') {
                    list[count].filename[j] = entries[i].filename[j];
                    j++;
                }
                list[count].filename[j] = '\0';
                list[count].size = entries[i].size_bytes;
                list[count].is_folder = (entries[i].flags == FLAG_FOLDER) ? 1 : 0;
                
                count++;
                if (count >= max_entries) break;
            }
        }
        curr_sec = fat_table[curr_sec]; 
    }
    
    kfree(temp);
    return count; 
}