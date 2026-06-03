#include "ata.h"
#include "io.h"

#define ATA_DATA_PORT         0x1F0
#define ATA_SECTOR_COUNT_PORT 0x1F2
#define ATA_LBA_LO_PORT       0x1F3
#define ATA_LBA_MID_PORT      0x1F4
#define ATA_LBA_HI_PORT       0x1F5
#define ATA_DRIVE_PORT        0x1F6
#define ATA_COMMAND_PORT      0x1F7
#define ATA_STATUS_PORT       0x1F7

void ata_read_sector(uint32_t lba, uint8_t* buffer) {
    // 1. Pilih drive (Master = 0xE0) dan sisipkan 4 bit teratas dari LBA
    outb(ATA_DRIVE_PORT, 0xE0 | ((lba >> 24) & 0x0F));
    
    // 2. Tentukan jumlah sektor yang mau dibaca (1 sektor)
    outb(ATA_SECTOR_COUNT_PORT, 1);
    
    // 3. Kirim sisa 24 bit alamat LBA
    outb(ATA_LBA_LO_PORT, (uint8_t)lba);
    outb(ATA_LBA_MID_PORT, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI_PORT, (uint8_t)(lba >> 16));
    
    // 4. Kirim perintah READ SECTORS (0x20)
    outb(ATA_COMMAND_PORT, 0x20);
    
    // 5. Polling: Tunggu sampai Hard Disk membalas siap (Bit 3 / DRQ harus 1)
    uint8_t status = inb(ATA_STATUS_PORT);
    while ((status & 0x08) == 0) {
        status = inb(ATA_STATUS_PORT);
    }
    
    // 6. Sedot Datanya! 1 Sektor = 512 byte. 
    // Karena inw membaca 2 byte sekaligus, kita hanya perlu looping 256 kali.
    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        ptr[i] = inw(ATA_DATA_PORT);
    }
}

void ata_write_sector(uint32_t lba, uint8_t* buffer) {
    // 1. Pilih drive dan alamat LBA
    outb(ATA_DRIVE_PORT, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECTOR_COUNT_PORT, 1);
    outb(ATA_LBA_LO_PORT, (uint8_t)lba);
    outb(ATA_LBA_MID_PORT, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI_PORT, (uint8_t)(lba >> 16));
    
    // 2. Kirim perintah WRITE SECTORS (0x30)
    outb(ATA_COMMAND_PORT, 0x30);
    
    // 3. Tunggu sampai Hard Disk siap menerima data (DRQ bit = 1)
    uint8_t status = inb(ATA_STATUS_PORT);
    while ((status & 0x08) == 0) {
        status = inb(ATA_STATUS_PORT);
    }
    
    // 4. Suntikkan Data! 256 kali loop x 2 byte = 512 byte (1 sektor)
    uint16_t* ptr = (uint16_t*)buffer;
    for (int i = 0; i < 256; i++) {
        outw(ATA_DATA_PORT, ptr[i]);
    }
    
    // 5. SANGAT PENTING: Perintah CACHE FLUSH (0xE7)
    // Memaksa hard disk menyimpan data fisik sekarang juga.
    outb(ATA_COMMAND_PORT, 0xE7);
    status = inb(ATA_STATUS_PORT);
    // Tunggu sampai hard disk selesai (BSY bit = 0)
    while ((status & 0x80) != 0) {
        status = inb(ATA_STATUS_PORT);
    }
}