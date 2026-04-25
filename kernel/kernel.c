/* kernel/kernel.c */
#include "kernel.h"
#include "utils.h"
#include "../drivers/display.h" 
#include "../cpu/gdt.h"
#include "../cpu/idt.h"
#include "../cpu/ports.h"
#include "../logs/logger.h"
#include "../cpu/timer.h"
#include "../drivers/ata.h"
#include "../drivers/filesystem.h" 

/* FORWARD DECLARATIONS */
void save_users_to_disk(void);
void ramdisk_write(char* filename, char* text);
void draw_dashboard(void);
void draw_footer(void);
void storage_write(char* filename, char* text);
void start_editor_storage(char* filename);
void init_users(void);
void load_users_from_disk(void);

/* GLOBAL CONFIG */
int shift_pressed = 0;
int ctrl_pressed = 0;

/* USER DATABASE */
struct user_t {
    char username[32];
    char password[32];
    int active;
};
struct user_t users[10];  // Support up to 10 users
char current_user[32] = "guest";

/* PROCESS TABLE (Simple tracking) */
struct process_t {
    int pid;
    char name[32];
    char user[32];
    int active;
};
struct process_t processes[10];

void init_processes(void) {
    for(int i = 0; i < 10; i++) {
        processes[i].active = 0;
        processes[i].pid = 0;
    }
    // Add kernel process
    processes[0].active = 1;
    processes[0].pid = 0;
    strcpy(processes[0].name, "kernel");
    strcpy(processes[0].user, "system");
}

void cmd_ps(void) {
    terminal_writestring("PID  USER     COMMAND\n");
    terminal_writestring("---  -------  -------\n");
    for(int i = 0; i < 10; i++) {
        if(processes[i].active) {
            char pid_str[8];
            itoa(processes[i].pid, pid_str);
            terminal_writestring(pid_str);
            terminal_writestring("    ");
            terminal_writestring(processes[i].user);
            terminal_writestring("    ");
            terminal_writestring(processes[i].name);
            terminal_writestring("\n");
        }
    }
}

/* THEME STORAGE */
enum vga_color current_theme_fg = VGA_COLOR_LIGHT_GREEN;
enum vga_color current_theme_bg = VGA_COLOR_BLACK;

/* --- HARDWARE & TIME --- */
#define BCD_TO_BIN(val) ((val & 0x0F) + ((val >> 4) * 10))

