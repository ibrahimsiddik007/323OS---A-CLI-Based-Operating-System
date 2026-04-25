/* drivers/filesystem.h */
#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdint.h>

/* Simple Filesystem Structure
 * Sector 0: File Table (up to 16 files with directories)
 * Sectors 1-100: Data sectors (multi-sector support)
 */

#define FS_MAX_FILES 16
#define FS_FILE_TABLE_SECTOR 0
#define FS_DATA_START_SECTOR 1
#define FS_SECTOR_SIZE 512
#define FS_MAX_SECTORS_PER_FILE 4  // 2KB max per file

typedef struct {
    char name[28];              // Filename
    char path[32];              // Full path
    uint32_t start_sector;      // Starting sector for file data
    uint32_t size;              // File size in bytes
    uint8_t used;               // 1 if entry is used
    uint8_t is_directory;       // 1 if directory
    char owner[16];             // Owner username
    uint8_t permissions;        // Read(4), Write(2), Execute(1)
} fs_entry_t;

void fs_init(void);
void fs_format(void);
int fs_write_file(const char* name, const char* data, uint32_t size);
int fs_read_file(const char* name, char* buffer, uint32_t* size);
void fs_list_files(void);
void fs_list_dir(const char* path);
int fs_delete_file(const char* name);
int fs_mkdir(const char* name);
int fs_rmdir(const char* name);
int fs_touch(const char* name);
int fs_copy_file(const char* src, const char* dest);
int fs_move_file(const char* src, const char* dest);
void fs_get_stats(uint32_t* total, uint32_t* used, uint32_t* free);
void fs_tree(const char* path, int depth);
char* fs_get_cwd(void);
int fs_chdir(const char* path);
int fs_check_permission(const char* name, const char* user, uint8_t perm);

#endif
