/* drivers/ata.c - IDE/ATA Hard Disk Driver (PIO Mode) */
#include "ata.h"
#include "../cpu/ports.h"
#include "../logs/logger.h"

/* Polling: Wait for the disk to be ready (BSY bit clear) */
void ata_wait_ready() {
    while (inb(ATA_PRIMARY_STATUS) & ATA_SR_BSY);
}

/* Polling: Wait for the disk to be ready for data transfer (DRQ bit set) */
void ata_wait_drq() {
    while (!(inb(ATA_PRIMARY_STATUS) & ATA_SR_DRQ));
}

/* Detection: Identifies if a primary master drive exists */
void ata_init() {
    klog("Initializing ATA disk driver...");
    // Disable ATA interrupts (use polling mode only)
    outb(0x3F6, 0x02);  // nIEN bit = 1, disable IRQ14
    outb(ATA_PRIMARY_DRIVE, 0xA0); 
    ata_wait_ready();
    klog("ATA disk driver ready.");
}

/**
 * @brief Reads 512 bytes (1 sector) from the disk.
 * 
 * @param lba    Logical Block Address (The sector index)
 * @param buffer Pointer to 512-byte destination memory
 */
int ata_read_sector(uint32_t lba, uint8_t* buffer) {
    ata_wait_ready();

    // Select drive and bits 24-27 of LBA
    outb(ATA_PRIMARY_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_SECCOUNT, 1); // We want 1 sector
    
    // Send LBA bits 0-23
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)lba);
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)(lba >> 16));
    
    // Send READ command
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_SECTORS);

    ata_wait_drq(); // Wait for data to be ready in the disk buffer

    // Read 256 16-bit values (512 bytes) from the Data Port
    for (int i = 0; i < 256; i++) {
        uint16_t data = inw(ATA_PRIMARY_DATA);
        buffer[i * 2] = (uint8_t)data;
        buffer[i * 2 + 1] = (uint8_t)(data >> 8);
    }

    return 0;
}

/**
 * @brief Writes 512 bytes (1 sector) to the disk.
 */
int ata_write_sector(uint32_t lba, uint8_t* buffer) {
    ata_wait_ready();

    outb(ATA_PRIMARY_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_SECCOUNT, 1);
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)lba);
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)(lba >> 16));
    
    // Send WRITE command
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE_SECTORS);

    ata_wait_drq(); // Wait until disk controller is ready to receive data

    // Send 512 bytes to the Data Port
    for (int i = 0; i < 256; i++) {
        uint16_t data = buffer[i * 2] | (buffer[i * 2 + 1] << 8);
        outw(ATA_PRIMARY_DATA, data);
    }

    // Wait for the drive to finish physical writing
    ata_wait_ready();
    return 0;
}