static int is_leap_year(int year) {
    return (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
}

static int days_in_month(int year, int month) {
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int d = 31;
    if (month >= 1 && month <= 12) {
        d = days[month - 1];
    }
    if (month == 2 && is_leap_year(year)) {
        d = 29;
    }
    return d;
}

uint8_t get_rtc_register(int reg) {
    outb(0x70, reg);
    return inb(0x71);
}

long boot_time = 0;

long get_time_seconds() {
    uint8_t s = BCD_TO_BIN(get_rtc_register(0x00));
    uint8_t m = BCD_TO_BIN(get_rtc_register(0x02));
    uint8_t h = BCD_TO_BIN(get_rtc_register(0x04));
    return s + (m * 60) + (h * 3600);
}

/* Get BDT Time String (Dhaka +6) in 12-hour format */
void get_bdt_string(char* buf) {
    uint8_t s = BCD_TO_BIN(get_rtc_register(0x00));
    uint8_t m = BCD_TO_BIN(get_rtc_register(0x02));
    uint8_t h = BCD_TO_BIN(get_rtc_register(0x04));
    h = (h + 6) % 24; 
    
    // Convert to 12-hour format
    char* period = "AM";
    if (h >= 12) {
        period = "PM";
        if (h > 12) h -= 12;
    }
    if (h == 0) h = 12; // Midnight = 12 AM
    
    buf[0] = (h / 10) + '0'; buf[1] = (h % 10) + '0';
    buf[2] = ':';
    buf[3] = (m / 10) + '0'; buf[4] = (m % 10) + '0';
    buf[5] = ':';
    buf[6] = (s / 10) + '0'; buf[7] = (s % 10) + '0';
    buf[8] = ' ';
    buf[9] = period[0]; buf[10] = period[1];
    buf[11] = 0;
}

/* Get BDT Date String */
void get_bdt_date(char* buf) {
    int day = BCD_TO_BIN(get_rtc_register(0x07));
    int month = BCD_TO_BIN(get_rtc_register(0x08));
    int year = BCD_TO_BIN(get_rtc_register(0x09));
    int hour = BCD_TO_BIN(get_rtc_register(0x04));
    int full_year = 2000 + year;

    if (hour + 6 >= 24) {
        day += 1;
        if (day > days_in_month(full_year, month)) {
            day = 1;
            month += 1;
            if (month > 12) {
                month = 1;
                full_year += 1;
            }
        }
    }
    year = full_year % 100;
    
    // Format: DD/MM/20YY
    buf[0] = (day / 10) + '0'; buf[1] = (day % 10) + '0';
    buf[2] = '/';
    buf[3] = (month / 10) + '0'; buf[4] = (month % 10) + '0';
    buf[5] = '/';
    buf[6] = '2'; buf[7] = '0';
    buf[8] = (year / 10) + '0'; buf[9] = (year % 10) + '0';
    buf[10] = 0;
}

void print_time() {
    char time_buf[16], date_buf[16];
    get_bdt_string(time_buf);
    get_bdt_date(date_buf);
    terminal_writestring(date_buf);
    terminal_writestring(" ");
    terminal_writestring(time_buf);
    terminal_writestring(" BDT\n");
}

void uptime() {
    long current = get_time_seconds();
    long diff = current - boot_time;
    if(diff < 0) diff += 86400; 
    
    int days = diff / 86400;
    int hours = (diff % 86400) / 3600;
    int mins = (diff % 3600) / 60;
    int secs = diff % 60;
    
    char buf[16];
    terminal_writestring("Uptime: ");
    
    if (days > 0) {
        itoa(days, buf);
        terminal_writestring(buf);
        terminal_writestring(days == 1 ? " day, " : " days, ");
    }
    
    itoa(hours, buf);
    terminal_writestring(buf);
    terminal_writestring(":");
    
    if (mins < 10) terminal_writestring("0");
    itoa(mins, buf);
    terminal_writestring(buf);
    terminal_writestring(":");
    
    if (secs < 10) terminal_writestring("0");
    itoa(secs, buf);
    terminal_writestring(buf);
    terminal_writestring("\n");
}

void cmd_free() {
    terminal_writestring("=== MEMORY INFO ===\n");
    terminal_writestring("Total RAM: 512 MB\n");
    terminal_writestring("Kernel: ~1 MB\n");
    terminal_writestring("Free: ~511 MB\n");
}

void cmd_df() {
    uint32_t total, used, free_space;
    fs_get_stats(&total, &used, &free_space);
    
    terminal_writestring("=== DISK USAGE ===\n");
    terminal_writestring("Filesystem: 323FS\n");
    
    char buf[16];
    terminal_writestring("Total: ");
    itoa(total, buf);
    terminal_writestring(buf);
    terminal_writestring(" bytes\n");
    
    terminal_writestring("Used:  ");
    itoa(used, buf);
    terminal_writestring(buf);
    terminal_writestring(" bytes\n");
    
    terminal_writestring("Free:  ");
    itoa(free_space, buf);
    terminal_writestring(buf);
    terminal_writestring(" bytes\n");
}

void cmd_whoami() {
    terminal_writestring(current_user);
    terminal_writestring("\n");
}

void cmd_top() {
    terminal_writestring("=== SYSTEM MONITOR ===\n");
    print_time();
    uptime();
    terminal_writestring("\n");
    
    terminal_writestring("CPU: i386 Compatible\n");
    terminal_writestring("Processes: 1 (kernel)\n");
    cmd_free();
    terminal_writestring("\n");
    cmd_df();
}

void shutdown() {
    klog("Shutdown initiated.");
    terminal_writestring("Shutting down 323OS...\n");
    outw(0x604, 0x2000); outw(0xB004, 0x2000);
    terminal_writestring("Safe to power off.\n");
    asm volatile("hlt");
}

void reboot() {
    klog("Reboot initiated.");
    terminal_writestring("Rebooting...\n");
    uint8_t good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE); 
    asm volatile("hlt");
}

