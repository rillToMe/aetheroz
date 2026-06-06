#ifndef ATA_H
#define ATA_H

#include <stdint.h>

// Fungsi untuk membaca 1 sektor (512 byte) dari Hard Disk
void ata_read_sector(uint32_t lba, uint8_t* buffer);
void ata_write_sector(uint32_t lba, uint8_t* buffer);
uint32_t ata_get_total_sectors(void);
#endif