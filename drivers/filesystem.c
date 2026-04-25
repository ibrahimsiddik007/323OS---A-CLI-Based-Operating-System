/* drivers/filesystem.c - Simple Filesystem Implementation */
#include "filesystem.h"
#include "ata.h"
#include "../kernel/utils.h"
#include "../drivers/display.h"
#include "../logs/logger.h"

static fs_entry_t file_table[FS_MAX_FILES];
static uint8_t sector_buffer[FS_SECTOR_SIZE];
static char current_directory[64] = "/";  // Current working directory

char* fs_get_cwd(void) {
    return current_directory;
}

int fs_chdir(const char* path) {
    if (strcmp(path, "/") == 0) {
        strcpy(current_directory, "/");
        return 0;
    }
    
    if (strcmp(path, "..") == 0) {
        int len = strlen(current_directory);
        if (len > 1) {
            for (int i = len - 2; i >= 0; i--) {
                if (current_directory[i] == '/') {
                    current_directory[i + 1] = 0;
                    if (i == 0) current_directory[1] = 0;
                    return 0;
                }
            }
        }
        return 0;
    }
    
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (file_table[i].used && file_table[i].is_directory) {
            if (strcmp(file_table[i].name, path) == 0) {
                if (current_directory[strlen(current_directory) - 1] != '/') {
                    strcat(current_directory, "/");
                }
                strcat(current_directory, path);
                return 0;
            }
        }
    }
    return -1;  // Not found
}

void fs_init(void) {
    ata_read_sector(FS_FILE_TABLE_SECTOR, sector_buffer);
    
    for(int i = 0; i < FS_MAX_FILES; i++) {
        fs_entry_t* entry = (fs_entry_t*)(sector_buffer + (i * sizeof(fs_entry_t)));
        file_table[i] = *entry;
    }
    
    klog("Filesystem initialized.");
}

void fs_format(void) {
    for(int i = 0; i < FS_MAX_FILES; i++) {
        file_table[i].used = 0;
        file_table[i].name[0] = 0;
        file_table[i].path[0] = 0;
        file_table[i].start_sector = 0;
        file_table[i].size = 0;
        file_table[i].is_directory = 0;
        file_table[i].owner[0] = 0;
        file_table[i].permissions = 6;  // Read+Write
    }
    
    for(int i = 0; i < FS_SECTOR_SIZE; i++) sector_buffer[i] = 0;
    for(int i = 0; i < FS_MAX_FILES; i++) {
        fs_entry_t* entry = (fs_entry_t*)(sector_buffer + (i * sizeof(fs_entry_t)));
        *entry = file_table[i];
    }
    ata_write_sector(FS_FILE_TABLE_SECTOR, sector_buffer);
    
    strcpy(current_directory, "/");
    klog("Filesystem formatted.");
}

void fs_save_table(void) {
    for(int i = 0; i < FS_SECTOR_SIZE; i++) sector_buffer[i] = 0;
    for(int i = 0; i < FS_MAX_FILES; i++) {
        fs_entry_t* entry = (fs_entry_t*)(sector_buffer + (i * sizeof(fs_entry_t)));
        *entry = file_table[i];
    }
    ata_write_sector(FS_FILE_TABLE_SECTOR, sector_buffer);
}

int fs_write_file(const char* name, const char* data, uint32_t size) {
    int slot = -1;
    int free_slot = -1;
    
    for(int i = 0; i < FS_MAX_FILES; i++) {
        if(file_table[i].used && strcmp(file_table[i].name, name) == 0) {
            slot = i;
            break;
        }
        if(!file_table[i].used && free_slot == -1) {
            free_slot = i;
        }
    }
    
    if(slot == -1) slot = free_slot;
    if(slot == -1) return -1; 
    
    uint32_t sectors_needed = (size + FS_SECTOR_SIZE - 1) / FS_SECTOR_SIZE;
    if (sectors_needed > FS_MAX_SECTORS_PER_FILE) sectors_needed = FS_MAX_SECTORS_PER_FILE;
    
    uint32_t start_sector = FS_DATA_START_SECTOR + (slot * FS_MAX_SECTORS_PER_FILE);
    
    file_table[slot].used = 1;
    strcpy(file_table[slot].name, (char*)name);
    strcpy(file_table[slot].path, current_directory);
    file_table[slot].start_sector = start_sector;
    file_table[slot].size = size;
    file_table[slot].is_directory = 0;
    file_table[slot].permissions = 6;  
    
    uint32_t remaining = size;
    uint32_t offset = 0;
    
    for (uint32_t s = 0; s < sectors_needed; s++) {
        for(int i = 0; i < FS_SECTOR_SIZE; i++) sector_buffer[i] = 0;
        
        uint32_t to_write = (remaining > FS_SECTOR_SIZE) ? FS_SECTOR_SIZE : remaining;
        for(uint32_t i = 0; i < to_write; i++) {
            sector_buffer[i] = data[offset + i];
        }
        
        ata_write_sector(start_sector + s, sector_buffer);
        offset += to_write;
        remaining -= to_write;
    }
    
    fs_save_table();
    
    return 0;
}

int fs_read_file(const char* name, char* buffer, uint32_t* size) {
    for(int i = 0; i < FS_MAX_FILES; i++) {
        if(file_table[i].used && strcmp(file_table[i].name, name) == 0) {
            *size = file_table[i].size;
            
            uint32_t sectors_needed = (*size + FS_SECTOR_SIZE - 1) / FS_SECTOR_SIZE;
            uint32_t offset = 0;
            
            for (uint32_t s = 0; s < sectors_needed && s < FS_MAX_SECTORS_PER_FILE; s++) {
                ata_read_sector(file_table[i].start_sector + s, sector_buffer);
                
                uint32_t to_read = (*size - offset > FS_SECTOR_SIZE) ? FS_SECTOR_SIZE : (*size - offset);
                for(uint32_t j = 0; j < to_read; j++) {
                    buffer[offset + j] = sector_buffer[j];
                }
                offset += to_read;
            }
            buffer[*size] = 0;
            
            return 0;
        }
    }
    
    return -1; 
}