/* --- KEYBOARD HELPER --- */
char scancode_to_ascii(uint8_t scancode) {
    const char lower[] = {0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' '};
    const char upper[] = {0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b','\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S','D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V','B','N','M','<','>','?',0,'*',0,'~'};
    if (scancode > 128) return 0;
    return shift_pressed ? upper[scancode] : lower[scancode];
}

void apply_theme(char* name) {
    if (strcmp(name, "matrix") == 0) {
        current_theme_fg = VGA_COLOR_LIGHT_GREEN;
        current_theme_bg = VGA_COLOR_BLACK;
    }
    else if (strcmp(name, "cyber") == 0) {
        current_theme_fg = VGA_COLOR_LIGHT_CYAN;
        current_theme_bg = VGA_COLOR_BLUE;
    }
    else if (strcmp(name, "retro") == 0) {
        current_theme_fg = VGA_COLOR_BROWN;
        current_theme_bg = VGA_COLOR_BLACK;
    }
    else if (strcmp(name, "default") == 0) {
        current_theme_fg = VGA_COLOR_LIGHT_GREEN;
        current_theme_bg = VGA_COLOR_BLACK;
    }
    else {
        terminal_writestring("Unknown theme.\n");
        return;
    }
    terminal_set_color(vga_entry_color(current_theme_fg, current_theme_bg));
    terminal_clear_area(5);
    terminal_writestring("Theme applied.\n");
}

void memdump() {
    klog("Memory Dump requested.");
    terminal_writestring("--- MEMORY DUMP (Kernel Start) ---\n");
    uint8_t* ptr = (uint8_t*) 0x1000; 
    char buf[16];
    for(int i=0; i<16; i++) {
        itoa(ptr[i], buf);
        terminal_writestring(buf); terminal_writestring(" ");
    }
    terminal_writestring("\n");
}

void sysinfo() {
    terminal_writestring("OS: 323OS | User: ");
    terminal_writestring(current_user);
    terminal_writestring(" | ");
    print_time();
}

void draw_footer() {
    const char* text = " Spring_26_CSE323_2 || V1.0_25_04_2026_Final Demo";
    int len = strlen(text);
    int row = 24;
    int start = 0;

    if (len < 80) {
        start = 80 - len - 1;
        if (start < 0) {
            start = 0;
        }
    }

    uint8_t base_color = vga_entry_color(current_theme_fg, current_theme_bg);
    uint8_t text_color = vga_entry_color(VGA_COLOR_DARK_GREY, current_theme_bg);

    for (int i = 0; i < 80; i++) {
        terminal_putentryat(' ', base_color, i, row);
    }

    for (int i = 0; i < len && (start + i) < 80; i++) {
        terminal_putentryat(text[i], text_color, start + i, row);
    }
}

/* --- LIVE UI UPDATER --- */
void update_live_clock() {
    static int last_spin_tick = 0;
    if (tick - last_spin_tick > 10) { 
        last_spin_tick = tick;
        char spinner[] = {'|', '/', '-', '\\'};
        int spin_idx = (tick / 10) % 4;
        
        uint8_t color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
        terminal_putentryat('[', color, 77, 0);
        terminal_putentryat(spinner[spin_idx], color, 78, 0);
        terminal_putentryat(']', color, 79, 0);
    }

    static int last_sec_tick = 0;
    if (tick - last_sec_tick > 50) { 
        last_sec_tick = tick;
        char time_str[16], date_str[16];
        get_bdt_string(time_str);
        get_bdt_date(date_str);
        
        uint8_t color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
        
        int start_x = 34;
        
        for(int i=0; date_str[i] && i < 10; i++) 
            terminal_putentryat(date_str[i], color, start_x + i, 0);
        terminal_putentryat(' ', color, start_x + 10, 0);
        
        for(int i=0; time_str[i] && i < 11; i++) 
            terminal_putentryat(time_str[i], color, start_x + 11 + i, 0);
        terminal_putentryat(' ', color, start_x + 22, 0);
        
        char* label = "Dhaka";
        for(int i=0; label[i]; i++) 
            terminal_putentryat(label[i], color, start_x + 23 + i, 0);

        draw_footer();
    }
}

/* --- RAM DISK DRIVER --- */
struct file_t { char name[32]; char content[512]; int used; };
struct file_t ramdisk[10];

void init_ramdisk() {
    for(int i=0; i<10; i++) { ramdisk[i].used = 0; ramdisk[i].name[0] = 0; }
    klog("RAMDisk initialized.");
}

/* --- DISK STORAGE FUNCTIONS (Using Real ATA Disk) --- */
void storage_write(char* filename, char* text) {
    uint32_t size = strlen(text);
    if(fs_write_file(filename, text, size) == 0) {
        terminal_writestring("File saved to disk.\n");
        klog("File written to disk.");
    } else {
        terminal_writestring("Error: Disk full.\n");
    }
}

void storage_read(char* filename) {
    char buffer[2048];  // Larger buffer for multi-sector files
    uint32_t size;
    if(fs_read_file(filename, buffer, &size) == 0) {
        terminal_writestring("--- ");
        terminal_writestring(filename);
        terminal_writestring(" ---\n");
        terminal_writestring(buffer);
        terminal_writestring("\n");
    } else {
        terminal_writestring("File not found on disk.\n");
    }
}

void storage_delete(char* filename) {
    if(fs_delete_file(filename) == 0) {
        terminal_writestring("File deleted from disk.\n");
        klog("File deleted from disk.");
    } else {
        terminal_writestring("File not found.\n");
    }
}

void init_users() {
    for(int i=0; i<10; i++) {
        users[i].active = 0;
        users[i].username[0] = 0;
        users[i].password[0] = 0;
    }
    klog("User database initialized.");
}

void load_users_from_disk() {
    char buffer[512];
    uint32_t size;
    if(fs_read_file("users.db", buffer, &size) == 0) {
        char* ptr = buffer;
            int user_idx = 0;
            
            while(*ptr && user_idx < 10) {
                int name_idx = 0;
                while(*ptr && *ptr != ':' && name_idx < 31) {
                    users[user_idx].username[name_idx++] = *ptr++;
                }
                users[user_idx].username[name_idx] = 0;
                
                if(*ptr == ':') ptr++; 
                
                int pass_idx = 0;
                while(*ptr && *ptr != '\n' && pass_idx < 31) {
                    users[user_idx].password[pass_idx++] = *ptr++;
                }
                users[user_idx].password[pass_idx] = 0;
                
                if(*ptr == '\n') ptr++; 
                
                if(users[user_idx].username[0] != 0) {
                    users[user_idx].active = 1;
                    user_idx++;
                }
            }
        klog("Users loaded from disk.");
        return;
    }
    
    users[0].active = 1;
    strcpy(users[0].username, "admin");
    strcpy(users[0].password, "1234");
    save_users_to_disk();
    klog("Default admin user created.");
}

void save_users_to_disk() {
    char content[512];
    int pos = 0;
    
    for(int i=0; i<10; i++) {
        if(users[i].active) {
            for(int j=0; users[i].username[j] && pos < 510; j++) {
                content[pos++] = users[i].username[j];
            }
            if(pos < 510) content[pos++] = ':';
            
            for(int j=0; users[i].password[j] && pos < 510; j++) {
                content[pos++] = users[i].password[j];
            }
            if(pos < 510) content[pos++] = '\n';
        }
    }
    content[pos] = 0;
    
    fs_write_file("users.db", content, pos);
    klog("User database saved to disk.");
}

int verify_login(char* username, char* password) {
    for(int i=0; i<10; i++) {
        if(users[i].active && 
           strcmp(users[i].username, username) == 0 &&
           strcmp(users[i].password, password) == 0) {
            return 1; 
        }
    }
    return 0; 
}

void add_user(char* username, char* password) {
    for(int i=0; i<10; i++) {
        if(users[i].active && strcmp(users[i].username, username) == 0) {
            terminal_writestring("Error: User already exists.\n");
            return;
        }
    }
    
    for(int i=0; i<10; i++) {
        if(!users[i].active) {
            users[i].active = 1;
            strcpy(users[i].username, username);
            strcpy(users[i].password, password);
            save_users_to_disk();
            terminal_writestring("User added successfully.\n");
            klog("New user added.");
            return;
        }
    }
    
    terminal_writestring("Error: User database full.\n");
}

void list_users() {
    terminal_writestring("--- REGISTERED USERS ---\n");
    int found = 0;
    for(int i=0; i<10; i++) {
        if(users[i].active) {
            terminal_writestring("- ");
            terminal_writestring(users[i].username);
            terminal_writestring("\n");
            found = 1;
        }
    }
    if(!found) terminal_writestring("(no users)\n");
}

void ramdisk_ls() {
    terminal_writestring("--- FILES (RAM) ---\n");
    int found=0;
    for(int i=0; i<10; i++) {
        if(ramdisk[i].used) {
            terminal_writestring("- ");
            terminal_writestring(ramdisk[i].name); 
            terminal_writestring("\n");
            found=1;
        }
    }
    if(!found) terminal_writestring("(empty)\n");
}

void ramdisk_create(char* filename) {
    for(int i=0; i<10; i++) {
        if(ramdisk[i].used && strcmp(ramdisk[i].name, filename) == 0) {
             terminal_writestring("Error: File exists.\n"); return;
        }
    }
    for(int i=0; i<10; i++) {
        if(!ramdisk[i].used) {
            ramdisk[i].used = 1;
            int j=0; while(filename[j] && j<31) { ramdisk[i].name[j] = filename[j]; j++; }
            ramdisk[i].name[j] = 0;
            ramdisk[i].content[0] = 0;
            terminal_writestring("File created.\n");
            klog("New file created.");
            return;
        }
    }
    terminal_writestring("RAM Disk Full.\n");
}

void ramdisk_write(char* filename, char* text) {
    for(int i=0; i<10; i++) {
        if(ramdisk[i].used && strcmp(ramdisk[i].name, filename) == 0) {
             int j=0; while(text[j] && j<511) { ramdisk[i].content[j] = text[j]; j++; }
             ramdisk[i].content[j] = 0;
             terminal_writestring("Updated.\n"); 
             klog("File content updated.");
             return;
        }
    }
    for(int i=0; i<10; i++) {
        if(!ramdisk[i].used) {
            ramdisk[i].used = 1;
            int j=0; while(filename[j] && j<31) { ramdisk[i].name[j] = filename[j]; j++; }
            ramdisk[i].name[j] = 0;
            j=0; while(text[j] && j<511) { ramdisk[i].content[j] = text[j]; j++; }
            ramdisk[i].content[j] = 0;
            terminal_writestring("File saved.\n");
            klog("New file created/saved.");
            return;
        }
    }
    terminal_writestring("Disk Full.\n");
}

void ramdisk_delete(char* filename) {
    for(int i=0; i<10; i++) {
        if(ramdisk[i].used && strcmp(ramdisk[i].name, filename) == 0) {
            ramdisk[i].used = 0;
            ramdisk[i].name[0] = 0;
            terminal_writestring("File deleted.\n");
            klog("File deleted.");
            return;
        }
    }
    terminal_writestring("File not found.\n");
}

/* HISTORY SYSTEM */
char history[5][64];
int hist_idx = 0;
void add_history(char* cmd) {
    for(int i=0; i<63; i++) history[hist_idx][i] = cmd[i];
    history[hist_idx][63] = 0;
    hist_idx = (hist_idx + 1) % 5;
}
void show_history() {
    terminal_writestring("--- HISTORY ---\n");
    for(int i=0; i<5; i++) {
        if(history[i][0] != 0) {
            terminal_writestring(history[i]); terminal_writestring("\n");
        }
    }
}

static void help_wait_key(void) {
    terminal_writestring("---- Press any key for more ----\n");
    while (1) {
        update_live_clock();
        if (inb(0x64) & 1) {
            uint8_t scancode = inb(0x60);
            if (scancode == 0x1D) ctrl_pressed = 1;
            else if (scancode == 0x9D) ctrl_pressed = 0;
            else if (scancode == 0x2A || scancode == 0x36) shift_pressed = 1;
            else if (scancode == 0xAA || scancode == 0xB6) shift_pressed = 0;
            else if (scancode < 0x80) {
                terminal_clear_area(5);
                return;
            }
        }
    }
}

/* FORWARD DECLARE DRAW_DASHBOARD */
void draw_dashboard();

/* STORAGE EDITOR - LOADS EXISTING CONTENT FROM STORAGE */
void start_editor_storage(char* filename) {
    terminal_set_color(vga_entry_color(current_theme_fg, current_theme_bg));
    terminal_clear_area(0);
    terminal_writestring(" 323OS Editor -- Press 'Ctrl+S' to Save & Exit \n");
    terminal_writestring("File: ");
    terminal_writestring(filename);
    terminal_writestring("\n\n");
    
    char buffer[2048];  // Larger buffer for multi-sector files
    int idx = 0;
    buffer[0] = 0;

    uint32_t size;
    if(fs_read_file(filename, buffer, &size) == 0) {
        idx = size;
        terminal_writestring(buffer);
    }

    while(1) {
        update_live_clock();

        if (inb(0x64) & 1) {
            uint8_t scancode = inb(0x60);
            
            if(scancode == 0x1D) ctrl_pressed = 1;
            else if(scancode == 0x9D) ctrl_pressed = 0;
            else if(scancode == 0x2A || scancode == 0x36) shift_pressed=1;
            else if(scancode == 0xAA || scancode == 0xB6) shift_pressed=0;
            
            else if(scancode < 0x80) {
                if (ctrl_pressed && scancode == 0x1F) {
                    buffer[idx] = 0;
                    storage_write(filename, buffer);
                    terminal_clear_area(0);
                    return;
                }

                char c = scancode_to_ascii(scancode);
                if (c == '\b') {
                    if (idx > 0) { idx--; backspace(buffer); terminal_putc('\b'); }
                }
                else if (c == '\n') {
                    if (idx < 2046) { buffer[idx++] = '\n'; terminal_putc('\n'); }
                }
                else if (c != 0) {
                    if (idx < 2046) { buffer[idx++] = c; terminal_putc(c); }
                }
            }
        }
    }
}

/* EDITOR MODE - LOADS EXISTING CONTENT */
void start_editor(char* filename) {
    terminal_set_color(vga_entry_color(current_theme_fg, current_theme_bg));
    terminal_clear_area(0); /* Editor needs full screen */
    terminal_writestring(" 323OS Editor -- Press 'Ctrl+S' to Save & Exit \n");
    terminal_writestring("File: "); terminal_writestring(filename); terminal_writestring("\n\n");
    
    char buffer[512];
    int idx = 0;
    buffer[0] = 0;

    /* PRE-LOAD CONTENT */
    for(int i=0; i<10; i++) {
        if(ramdisk[i].used && strcmp(ramdisk[i].name, filename) == 0) {
            int c = 0;
            while(ramdisk[i].content[c] != 0 && c < 511) {
                buffer[c] = ramdisk[i].content[c];
                c++;
            }
            buffer[c] = 0;
            idx = c;
            
            terminal_writestring(buffer);
            break;
        }
    }

    while(1) {
        update_live_clock();

        if (inb(0x64) & 1) {
            uint8_t scancode = inb(0x60);
            
            if(scancode == 0x1D) ctrl_pressed = 1;
            else if(scancode == 0x9D) ctrl_pressed = 0;
            else if(scancode == 0x2A || scancode == 0x36) shift_pressed=1;
            else if(scancode == 0xAA || scancode == 0xB6) shift_pressed=0;
            
            else if(scancode < 0x80) {
                if (ctrl_pressed && scancode == 0x1F) {
                    buffer[idx] = 0;
                    ramdisk_write(filename, buffer);
                    terminal_clear_area(0); 
                    return; 
                }

                char c = scancode_to_ascii(scancode);
                if (c == '\b') {
                    if (idx > 0) { idx--; backspace(buffer); terminal_putc('\b'); }
                }
                else if (c == '\n') {
                    if (idx < 510) { buffer[idx++] = '\n'; terminal_putc('\n'); }
                }
                else if (c != 0) {
                    if (idx < 510) { buffer[idx++] = c; terminal_putc(c); }
                }
            }
        }
    }
}

int execute_command(char* input) {
    add_history(input);
    if (strcmp(input, "help") == 0) {
        terminal_clear_area(5);
        terminal_writestring("System:\n");
        terminal_writestring("  top     - system monitor (time, uptime, memory, disk)\n");
        terminal_writestring("  free    - memory summary\n");
        terminal_writestring("  df      - disk usage summary\n");
        terminal_writestring("  ps      - list running processes\n");
        terminal_writestring("  whoami  - show current user\n");
        terminal_writestring("  sysinfo - OS, user, and time info\n");
        terminal_writestring("  uptime  - time since boot\n");
        terminal_writestring("  time    - current date/time\n");

        help_wait_key();

        terminal_writestring("Files:\n");
        terminal_writestring("  ls      - list directory contents\n");
        terminal_writestring("  mkdir   - create a directory\n");
        terminal_writestring("  rmdir   - remove an empty directory\n");
        terminal_writestring("  cd      - change directory\n");
        terminal_writestring("  pwd     - show current directory\n");
        terminal_writestring("  tree    - show directory tree\n");
        terminal_writestring("  touch   - create or update a file\n");
        terminal_writestring("  edit    - open the text editor\n");
        terminal_writestring("  cat     - print file contents\n");
        terminal_writestring("  cp      - copy a file\n");
        terminal_writestring("  mv      - move or rename a file\n");
        terminal_writestring("  rm      - delete a file\n");

        help_wait_key();

        terminal_writestring("Admin:\n");
        terminal_writestring("  passwd  - change current user password\n");
        terminal_writestring("  useradd - add a new user\n");
        terminal_writestring("  users   - list users\n");
        terminal_writestring("  logs    - show kernel log\n");
        terminal_writestring("  format  - format the disk\n");

        help_wait_key();

        terminal_writestring("Other:\n");
        terminal_writestring("  calc    - simple calculator (e.g., calc 2+2)\n");
        terminal_writestring("  theme   - set color theme\n");
        terminal_writestring("  history - show recent commands\n");
        terminal_writestring("  clear   - clear the screen\n");
        terminal_writestring("  help    - show this help\n");
        terminal_writestring("  logout  - return to login\n");
        terminal_writestring("  shutdown - power off\n");
    } 
    else if (strcmp(input, "logout") == 0) return 1;
    else if (strcmp(input, "clear") == 0 || strcmp(input, "cls") == 0) { 
        terminal_clear_area(5); 
    }
    else if (strcmp(input, "sysinfo") == 0) sysinfo();
    else if (strcmp(input, "dmesg") == 0 || strcmp(input, "logs") == 0) klog_dump();
    else if (strcmp(input, "ls") == 0) {
        extern char* fs_get_cwd(void);
        fs_list_dir(fs_get_cwd());
    }
    else if (strcmp(input, "pwd") == 0) {
        extern char* fs_get_cwd(void);
        terminal_writestring(fs_get_cwd());
        terminal_writestring("\n");
    }
    else if (strcmp(input, "top") == 0) cmd_top();
    else if (strcmp(input, "free") == 0) cmd_free();
    else if (strcmp(input, "df") == 0) cmd_df();
    else if (strcmp(input, "whoami") == 0) cmd_whoami();
    else if (strcmp(input, "time") == 0) print_time();
    else if (strcmp(input, "date") == 0) print_time();
    else if (strcmp(input, "uptime") == 0) uptime();
    else if (strcmp(input, "history") == 0) show_history();
    else if (strcmp(input, "memdump") == 0) memdump();
    else if (strcmp(input, "exit") == 0 || strcmp(input, "shutdown") == 0) shutdown();
    else if (strcmp(input, "reboot") == 0) reboot();
    else if (input[0]=='t' && input[1]=='h' && input[2]=='e' && input[3]=='m' && input[4]=='e') {
        char* ptr = skip_spaces(input + 5);
        if (*ptr) apply_theme(ptr);
        else terminal_writestring("Usage: theme [matrix|cyber|retro|default]\n");
    }
    else if (input[0]=='p' && input[1]=='a' && input[2]=='s' && input[3]=='s' && input[4]=='w' && input[5]=='d') {
        char* ptr = skip_spaces(input + 6);
        if (*ptr) { 
            for(int i=0; i<10; i++) {
                if(users[i].active && strcmp(users[i].username, current_user) == 0) {
                    strcpy(users[i].password, ptr);
                    save_users_to_disk();
                    terminal_writestring("Password updated.\n"); 
                    klog("User password changed.");
                    return 0;
                }
            }
            terminal_writestring("Error: User not found.\n");
        } 
        else terminal_writestring("Usage: passwd [new_password]\n");
    }
    else if (input[0]=='u' && input[1]=='s' && input[2]=='e' && input[3]=='r' && input[4]=='a' && input[5]=='d' && input[6]=='d') {
        char* ptr = skip_spaces(input + 7);
        if (*ptr) {
            char username[32], password[32];
            int i = 0;
            while(*ptr && *ptr != ' ' && i < 31) username[i++] = *ptr++;
            username[i] = 0;
            
            ptr = skip_spaces(ptr);
            i = 0;
            while(*ptr && *ptr != ' ' && i < 31) password[i++] = *ptr++;
            password[i] = 0;
            
            if(username[0] && password[0]) {
                add_user(username, password);
            } else {
                terminal_writestring("Usage: useradd [username] [password]\n");
            }
        }
        else terminal_writestring("Usage: useradd [username] [password]\n");
    }
    else if (strcmp(input, "users") == 0) {
        list_users();
    }
    else if (input[0]=='c' && input[1]=='a' && input[2]=='t') {
        char* ptr = skip_spaces(input + 3);
        if (*ptr) storage_read(ptr);
        else terminal_writestring("Usage: cat [filename]\n");
    }
    else if (strcmp(input, "format") == 0) {
        terminal_writestring("Formatting disk...\n");
        fs_format();
        terminal_writestring("Disk formatted.\n");
    }
    else if (input[0]=='e' && input[1]=='d' && input[2]=='i' && input[3]=='t') {
        char* ptr = skip_spaces(input + 4);
        if (*ptr) {
            start_editor_storage(ptr);
            draw_dashboard();
        }
        else terminal_writestring("Usage: edit [filename]\n");
    }
    else if (input[0]=='r' && input[1]=='m') {
        char* ptr = skip_spaces(input + 2);
        if (*ptr) storage_delete(ptr);
        else terminal_writestring("Usage: rm [filename]\n");
    }
    else if (input[0]=='m' && input[1]=='k' && input[2]=='d' && input[3]=='i' && input[4]=='r') {
        char* ptr = skip_spaces(input + 5);
        if (*ptr) {
            extern int fs_mkdir(const char*);
            if (fs_mkdir(ptr) == 0) {
                terminal_writestring("Directory created.\n");
            } else {
                terminal_writestring("Error: Cannot create directory.\n");
            }
        } else {
            terminal_writestring("Usage: mkdir [dirname]\n");
        }
    }
    else if (input[0]=='c' && input[1]=='d') {
        char* ptr = skip_spaces(input + 2);
        if (*ptr) {
            extern int fs_chdir(const char*);
            if (fs_chdir(ptr) != 0) {
                terminal_writestring("Error: Directory not found.\n");
            }
        } else {
            extern int fs_chdir(const char*);
            fs_chdir("/");
        }
    }
    else if (input[0]=='c' && input[1]=='p') {
        char* ptr = skip_spaces(input + 2);
        if (*ptr) {
            char src[32], dest[32];
            int i = 0;
            while(*ptr && *ptr != ' ' && i < 31) src[i++] = *ptr++;
            src[i] = 0;
            ptr = skip_spaces(ptr);
            i = 0;
            while(*ptr && *ptr != ' ' && i < 31) dest[i++] = *ptr++;
            dest[i] = 0;
            
            if (src[0] && dest[0]) {
                extern int fs_copy_file(const char*, const char*);
                if (fs_copy_file(src, dest) == 0) {
                    terminal_writestring("File copied.\n");
                } else {
                    terminal_writestring("Error: Cannot copy file.\n");
                }
            } else {
                terminal_writestring("Usage: cp [source] [dest]\n");
            }
        } else {
            terminal_writestring("Usage: cp [source] [dest]\n");
        }
    }
    else if (input[0]=='m' && input[1]=='v') {
        char* ptr = skip_spaces(input + 2);
        if (*ptr) {
            char src[32], dest[32];
            int i = 0;
            while(*ptr && *ptr != ' ' && i < 31) src[i++] = *ptr++;
            src[i] = 0;
            ptr = skip_spaces(ptr);
            i = 0;
            while(*ptr && *ptr != ' ' && i < 31) dest[i++] = *ptr++;
            dest[i] = 0;
            
            if (src[0] && dest[0]) {
                extern int fs_move_file(const char*, const char*);
                if (fs_move_file(src, dest) == 0) {
                    terminal_writestring("File moved.\n");
                } else {
                    terminal_writestring("Error: Cannot move file.\n");
                }
            } else {
                terminal_writestring("Usage: mv [source] [dest]\n");
            }
        } else {
            terminal_writestring("Usage: mv [source] [dest]\n");
        }
    }
    else if (input[0]=='r' && input[1]=='m' && input[2]=='d' && input[3]=='i' && input[4]=='r') {
        char* ptr = skip_spaces(input + 5);
        if (*ptr) {
            extern int fs_rmdir(const char*);
            int result = fs_rmdir(ptr);
            if (result == 0) {
                terminal_writestring("Directory removed.\n");
            } else if (result == -2) {
                terminal_writestring("Error: Directory not empty.\n");
            } else {
                terminal_writestring("Error: Directory not found.\n");
            }
        } else {
            terminal_writestring("Usage: rmdir [dirname]\n");
        }
    }
    else if (input[0]=='t' && input[1]=='o' && input[2]=='u' && input[3]=='c' && input[4]=='h') {
        char* ptr = skip_spaces(input + 5);
        if (*ptr) {
            extern int fs_touch(const char*);
            if (fs_touch(ptr) == 0) {
                terminal_writestring("File created/updated.\n");
            } else {
                terminal_writestring("Error: Cannot create file.\n");
            }
        } else {
            terminal_writestring("Usage: touch [filename]\n");
        }
    }
    else if (input[0]=='t' && input[1]=='r' && input[2]=='e' && input[3]=='e') {
        extern void fs_tree(const char*, int);
        extern char* fs_get_cwd(void);
        fs_tree(fs_get_cwd(), 0);
    }
    else if (input[0]=='p' && input[1]=='s') {
        cmd_ps();
    }
    else if (input[0]=='c' && input[1]=='a' && input[2]=='l' && input[3]=='c') {
        char* ptr = skip_spaces(input+4);
        char n1[16]={0}, n2[16]={0}; int i=0;
        while(*ptr>='0' && *ptr<='9') n1[i++] = *ptr++;
        ptr = skip_spaces(ptr); char op = *ptr++; ptr = skip_spaces(ptr);
        i=0; while(*ptr>='0' && *ptr<='9') n2[i++] = *ptr++;
        int a = atoi(n1), b = atoi(n2), res=0;
        if(op=='+') res=a+b; else if(op=='-') res=a-b; else if(op=='*') res=a*b;
        else if(op=='/') { if(b==0) terminal_writestring("Div 0\n"); else res=a/b; }
        else if(op=='%') { if(b==0) terminal_writestring("Div 0\n"); else res=a%b; }
        if(!(op=='/' && b==0)) {
            char buf[32]; itoa(res, buf); 
            terminal_writestring("Result: "); terminal_writestring(buf); terminal_writestring("\n");
        }
    }
    else if (strlen(input) > 0) {
        terminal_writestring("Unknown command.\n");
        klog("Unknown command error.");
    }
    return 0; 
}

void draw_dashboard() {
    terminal_set_color(vga_entry_color(current_theme_fg, current_theme_bg));
    terminal_clear();
    
    for(int i=0; i<80; i++) terminal_putentryat(' ', vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE), i, 0);
    
    char header[80] = "  323OS  |  User: ";
    strcat(header, current_user);
    for(int i=0; header[i]; i++) terminal_putentryat(header[i], vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE), i, 0);
    
    terminal_set_color(vga_entry_color(current_theme_fg, current_theme_bg));
    terminal_writestring("\n"); 
    terminal_writestring("--------------------------------------------------------------------------------\n");
    terminal_writestring(" COMMANDS: ls  edit  cat  rm  logs  time  theme  logout  shutdown  help\n");
    terminal_writestring("--------------------------------------------------------------------------------\n");

    draw_footer();
}

