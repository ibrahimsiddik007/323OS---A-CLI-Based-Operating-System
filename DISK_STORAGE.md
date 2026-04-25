# Real Disk Storage Implementation

## Overview
323OS features **real persistent disk storage** using an ATA/IDE driver that writes to an actual disk image file. All file operations use the disk - there is no temporary RAM storage.

## Architecture

### Hardware Layer
- **ATA Driver** (`drivers/ata.c`) - Low-level disk I/O
  - Communicates with IDE controller (ports 0x1F0-0x1F7)
  - Reads/writes 512-byte sectors
  - PIO mode (Programmed I/O)

### Filesystem Layer
- **Simple Filesystem** (`drivers/filesystem.c`)
  - Sector 0: File table (up to 8 files)
  - Sectors 1+: Data storage
  - 512 bytes per file limit
  - No directory support (flat structure)

### Disk Image
- **File**: `disk.img` (10MB)
- **Format**: Raw disk image
- **Persistence**: Survives OS reboots and QEMU shutdowns

## Commands

### File Operations (All Persistent)
```bash
ls              # List all files on disk
edit file.txt   # Create/edit file (Ctrl+S to save)
cat file.txt    # Display file contents
rm file.txt     # Delete file from disk
format          # Format disk (WARNING: deletes all files!)
```

### User Database
- `users.db` is stored on the real disk
- Persists between reboots
- Use `useradd` to add users (saved to disk automatically)

## What's Persistent

**✅ Everything is persistent now!**
- All files created with `edit`
- User database (`users.db`)
- Survives power-off and reboots
- No temporary RAM storage

## Testing Persistence

1. Boot the OS: `./build.sh`
2. Login as admin (password: 1234)
3. **First time only**: `format` (initializes disk)
4. Add a user: `useradd bob secret`
5. Create a file: `edit test.txt` (type content, Ctrl+S)
6. List files: `ls`
7. Close QEMU completely
8. Rebuild and run: `./build.sh`
9. Login and check: `ls` shows test.txt ✓
10. Login as bob with password "secret" ✓

## Technical Details

### ATA Communication
```
Port 0x1F0: Data port (16-bit read/write)
Port 0x1F1: Error register
Port 0x1F2: Sector count
Port 0x1F3-0x1F5: LBA address
Port 0x1F6: Drive/head select
Port 0x1F7: Command/status
```

### File Table Structure
```c
typedef struct {
    char name[32];          // Filename
    uint32_t start_sector;  // Data sector number
    uint32_t size;          // File size in bytes
    uint8_t used;           // Entry active flag
} fs_entry_t;
```

## Limitations

- **File limit**: 8 files maximum
- **File size**: 512 bytes per file
- **No directories**: Flat filesystem only
- **No fragmentation handling**: One sector per file