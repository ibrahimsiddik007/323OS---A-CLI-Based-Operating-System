/* drivers/ata.h */
#ifndef ATA_H
#define ATA_H

#include <stdint.h>

/* ATA Ports */
#define ATA_PRIMARY_DATA       0x1F0
#define ATA_PRIMARY_ERROR      0x1F1
#define ATA_PRIMARY_SECCOUNT   0x1F2
#define ATA_PRIMARY_LBA_LO     0x1F3
#define ATA_PRIMARY_LBA_MID    0x1F4
#define ATA_PRIMARY_LBA_HI     0x1F5
#define ATA_PRIMARY_DRIVE      0x1F6
#define ATA_PRIMARY_STATUS     0x1F7
#define ATA_PRIMARY_COMMAND    0x1F7

/* ATA Commands */
#define ATA_CMD_READ_SECTORS   0x20
#define ATA_CMD_WRITE_SECTORS  0x30
#define ATA_CMD_IDENTIFY       0xEC

/* ATA Status Bits */
#define ATA_SR_BSY   0x80    // Busy
#define ATA_SR_DRDY  0x40    // Drive ready
#define ATA_SR_DRQ   0x08    // Data request ready
#define ATA_SR_ERR   0x01    // Error

/* Function Prototypes */
void ata_init(void);
int ata_read_sector(uint32_t lba, uint8_t* buffer);
int ata_write_sector(uint32_t lba, uint8_t* buffer);
void ata_wait_ready(void);

#endif