void kernel_main(void) {
    terminal_initialize();
    init_gdt();
    init_idt();
    
    logger_init();
    klog("Kernel booting...");
    
    init_timer(50); 
    asm volatile("sti");
    klog("Interrupts Enabled.");
    
    init_ramdisk();
    ata_init();       
    fs_init();        
    init_users();
    init_processes(); 
    load_users_from_disk();
    boot_time = get_time_seconds();

    while(1) { 
        terminal_set_color(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_clear();
        terminal_writestring("\n\n   323OS Login\n   -----------\n");
        draw_footer();
        char username_buf[32];
        char password_buf[32];
        int buf_idx = 0;
        int logged_in = 0;

        while(!logged_in) {
            terminal_writestring("\n   Username: ");
            buf_idx = 0; username_buf[0] = 0;

            while(1) {
                update_live_clock(); 
                if (inb(0x64) & 1) {
                    uint8_t scancode = inb(0x60);
                    if(scancode == 0x2A || scancode == 0x36) shift_pressed=1;
                    else if(scancode == 0xAA || scancode == 0xB6) shift_pressed=0;
                    else if (scancode < 0x80) {
                        char c = scancode_to_ascii(scancode);
                        if (c == '\n') { 
                            terminal_putc('\n');
                            username_buf[buf_idx] = 0;
                            break;
                        } 
                        else if (c == '\b') {
                            if (buf_idx > 0) { buf_idx--; username_buf[buf_idx] = 0; terminal_putc('\b'); }
                        }
                        else if (c != 0 && buf_idx < 31) {
                            username_buf[buf_idx++] = c;
                            terminal_putc(c);
                        }
                    }
                }
            }

            terminal_writestring("   Password: ");
            buf_idx = 0; password_buf[0] = 0;

            while(1) {
                update_live_clock(); 
                if (inb(0x64) & 1) {
                    uint8_t scancode = inb(0x60);
                    if(scancode == 0x2A || scancode == 0x36) shift_pressed=1;
                    else if(scancode == 0xAA || scancode == 0xB6) shift_pressed=0;
                    else if (scancode < 0x80) {
                        char c = scancode_to_ascii(scancode);
                        if (c == '\n') { 
                            terminal_putc('\n');
                            password_buf[buf_idx] = 0;
                            if(verify_login(username_buf, password_buf)) {
                                logged_in = 1;
                                strcpy(current_user, username_buf);
                                klog("User logged in.");
                            } else {
                                terminal_writestring("   Access Denied.\n");
                            }
                            break;
                        } 
                        else if (c == '\b') {
                            if (buf_idx > 0) { buf_idx--; password_buf[buf_idx] = 0; terminal_putc('\b'); }
                        }
                        else if (c != 0 && buf_idx < 31) {
                            password_buf[buf_idx++] = c;
                            terminal_putc('*'); 
                        }
                    }
                }
            }
        }

        draw_dashboard();
        terminal_writestring("> ");
        
        char key_buffer[256];
        key_buffer[0] = '\0';
        int session_active = 1;

        while(session_active) {
            update_live_clock();

            if (inb(0x64) & 1) {
                uint8_t scancode = inb(0x60);
                
                if(scancode == 0x1D) ctrl_pressed = 1;
                else if(scancode == 0x9D) ctrl_pressed = 0;
                else if(scancode == 0x2A || scancode == 0x36) shift_pressed=1;
                else if(scancode == 0xAA || scancode == 0xB6) shift_pressed=0;
                else if(scancode < 0x80) {
                    char c = scancode_to_ascii(scancode);
                    
                    if (c == '\n') {
                        terminal_writestring("\n");
                        
                        if (execute_command(key_buffer) == 1) {
                            session_active = 0; 
                            klog("User logged out.");
                        } else {
                            key_buffer[0] = '\0'; 
                            if (session_active) terminal_writestring("> ");
                        }
                    }
                    else if (c == '\b') {
                        if (strlen(key_buffer) > 0) { backspace(key_buffer); terminal_putc('\b'); }
                    }
                    else if (c != 0) {
                        append(key_buffer, c); terminal_putc(c);
                    }
                }
            }
        }
    }
}