void fs_list_files(void) {
    terminal_writestring("--- FILES ON DISK ---\n");
    int found = 0;
    
    for(int i = 0; i < FS_MAX_FILES; i++) {
        if(file_table[i].used) {
            terminal_writestring("- ");
            terminal_writestring(file_table[i].name);
            terminal_writestring(" (");
            char size_str[16];
            itoa(file_table[i].size, size_str);
            terminal_writestring(size_str);
            terminal_writestring(" bytes)\n");
            found = 1;
        }
    }
    
    if(!found) terminal_writestring("(empty)\n");
}

int fs_delete_file(const char* name) {
    for(int i = 0; i < FS_MAX_FILES; i++) {
        if(file_table[i].used && strcmp(file_table[i].name, name) == 0) {
            file_table[i].used = 0;
            file_table[i].name[0] = 0;
            fs_save_table();
            return 0;
        }
    }
    return -1;
}

int fs_mkdir(const char* name) {
    for(int i = 0; i < FS_MAX_FILES; i++) {
        if(!file_table[i].used) {
            file_table[i].used = 1;
            strcpy(file_table[i].name, (char*)name);
            strcpy(file_table[i].path, current_directory);
            file_table[i].is_directory = 1;
            file_table[i].size = 0;
            file_table[i].start_sector = 0;
            file_table[i].permissions = 7;  
            fs_save_table();
            return 0;
        }
    }
    return -1;  
}

int fs_copy_file(const char* src, const char* dest) {
    char buffer[2048];
    uint32_t size;
    
    if (fs_read_file(src, buffer, &size) == 0) {
        return fs_write_file(dest, buffer, size);
    }
    return -1;
}

int fs_move_file(const char* src, const char* dest) {
    if (fs_copy_file(src, dest) == 0) {
        return fs_delete_file(src);
    }
    return -1;
}

void fs_get_stats(uint32_t* total, uint32_t* used, uint32_t* free) {
    *total = FS_MAX_FILES * FS_MAX_SECTORS_PER_FILE * FS_SECTOR_SIZE;
    *used = 0;
    
    for(int i = 0; i < FS_MAX_FILES; i++) {
        if(file_table[i].used) {
            *used += file_table[i].size;
        }
    }
    
    *free = *total - *used;
}

void fs_list_dir(const char* path) {
    terminal_writestring("--- ");
    terminal_writestring((char*)path);
    terminal_writestring(" ---\n");
    int found = 0;
    
    for(int i = 0; i < FS_MAX_FILES; i++) {
        if(file_table[i].used && strcmp(file_table[i].path, path) == 0) {
            if (file_table[i].is_directory) {
                terminal_writestring("[DIR]  ");
            } else {
                terminal_writestring("[FILE] ");
            }
            terminal_writestring(file_table[i].name);
            if (!file_table[i].is_directory) {
                terminal_writestring(" (");
                char size_str[16];
                itoa(file_table[i].size, size_str);
                terminal_writestring(size_str);
                terminal_writestring(" bytes)");
            }
            terminal_writestring("\n");
            found = 1;
        }
    }
    
    if(!found) terminal_writestring("(empty)\n");
}

int fs_rmdir(const char* name) {
    for(int i = 0; i < FS_MAX_FILES; i++) {
        if(file_table[i].used && file_table[i].is_directory && 
           strcmp(file_table[i].name, name) == 0) {
            
            for(int j = 0; j < FS_MAX_FILES; j++) {
                if(file_table[j].used && strcmp(file_table[j].path, file_table[i].path) == 0 
                   && j != i) {
                    return -2;  
                }
            }
            
            file_table[i].used = 0;
            fs_save_table();
            return 0;
        }
    }
    return -1;  
}

int fs_touch(const char* name) {
    for(int i = 0; i < FS_MAX_FILES; i++) {
        if(file_table[i].used && strcmp(file_table[i].name, name) == 0) {
            return 0;
        }
    }
    
    return fs_write_file(name, "", 0);
}

void fs_tree_recursive(const char* path, int depth) {
    for(int i = 0; i < FS_MAX_FILES; i++) {
        if(file_table[i].used && strcmp(file_table[i].path, path) == 0) {
            for(int d = 0; d < depth; d++) {
                terminal_writestring("  ");
            }
            terminal_writestring("|-- ");
            
            if (file_table[i].is_directory) {
                terminal_writestring("[");
                terminal_writestring(file_table[i].name);
                terminal_writestring("]\n");
                
                char new_path[64];
                strcpy(new_path, path);
                if(new_path[strlen(new_path)-1] != '/') strcat(new_path, "/");
                strcat(new_path, file_table[i].name);
                fs_tree_recursive(new_path, depth + 1);
            } else {
                terminal_writestring(file_table[i].name);
                terminal_writestring("\n");
            }
        }
    }
}

void fs_tree(const char* path, int depth) {
    terminal_writestring(path);
    terminal_writestring("\n");
    fs_tree_recursive(path, depth);
}

int fs_check_permission(const char* name, const char* user, uint8_t perm) {
    for(int i = 0; i < FS_MAX_FILES; i++) {
        if(file_table[i].used && strcmp(file_table[i].name, name) == 0) {
            if(strcmp(file_table[i].owner, user) == 0 || strcmp(user, "root") == 0) {
                return (file_table[i].permissions & perm) != 0;
            }
            return 0;  
        }
    }
    return -1;  
}