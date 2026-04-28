# 323OS Complete Technical Documentation
**Version 3.6** - A Custom x86 Operating System Written in C and Assembly

---

## Plain-English Orientation (Read This First)
- **What it is:** A tiny teaching OS that boots on a PC, shows a text screen, lets you log in, run simple commands, and save files to a virtual disk.
- **How it works (story version):** GRUB loads the kernel → the kernel sets up the CPU’s safety rails (GDT/IDT) → turns on interrupts (so keyboard/timer work) → starts a 50-times-per-second heartbeat → draws a login screen → after you log in, it listens for keystrokes, runs commands, and redraws the screen.
- **What you can do:** Log in, view time/uptime, list/make/delete files and folders, edit text files, change themes, add users, view logs, and shut down/reboot.
- **What’s purposely simple:** No apps outside the shell/editor, no networking, no multitasking, no memory protection, passwords stored in plain text, files max 2 KB each, and only 16 file slots.
- **Mental model:** Think of it as a single-loop program that keeps checking the keyboard, updating the clock, and running small commands, while using a very small homemade filesystem on a virtual disk.

### Jargon-to-Plain Words
- **GDT:** Table that tells the CPU what memory areas are valid. Think “memory zone map.”
- **IDT:** Table that says “when event X happens, call function Y.” Think “emergency contact list.”
- **Interrupt:** A hardware “hey, pay attention!”—used for keyboard presses and timer ticks.
- **PIT timer:** The 50-per-second heartbeat that keeps time and animations moving.
- **VGA text buffer:** A special memory area; writing letters here makes them appear on screen.
- **ATA driver:** Code that reads/writes 512-byte blocks on the virtual hard disk.
- **Filesystem:** A simple table that remembers filenames, sizes, and where each file’s data lives.
- **RTC:** The real-time clock chip that holds the date/time even when the PC is off.

If you only have 60 seconds, skim the bullet points above, then read the “Boot Process” and “How to Demo” sections below. Everything else goes deeper for curious readers.

---

## Table of Contents
1. [Overview](#overview)
2. [Boot Process](#boot-process)
3. [CPU & Memory Management](#cpu--memory-management)
4. [Hardware Drivers](#hardware-drivers)
5. [Filesystem](#filesystem)
6. [User Interface](#user-interface)
7. [Complete Code Walkthrough](#complete-code-walkthrough)
8. [Build System](#build-system)
9. [How to Demo](#how-to-demo)

---

## Overview

### What is 323OS?
323OS is a minimal operating system kernel designed for x86 architecture (32-bit). It demonstrates fundamental OS concepts including:
- **CPU protection mechanisms** (GDT, IDT)
- **Hardware interrupt handling** (Timer, Keyboard)
- **Device drivers** (VGA display, ATA hard disk, Keyboard)
- **Persistent file storage** (Custom filesystem called 323FS)
- **Multi-user authentication**
- **Interactive shell** with built-in commands

### Architecture
```
Hardware Layer (x86 PC)
    ↓
GRUB Bootloader
    ↓
Kernel (kernel.asm → kernel_main)
    ↓
┌─────────────────────────────────────┐
│  CPU Layer (GDT, IDT, Interrupts)   │
├─────────────────────────────────────┤
│  Drivers (Display, ATA Disk, KB)    │
├─────────────────────────────────────┤
│  Filesystem (323FS)                │
├─────────────────────────────────────┤
│  Application Layer (Shell, Editor)  │
└─────────────────────────────────────┘
```

---

## Boot Process

### Step 1: GRUB Bootloader (grub.cfg)
```
menuentry "323OS" {
    multiboot /boot/kernel.bin
}
```
**What happens:**
- GRUB loads the kernel binary into memory at address `0x100000` (1MB)
- Checks for Multiboot header (magic number `0x1BADB002`)
- Transfers control to `_start` symbol in kernel

---

### Step 2: Assembly Bootstrap (kernel/kernel.asm)

```asm
bits 32                     ; Tell NASM we're in 32-bit protected mode
```

#### Multiboot Header
```asm
MBALIGN     equ  1 << 0     ; Align modules on page boundaries
MEMINFO     equ  1 << 1     ; Provide memory map
FLAGS       equ  MBALIGN | MEMINFO
MAGIC       equ  0x1BADB002 ; Multiboot magic number
CHECKSUM    equ  -(MAGIC + FLAGS)  ; Checksum to verify header
```

**Purpose:** This header tells GRUB: "I'm a bootable kernel following the Multiboot specification."

#### Stack Setup
```asm
section .bss
align 16
stack_bottom:
    resb 16384              ; Reserve 16KB for stack
stack_top:
```

**Why?** The CPU needs a stack for function calls, local variables, and interrupt handling. We allocate 16KB of memory for this purpose.

#### Entry Point
```asm
section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top      ; Set stack pointer to top of our stack
    call kernel_main        ; Jump to C code
    
    cli                     ; Disable interrupts
.hang:
    hlt                     ; Halt CPU
    jmp .hang               ; Loop forever if kernel returns
```

**Explanation:**
1. `esp` (Stack Pointer) is set to our allocated stack
2. Call the C function `kernel_main()`
3. If kernel returns (shouldn't happen), halt the CPU

---

### Step 3: C Kernel Entry (kernel/kernel.c - kernel_main)

```c
void kernel_main(void) {
    // 1. Initialize VGA text display
    terminal_initialize();
    
    // 2. Set up CPU protection (GDT - Global Descriptor Table)
    init_gdt();
    
    // 3. Set up interrupt handling (IDT - Interrupt Descriptor Table)
    init_idt();
    
    // 4. Initialize logging system
    logger_init();
    klog("Kernel booting...");
    
    // 5. Start timer (50Hz = 20ms per tick)
    init_timer(50);
    
    // 6. Enable hardware interrupts
    asm volatile("sti");
    klog("Interrupts Enabled.");
    
    // 7. Initialize storage systems
    init_ramdisk();    // Volatile in-memory storage
    ata_init();        // Physical hard disk driver
    fs_init();         // Persistent filesystem
    
    // 8. Load user database
    init_users();
    init_processes();
    load_users_from_disk();
    
    // 9. Record boot time
    boot_time = get_time_seconds();
    
    // 10. Start login loop...
}
```

---

## CPU & Memory Management

### Global Descriptor Table (GDT)

**File: cpu/gdt.c**

#### What is the GDT?
The GDT defines memory segments. In protected mode, the CPU requires segment descriptors to access memory.

#### Data Structure (gdt.h)
```c
struct gdt_entry_struct {
    uint16_t limit_low;     // Lower 16 bits of segment limit
    uint16_t base_low;      // Lower 16 bits of base address
    uint8_t  base_middle;   // Next 8 bits of base
    uint8_t  access;        // Access flags (privilege level, type)
    uint8_t  granularity;   // Granularity + upper limit bits
    uint8_t  base_high;     // Highest 8 bits of base
} __attribute__((packed));  // No padding - exact memory layout
```

**Why `__attribute__((packed))`?** 
The compiler might add padding bytes for alignment. We need exact byte-by-byte layout to match CPU expectations.

#### Setting Up GDT Entries
```c
void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, 
                  uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;
    
    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;
    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access      = access;
}
```

**Breakdown:**
- `base`: Starting address of the segment
- `limit`: Size of the segment
- `access`: Permission bits (read/write/execute, privilege level)
- `gran`: Granularity (page-level or byte-level)

#### Five Required Segments
```c
void init_gdt() {
    // 0: Null Segment (Required by CPU, never used)
    gdt_set_gate(0, 0, 0, 0, 0);
    
    // 1: Kernel Code Segment (0x08)
    //    Base=0, Limit=4GB, Access=0x9A (code, ring 0)
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    
    // 2: Kernel Data Segment (0x10)
    //    Base=0, Limit=4GB, Access=0x92 (data, ring 0)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    
    // 3: User Code Segment (0x18)
    //    Access=0xFA (code, ring 3 - user mode)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    
    // 4: User Data Segment (0x20)
    //    Access=0xF2 (data, ring 3)
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);
    
    // Load GDT into CPU
    gdt_flush((uint32_t)&gdt_ptr);
}
```

**Access Byte Breakdown (0x9A for kernel code):**
```
Binary:  1001 1010
         │││└ ││││
         │││  ││└└ Type: Code, Readable
         │││  └└── Descriptor Type: Code/Data segment
         ││└────── Privilege: Ring 0 (kernel)
         │└─────── Present: Yes
         └──────── (unused)
```

#### Loading GDT (cpu/gdt.asm)
```asm
global gdt_flush

gdt_flush:
    mov eax, [esp+4]      ; Get pointer to GDT descriptor
    lgdt [eax]            ; Load GDT (CPU instruction)
    
    mov ax, 0x10          ; 0x10 = offset to data segment (entry 2)
    mov ds, ax            ; Load data segment selectors
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    jmp 0x08:.flush       ; Far jump to reload code segment
.flush:
    ret
```

**Why the far jump?**
After loading GDT, the CPU's code segment register must be updated. A far jump forces this update.

---

### Interrupt Descriptor Table (IDT)

**File: cpu/idt.c**

#### What is the IDT?
The IDT tells the CPU what function to call when an interrupt occurs (hardware event or software exception).

#### Data Structure
```c
struct idt_entry_struct {
    uint16_t base_low;    // Lower 16 bits of handler address
    uint16_t sel;         // Code segment selector (0x08)
    uint8_t  always0;     // Always zero
    uint8_t  flags;       // Type and attributes
    uint16_t base_high;   // Upper 16 bits of handler address
} __attribute__((packed));
```

#### Setting an IDT Entry
```c
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt_entries[num].base_low  = base & 0xFFFF;
    idt_entries[num].base_high = (base >> 16) & 0xFFFF;
    idt_entries[num].sel       = sel;           // 0x08 = kernel code
    idt_entries[num].always0   = 0;
    idt_entries[num].flags     = flags | 0x60;  // User-accessible
}
```

#### PIC Remapping - Critical!
```c
void pic_remap() {
    // Save masks
    uint8_t a1 = inb(0x21);
    uint8_t a2 = inb(0xA1);
    
    // Initialize PICs
    outb(0x20, 0x11);    // ICW1: Start initialization
    outb(0xA0, 0x11);
    
    // Remap IRQs
    outb(0x21, 0x20);    // Master PIC: IRQ 0-7 → interrupts 32-39
    outb(0xA1, 0x28);    // Slave PIC: IRQ 8-15 → interrupts 40-47
    
    // Setup cascade
    outb(0x21, 0x04);    // Tell master: slave at IRQ2
    outb(0xA1, 0x02);    // Tell slave: cascade identity
    
    // 8086 mode
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    
    // Restore masks
    outb(0x21, a1);
    outb(0xA1, a2);
}
```

**Why remap?**
By default, hardware interrupts (IRQs) overlap with CPU exceptions (0-31). We move IRQs to 32-47 to avoid conflicts.

---

### Interrupt Service Routines (ISR)

**File: cpu/interrupt.asm**

#### Generic ISR Stub
```asm
isr32:                    ; Timer interrupt (IRQ0)
    cli                   ; Disable interrupts
    push byte 0           ; Push dummy error code
    push byte 32          ; Push interrupt number
    jmp irq_common_stub
```

#### Common Handler
```asm
irq_common_stub:
    pusha                 ; Save all registers (eax, ebx, ecx...)
    
    mov ax, ds            ; Save data segment
    push eax
    
    mov ax, 0x10          ; Load kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    call irq_handler      ; Call C handler
    
    pop eax               ; Restore original data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    popa                  ; Restore all registers
    add esp, 8            ; Clean up error code and int number
    sti                   ; Re-enable interrupts
    iret                  ; Return from interrupt
```

**Step-by-step:**
1. `pusha` - Save CPU state
2. Switch to kernel data segment
3. Call C handler function
4. Restore CPU state
5. `iret` - Return to interrupted code

#### C-Side Handler (cpu/isr.c)
```c
void irq_handler(registers_t r) {
    // Send End-Of-Interrupt to PIC
    if (r.int_no >= 40) 
        outb(0xA0, 0x20);  // Slave PIC
    outb(0x20, 0x20);      // Master PIC
    
    // Call registered handler
    if (interrupt_handlers[r.int_no] != 0) {
        isr_t handler = interrupt_handlers[r.int_no];
        handler(r);
    }
}
```

**Why EOI (End-Of-Interrupt)?**
The PIC won't send more interrupts until we acknowledge the current one.

---

### Timer Driver

**File: cpu/timer.c**

#### Programmable Interval Timer (PIT)
```c
void init_timer(uint32_t freq) {
    // Register our callback for IRQ0 (interrupt 32)
    register_interrupt_handler(IRQ0, timer_callback);
    
    // Calculate divisor (PIT runs at 1.193180 MHz)
    uint32_t divisor = 1193180 / freq;
    
    uint8_t low  = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);
    
    // Send command byte (0x36 = channel 0, square wave)
    outb(0x43, 0x36);
    
    // Send frequency divisor
    outb(0x40, low);
    outb(0x40, high);
}
```

**What does 50Hz mean?**
The timer fires an interrupt 50 times per second (every 20 milliseconds).

#### Timer Callback
```c
volatile uint32_t tick = 0;

static void timer_callback(registers_t regs) {
    tick++;  // Increment global tick counter
}
```

**Usage in kernel:**
```c
// Update clock every 50 ticks (1 second at 50Hz)
if (tick - last_sec_tick > 50) {
    // Refresh display
}
```

---

## Hardware Drivers

### VGA Text Mode Display

**File: drivers/display.c**

#### VGA Memory Layout
```c
volatile uint16_t* vga_buffer = (uint16_t*) 0xB8000;
```

**Physical memory `0xB8000` is mapped to screen:**
- 80 columns × 25 rows = 2000 characters
- Each character = 16 bits (2 bytes):
  - Lower 8 bits: ASCII character
  - Upper 8 bits: Color attribute

#### Color System
```c
uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}
```

**Example:**
```c
// Green text on black background
uint8_t color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
// Binary: 0000 (black bg) 1010 (green fg) = 0x0A
```

#### Writing to Screen
```c
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
    const size_t index = y * VGA_WIDTH + x;  // 2D to 1D conversion
    vga_buffer[index] = vga_entry(c, color);
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}
```

**Memory layout for "A" in green:**
```
vga_buffer[0] = 0x0A41
                  │└─── Character: 0x41 ('A')
                  └──── Color: 0x0A (green on black)
```

#### Cursor Control
```c
void update_cursor(int x, int y) {
    uint16_t pos = y * VGA_WIDTH + x;
    
    // Tell VGA controller to update cursor
    outb(0x3D4, 0x0F);           // Select low byte
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    
    outb(0x3D4, 0x0E);           // Select high byte
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}
```

#### Screen Scrolling (Simplified)
```c
void terminal_putc(char c) {
    if (c == '\n') {
        terminal_col = 0;
        terminal_row++;
    } else {
        terminal_putentryat(c, terminal_color, terminal_col, terminal_row);
        terminal_col++;
    }
    
    if (terminal_col >= VGA_WIDTH) {
        terminal_col = 0;
        terminal_row++;
    }
    
    // Pseudo-scrolling: clear lower area when full
    if (terminal_row >= VGA_HEIGHT) {
        terminal_clear_area(5);  // Keep top 5 rows
    }
    
    update_cursor(terminal_col, terminal_row);
}
```

---

### Keyboard Driver

**Integrated in kernel.c**

#### Scancode to ASCII Conversion
```c
char scancode_to_ascii(uint8_t scancode) {
    const char lower[] = {
        0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
        '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
        0,'a','s','d','f','g','h','j','k','l',';','\'','`',
        0,'\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
    };
    
    const char upper[] = {
        0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
        '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
        0,'A','S','D','F','G','H','J','K','L',':','"','~',
        0,'|','Z','X','C','V','B','N','M','<','>','?',0,'*',0,'~'
    };
    
    if (scancode > 128) return 0;  // Key release
    return shift_pressed ? upper[scancode] : lower[scancode];
}
```

**How it works:**
- Scancode 0x1E = 'a' key
- If shift is pressed → return 'A'
- Else → return 'a'

#### Keyboard Interrupt Handler (IRQ1)
```c
// In main loop:
if (inb(0x64) & 1) {            // Check if data available
    uint8_t scancode = inb(0x60); // Read scancode from port 0x60
    
    // Handle modifier keys
    if (scancode == 0x2A || scancode == 0x36) 
        shift_pressed = 1;      // Left/Right Shift pressed
    else if (scancode == 0xAA || scancode == 0xB6) 
        shift_pressed = 0;      // Shift released
    else if (scancode == 0x1D) 
        ctrl_pressed = 1;       // Ctrl pressed
    else if (scancode == 0x9D) 
        ctrl_pressed = 0;       // Ctrl released
    
    // Convert to character
    char c = scancode_to_ascii(scancode);
    
    if (c == '\n') {
        // Process command
    } else if (c == '\b') {
        // Handle backspace
    } else {
        append(key_buffer, c);
        terminal_putc(c);
    }
}
```

---

### ATA Hard Disk Driver

**File: drivers/ata.c**

#### ATA I/O Ports
```c
#define ATA_PRIMARY_DATA       0x1F0  // Read/Write data
#define ATA_PRIMARY_SECCOUNT   0x1F2  // Sector count
#define ATA_PRIMARY_LBA_LO     0x1F3  // LBA bits 0-7
#define ATA_PRIMARY_LBA_MID    0x1F4  // LBA bits 8-15
#define ATA_PRIMARY_LBA_HI     0x1F5  // LBA bits 16-23
#define ATA_PRIMARY_DRIVE      0x1F6  // Drive select + LBA bits 24-27
#define ATA_PRIMARY_STATUS     0x1F7  // Status register
#define ATA_PRIMARY_COMMAND    0x1F7  // Command register
```

#### Reading a Sector (512 bytes)
```c
int ata_read_sector(uint32_t lba, uint8_t* buffer) {
    // 1. Wait until drive is ready
    ata_wait_ready();
    
    // 2. Select master drive with LBA mode
    outb(ATA_PRIMARY_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    
    // 3. Send sector count (1 sector)
    outb(ATA_PRIMARY_SECCOUNT, 1);
    
    // 4. Send LBA address (28-bit addressing)
    outb(ATA_PRIMARY_LBA_LO,  (uint8_t)lba);
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI,  (uint8_t)(lba >> 16));
    
    // 5. Send READ command
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_SECTORS);
    
    // 6. Wait for data to be ready
    ata_wait_drq();
    
    // 7. Read 256 words (512 bytes)
    for(int i = 0; i < 256; i++) {
        uint16_t data = inw(ATA_PRIMARY_DATA);  // Read 16 bits
        buffer[i * 2]     = (uint8_t)data;
        buffer[i * 2 + 1] = (uint8_t)(data >> 8);
    }
    
    return 0;
}
```

**LBA (Logical Block Addressing):**
Instead of cylinder/head/sector, we use a simple linear address:
- LBA 0 = First sector on disk
- LBA 1 = Second sector
- etc.

#### Writing a Sector
```c
int ata_write_sector(uint32_t lba, uint8_t* buffer) {
    ata_wait_ready();
    
    // Setup (same as read)
    outb(ATA_PRIMARY_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PRIMARY_SECCOUNT, 1);
    outb(ATA_PRIMARY_LBA_LO,  (uint8_t)lba);
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HI,  (uint8_t)(lba >> 16));
    
    // Send WRITE command
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE_SECTORS);
    
    ata_wait_drq();
    
    // Write 256 words
    for(int i = 0; i < 256; i++) {
        uint16_t data = buffer[i * 2] | (buffer[i * 2 + 1] << 8);
        outw(ATA_PRIMARY_DATA, data);
    }
    
    ata_wait_ready();  // Wait for write to complete
    return 0;
}
```

---

## Filesystem (323FS)

**File: drivers/filesystem.c**

### Architecture

```
Disk Layout:
┌──────────────────────────────────────┐
│ Sector 0: File Table (16 entries)   │ ← Metadata
├──────────────────────────────────────┤
│ Sectors 1-4: File 0 data (2KB max)  │
├──────────────────────────────────────┤
│ Sectors 5-8: File 1 data (2KB max)  │
├──────────────────────────────────────┤
│ ...                                  │
└──────────────────────────────────────┘
```

### File Entry Structure
```c
typedef struct {
    char name[28];              // Filename
    char path[32];              // Directory path
    uint32_t start_sector;      // First sector of data
    uint32_t size;              // File size in bytes
    uint8_t used;               // 1 if entry is active
    uint8_t is_directory;       // 1 if directory
    char owner[16];             // Username
    uint8_t permissions;        // RWX bits
} fs_entry_t;
```

### Initialization
```c
void fs_init(void) {
    // Read file table from disk sector 0
    ata_read_sector(FS_FILE_TABLE_SECTOR, sector_buffer);
    
    // Copy to memory
    for(int i = 0; i < FS_MAX_FILES; i++) {
        fs_entry_t* entry = (fs_entry_t*)(sector_buffer + (i * sizeof(fs_entry_t)));
        file_table[i] = *entry;
    }
    
    klog("Filesystem initialized.");
}
```

### Writing a File
```c
int fs_write_file(const char* name, const char* data, uint32_t size) {
    // 1. Find existing file or empty slot
    int slot = -1;
    for(int i = 0; i < FS_MAX_FILES; i++) {
        if(file_table[i].used && strcmp(file_table[i].name, name) == 0) {
            slot = i;  // Overwrite existing
            break;
        }
        if(!file_table[i].used && slot == -1) {
            slot = i;  // First free slot
        }
    }
    
    if(slot == -1) return -1;  // Disk full
    
    // 2. Calculate sectors needed (up to 4 sectors = 2KB)
    uint32_t sectors_needed = (size + FS_SECTOR_SIZE - 1) / FS_SECTOR_SIZE;
    if (sectors_needed > FS_MAX_SECTORS_PER_FILE) 
        sectors_needed = FS_MAX_SECTORS_PER_FILE;
    
    // 3. Allocate sectors (fixed allocation)
    uint32_t start_sector = FS_DATA_START_SECTOR + (slot * FS_MAX_SECTORS_PER_FILE);
    
    // 4. Update file table
    file_table[slot].used = 1;
    strcpy(file_table[slot].name, name);
    strcpy(file_table[slot].path, current_directory);
    file_table[slot].start_sector = start_sector;
    file_table[slot].size = size;
    file_table[slot].is_directory = 0;
    
    // 5. Write data to disk (multi-sector)
    uint32_t remaining = size;
    uint32_t offset = 0;
    
    for (uint32_t s = 0; s < sectors_needed; s++) {
        // Clear buffer
        for(int i = 0; i < FS_SECTOR_SIZE; i++) 
            sector_buffer[i] = 0;
        
        // Copy data chunk
        uint32_t to_write = (remaining > FS_SECTOR_SIZE) ? FS_SECTOR_SIZE : remaining;
        for(uint32_t i = 0; i < to_write; i++) {
            sector_buffer[i] = data[offset + i];
        }
        
        // Write sector
        ata_write_sector(start_sector + s, sector_buffer);
        
        offset += to_write;
        remaining -= to_write;
    }
    
    // 6. Save file table to disk
    fs_save_table();
    
    return 0;
}
```

**Key Points:**
- **Fixed allocation:** Each file gets 4 sectors (2KB) reserved
- **Fragmentation:** Not an issue due to fixed allocation
- **Metadata persistence:** File table saved after every change

### Reading a File
```c
int fs_read_file(const char* name, char* buffer, uint32_t* size) {
    // Find file in table
    for(int i = 0; i < FS_MAX_FILES; i++) {
        if(file_table[i].used && strcmp(file_table[i].name, name) == 0) {
            *size = file_table[i].size;
            
            // Calculate sectors to read
            uint32_t sectors_needed = (*size + FS_SECTOR_SIZE - 1) / FS_SECTOR_SIZE;
            uint32_t offset = 0;
            
            // Read each sector
            for (uint32_t s = 0; s < sectors_needed && s < FS_MAX_SECTORS_PER_FILE; s++) {
                ata_read_sector(file_table[i].start_sector + s, sector_buffer);
                
                uint32_t to_read = (*size - offset > FS_SECTOR_SIZE) ? 
                                   FS_SECTOR_SIZE : (*size - offset);
                
                for(uint32_t j = 0; j < to_read; j++) {
                    buffer[offset + j] = sector_buffer[j];
                }
                offset += to_read;
            }
            buffer[*size] = 0;  // Null-terminate
            
            return 0;  // Success
        }
    }
    
    return -1;  // File not found
}
```

### Directory Support
```c
int fs_mkdir(const char* name) {
    // Find empty slot
    for(int i = 0; i < FS_MAX_FILES; i++) {
        if(!file_table[i].used) {
            file_table[i].used = 1;
            strcpy(file_table[i].name, name);
            strcpy(file_table[i].path, current_directory);
            file_table[i].is_directory = 1;
            file_table[i].size = 0;
            file_table[i].start_sector = 0;  // Directories don't use data
            fs_save_table();
            return 0;
        }
    }
    return -1;  // No space
}
```

### Changing Directory
```c
int fs_chdir(const char* path) {
    if (strcmp(path, "/") == 0) {
        strcpy(current_directory, "/");
        return 0;
    }
    
    if (strcmp(path, "..") == 0) {
        // Go up one directory
        int len = strlen(current_directory);
        if (len > 1) {
            for (int i = len - 2; i >= 0; i--) {
                if (current_directory[i] == '/') {
                    current_directory[i + 1] = 0;
                    return 0;
                }
            }
        }
        return 0;
    }
    
    // Check if directory exists
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (file_table[i].used && file_table[i].is_directory &&
            strcmp(file_table[i].name, path) == 0) {
            
            if (current_directory[strlen(current_directory) - 1] != '/') {
                strcat(current_directory, "/");
            }
            strcat(current_directory, path);
            return 0;
        }
    }
    return -1;  // Not found
}
```

---

## User Interface

### Login System

```c
while(!logged_in) {
    terminal_writestring("\n   Username: ");
    
    // Read username
    buf_idx = 0;
    username_buf[0] = 0;
    
    while(1) {
        update_live_clock();
        
        if (inb(0x64) & 1) {
            uint8_t scancode = inb(0x60);
            
            // Handle shift key
            if(scancode == 0x2A || scancode == 0x36) 
                shift_pressed = 1;
            else if(scancode == 0xAA || scancode == 0xB6) 
                shift_pressed = 0;
            else if (scancode < 0x80) {
                char c = scancode_to_ascii(scancode);
                
                if (c == '\n') {
                    terminal_putc('\n');
                    username_buf[buf_idx] = 0;
                    break;
                } else if (c == '\b') {
                    if (buf_idx > 0) {
                        buf_idx--;
                        username_buf[buf_idx] = 0;
                        terminal_putc('\b');
                    }
                } else if (c != 0 && buf_idx < 31) {
                    username_buf[buf_idx++] = c;
                    terminal_putc(c);
                }
            }
        }
    }
    
    // Similar process for password, but show '*' instead of actual characters
    terminal_writestring("   Password: ");
    buf_idx = 0;
    password_buf[0] = 0;
    
    while(1) {
        // ... (same as username) ...
        if (c != 0 && buf_idx < 31) {
            password_buf[buf_idx++] = c;
            terminal_putc('*');  // Show asterisk for security
        }
    }
    
    // Verify credentials
    if(verify_login(username_buf, password_buf)) {
        logged_in = 1;
        strcpy(current_user, username_buf);
        klog("User logged in.");
    } else {
        terminal_writestring("   Access Denied.\n");
    }
}
```

### User Database (users.db)

**File format:**
```
admin:1234
bob:secret
alice:pass123
```

#### Loading Users from Disk
```c
void load_users_from_disk() {
    char buffer[512];
    uint32_t size;
    
    if(fs_read_file("users.db", buffer, &size) == 0) {
        // Parse format: "username:password\n"
        char* ptr = buffer;
        int user_idx = 0;
        
        while(*ptr && user_idx < 10) {
            // Read username until ':'
            int name_idx = 0;
            while(*ptr && *ptr != ':' && name_idx < 31) {
                users[user_idx].username[name_idx++] = *ptr++;
            }
            users[user_idx].username[name_idx] = 0;
            
            if(*ptr == ':') ptr++;
            
            // Read password until '\n'
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
    
    // No users.db found - create default admin
    users[0].active = 1;
    strcpy(users[0].username, "admin");
    strcpy(users[0].password, "1234");
    save_users_to_disk();
}
```

### Dashboard

```c
void draw_dashboard() {
    terminal_clear();
    
    // Top bar (blue background)
    for(int i=0; i<80; i++) 
        terminal_putentryat(' ', vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE), i, 0);
    
    // Header text
    char header[80] = "  323OS v3.6  |  User: "
    strcat(header, current_user);
    
    for(int i=0; header[i]; i++) 
        terminal_putentryat(header[i], vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE), i, 0);
    
    // Command area
    terminal_set_color(vga_entry_color(current_theme_fg, current_theme_bg));
    terminal_writestring("\n");
    terminal_writestring("-----------------------------------\n");
    terminal_writestring(" COMMANDS: ls edit cat rm ...\n");
    terminal_writestring("-----------------------------------\n");
}
```

### Live Clock Display
```c
void update_live_clock() {
    static int last_spin_tick = 0;
    
    // Spinning indicator every 200ms (10 ticks at 50Hz)
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
    
    // Update time every second (50 ticks)
    if (tick - last_sec_tick > 50) {
        last_sec_tick = tick;
        
        char time_str[16], date_str[16];
        get_bdt_string(time_str);    // HH:MM:SS AM/PM
        get_bdt_date(date_str);      // DD/MM/YYYY
        
        uint8_t color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
        int start_x = 34;
        
        // Write date
        for(int i=0; date_str[i] && i < 10; i++) 
            terminal_putentryat(date_str[i], color, start_x + i, 0);
        
        terminal_putentryat(' ', color, start_x + 10, 0);
        
        // Write time
        for(int i=0; time_str[i] && i < 11; i++) 
            terminal_putentryat(time_str[i], color, start_x + 11 + i, 0);
        
        // Label
        char* label = " Dhaka";
        for(int i=0; label[i]; i++) 
            terminal_putentryat(label[i], color, start_x + 22 + i, 0);
    }
}
```

### Real-Time Clock (RTC)

```c
#define BCD_TO_BIN(val) ((val & 0x0F) + ((val >> 4) * 10))

uint8_t get_rtc_register(int reg) {
    outb(0x70, reg);  // Select register
    return inb(0x71); // Read value
}

void get_bdt_string(char* buf) {
    // Read from RTC
    uint8_t s = BCD_TO_BIN(get_rtc_register(0x00));  // Seconds
    uint8_t m = BCD_TO_BIN(get_rtc_register(0x02));  // Minutes
    uint8_t h = BCD_TO_BIN(get_rtc_register(0x04));  // Hours
    
    // Convert to Bangladesh Time (UTC+6)
    h = (h + 6) % 24;
    
    // Convert to 12-hour format
    char* period = "AM";
    if (h >= 12) {
        period = "PM";
        if (h > 12) h -= 12;
    }
    if (h == 0) h = 12;
    
    // Format string
    buf[0] = (h / 10) + '0';
    buf[1] = (h % 10) + '0';
    buf[2] = ':';
    buf[3] = (m / 10) + '0';
    buf[4] = (m % 10) + '0';
    buf[5] = ':';
    buf[6] = (s / 10) + '0';
    buf[7] = (s % 10) + '0';
    buf[8] = ' ';
    buf[9] = period[0];
    buf[10] = period[1];
    buf[11] = 0;
}
```

**BCD (Binary-Coded Decimal):**
RTC stores values in BCD format where each nibble represents a decimal digit.
Example: 0x23 = 23 (not 35!)

---

### Text Editor

```c
void start_editor_storage(char* filename) {
    terminal_clear_area(0);
    terminal_writestring(" 323OS Editor -- Press 'Ctrl+S' to Save & Exit \n");
    terminal_writestring("File: ");
    terminal_writestring(filename);
    terminal_writestring("\n\n");
    
    char buffer[2048];
    int idx = 0;
    buffer[0] = 0;
    
    // Pre-load existing content
    uint32_t size;
    if(fs_read_file(filename, buffer, &size) == 0) {
        idx = size;
        terminal_writestring(buffer);  // Display existing content
    }
    
    // Edit loop
    while(1) {
        update_live_clock();
        
        if (inb(0x64) & 1) {
            uint8_t scancode = inb(0x60);
            
            // Handle modifier keys
            if(scancode == 0x1D) ctrl_pressed = 1;
            else if(scancode == 0x9D) ctrl_pressed = 0;
            else if(scancode == 0x2A || scancode == 0x36) shift_pressed = 1;
            else if(scancode == 0xAA || scancode == 0xB6) shift_pressed = 0;
            
            else if(scancode < 0x80) {
                // Ctrl+S to save
                if (ctrl_pressed && scancode == 0x1F) {  // S key
                    buffer[idx] = 0;
                    storage_write(filename, buffer);
                    terminal_clear_area(0);
                    return;
                }
                
                char c = scancode_to_ascii(scancode);
                
                if (c == '\b') {
                    if (idx > 0) {
                        idx--;
                        backspace(buffer);
                        terminal_putc('\b');
                    }
                } else if (c == '\n') {
                    if (idx < 2046) {
                        buffer[idx++] = '\n';
                        terminal_putc('\n');
                    }
                } else if (c != 0) {
                    if (idx < 2046) {
                        buffer[idx++] = c;
                        terminal_putc(c);
                    }
                }
            }
        }
    }
}
```

**Key Features:**
- Loads existing file content
- Real-time character display
- Ctrl+S saves and exits
- 2KB file size limit

---

### Command Processor

```c
int execute_command(char* input) {
    add_history(input);  // Save to history
    
    if (strcmp(input, "help") == 0) {
        terminal_writestring("System: top, free, df, ps, whoami...\n");
        terminal_writestring("Files: ls, mkdir, cd, edit, cat, rm...\n");
        // ...
    }
    else if (strcmp(input, "logout") == 0) {
        return 1;  // Signal to exit session
    }
    else if (strcmp(input, "clear") == 0) {
        terminal_clear_area(5);  // Keep top bar
    }
    else if (strcmp(input, "ls") == 0) {
        fs_list_dir(fs_get_cwd());
    }
    else if (input[0]=='c' && input[1]=='a' && input[2]=='t') {
        char* ptr = skip_spaces(input + 3);  // Skip "cat"
        if (*ptr) storage_read(ptr);
        else terminal_writestring("Usage: cat [filename]\n");
    }
    // ... (many more commands)
    
    return 0;  // Continue session
}
```

**Command Parsing Example (calc):**
```c
else if (input[0]=='c' && input[1]=='a' && input[2]=='l' && input[3]=='c') {
    char* ptr = skip_spaces(input+4);
    char n1[16]={0}, n2[16]={0};
    int i=0;
    
    // Parse first number
    while(*ptr>='0' && *ptr<='9') n1[i++] = *ptr++;
    
    ptr = skip_spaces(ptr);
    char op = *ptr++;  // Get operator (+, -, *, /, %)
    ptr = skip_spaces(ptr);
    
    // Parse second number
    i=0;
    while(*ptr>='0' && *ptr<='9') n2[i++] = *ptr++;
    
    int a = atoi(n1), b = atoi(n2), res=0;
    
    if(op=='+') res=a+b;
    else if(op=='-') res=a-b;
    else if(op=='*') res=a*b;
    else if(op=='/') {
        if(b==0) terminal_writestring("Div 0\n");
        else res=a/b;
    }
    else if(op=='%') {
        if(b==0) terminal_writestring("Div 0\n");
        else res=a%b;
    }
    
    if(!(op=='/' && b==0)) {
        char buf[32];
        itoa(res, buf);
        terminal_writestring("Result: ");
        terminal_writestring(buf);
        terminal_writestring("\n");
    }
}
```

---

## Complete Code Walkthrough

### Boot Sequence (Detailed)

1. **Power On** → BIOS POST
2. **BIOS** → Loads GRUB from boot sector
3. **GRUB** → Reads `grub.cfg`, loads `kernel.bin` to 0x100000
4. **Multiboot Check** → GRUB verifies magic number `0x1BADB002`
5. **Jump to _start** → Control transfers to `kernel/kernel.asm`
6. **Stack Setup** → ESP points to 16KB stack
7. **Call kernel_main** → Enter C code

### kernel_main() Execution Flow

```
kernel_main()
│
├─ terminal_initialize()
│  └─ Clear screen, set color
│
├─ init_gdt()
│  ├─ Setup 5 segment descriptors
│  └─ Load GDT via assembly
│
├─ init_idt()
│  ├─ Remap PIC (IRQs 32-47)
│  ├─ Set interrupt gates
│  └─ Load IDT via assembly
│
├─ logger_init()
│  └─ Initialize ring buffer
│
├─ init_timer(50)
│  ├─ Register IRQ0 handler
│  └─ Program PIT for 50Hz
│
├─ asm("sti")
│  └─ Enable interrupts
│
├─ init_ramdisk()
│  └─ Clear 10 file slots
│
├─ ata_init()
│  └─ Select master drive
│
├─ fs_init()
│  └─ Read file table from disk
│
├─ init_users()
│  └─ Clear user array
│
├─ load_users_from_disk()
│  ├─ Read users.db
│  └─ Parse username:password
│
├─ boot_time = get_time_seconds()
│  └─ Record boot timestamp
│
└─ LOGIN LOOP
   │
   ├─ Get username/password
   ├─ verify_login()
   ├─ draw_dashboard()
   │
   └─ COMMAND LOOP
      │
      ├─ update_live_clock()
      │  ├─ Spinner animation
      │  └─ Time/date display
      │
      ├─ Check keyboard (port 0x60)
      ├─ Build command string
      │
      ├─ On ENTER:
      │  ├─ execute_command()
      │  └─ Display prompt
      │
      └─ Loop until logout
```

### Interrupt Flow (Timer Example)

```
Every 20ms (50Hz):
│
1. PIT fires IRQ0
   │
2. CPU receives interrupt 32
   │
3. CPU pushes state to stack
   │
4. CPU looks up interrupt 32 in IDT
   │
5. Jump to isr32 (interrupt.asm)
   │
   ├─ cli (disable interrupts)
   ├─ Push error code (0)
   ├─ Push int number (32)
   └─ Jump to irq_common_stub
      │
      ├─ pusha (save registers)
      ├─ Switch to kernel data segment
      ├─ Call irq_handler() in C
      │  │
      │  ├─ Send EOI to PIC
      │  └─ Call timer_callback()
      │     └─ tick++
      │
      ├─ Restore data segment
      ├─ popa (restore registers)
      ├─ sti (re-enable interrupts)
      └─ iret (return from interrupt)
         │
         └─ Resume interrupted code
```

### File Write Flow

```
User types: edit test.txt
│
1. start_editor_storage("test.txt")
   ├─ Try to load existing content
   ├─ Display editor UI
   └─ Enter edit loop
      │
      └─ User presses Ctrl+S
         │
2. storage_write("test.txt", buffer)
   │
3. fs_write_file("test.txt", buffer, size)
   │
   ├─ Find slot in file_table
   ├─ Calculate sectors needed
   │  └─ sectors = (size + 511) / 512
   │
   ├─ Allocate sectors
   │  └─ start = 1 + (slot * 4)
   │
   ├─ Update file_table[slot]
   │  ├─ name = "test.txt"
   │  ├─ size = buffer length
   │  └─ start_sector = calculated
   │
   ├─ Write data sectors
   │  │
   │  └─ For each sector:
   │     ├─ Copy 512 bytes to sector_buffer
   │     └─ ata_write_sector(LBA, buffer)
   │        │
   │        ├─ outb(0x1F6, drive+LBA[27:24])
   │        ├─ outb(0x1F3-5, LBA[23:0])
   │        ├─ outb(0x1F7, WRITE_CMD)
   │        └─ outw(0x1F0, data) × 256
   │
   └─ fs_save_table()
      └─ ata_write_sector(0, file_table)
```

---

## Build System

### Build Script (build.sh)

```bash
#!/bin/bash
set -e  # Exit on any error

# 1. ASSEMBLY PHASE
nasm -f elf32 kernel/kernel.asm -o kernel/kernel_asm.o
nasm -f elf32 cpu/gdt.asm -o cpu/gdt_asm.o
nasm -f elf32 cpu/interrupt.asm -o cpu/interrupt_asm.o

# 2. C COMPILATION PHASE
gcc -m32 -ffreestanding -c kernel/kernel.c -o kernel/kernel_c.o
gcc -m32 -ffreestanding -c kernel/utils.c -o kernel/utils.o
gcc -m32 -ffreestanding -c drivers/display.c -o drivers/display.o
gcc -m32 -ffreestanding -c drivers/ata.c -o drivers/ata.o
gcc -m32 -ffreestanding -c drivers/filesystem.c -o drivers/filesystem.o
gcc -m32 -ffreestanding -c logs/logger.c -o logs/logger.o
gcc -m32 -ffreestanding -c cpu/timer.c -o cpu/timer.o
gcc -m32 -ffreestanding -c cpu/gdt.c -o cpu/gdt_c.o
gcc -m32 -ffreestanding -c cpu/idt.c -o cpu/idt_c.o
gcc -m32 -ffreestanding -c cpu/isr.c -o cpu/isr_c.o

# 3. LINKING PHASE
ld -m elf_i386 -T linker.ld -o kernel/kernel.bin \
    kernel/kernel_asm.o \
    kernel/kernel_c.o \
    kernel/utils.o \
    drivers/display.o \
    drivers/ata.o \
    drivers/filesystem.o \
    logs/logger.o \
    cpu/timer.o \
    cpu/gdt_asm.o \
    cpu/gdt_c.o \
    cpu/idt_c.o \
    cpu/isr_c.o \
    cpu/interrupt_asm.o

# 4. VERIFICATION
grub-file --is-x86-multiboot kernel/kernel.bin

# 5. CREATE BOOTABLE ISO
mkdir -p iso/boot/grub
cp kernel/kernel.bin iso/boot/
grub-mkrescue -o 323OS.iso iso/
```

**Flags explained:**
- `-m32`: Compile for 32-bit (i386)
- `-ffreestanding`: No standard library (freestanding environment)
- `-T linker.ld`: Use custom linker script
- `-m elf_i386`: Output 32-bit ELF format

### Linker Script (linker.ld)

```ld
ENTRY(_start)           /* Entry point symbol */

SECTIONS
{
    . = 1M;             /* Load kernel at 1MB (0x100000) */

    .text : {           /* Code section */
        *(.multiboot)   /* Multiboot header MUST be first */
        *(.text)        /* Code from all .o files */
    }

    .rodata : {         /* Read-only data (strings, constants) */
        *(.rodata)
    }

    .data : {           /* Initialized data */
        *(.data)
    }

    .bss : {            /* Uninitialized data (zeroed at load) */
        *(COMMON)
        *(.bss)
    }
}
```

**Why 1MB?**
- First 640KB: Conventional memory (BIOS usage)
- 640KB-1MB: Video memory, ROM
- 1MB+: Extended memory (safe for kernel)

---

## Utility Functions

### String Comparison
```c
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
```

**Returns:**
- 0: Strings are equal
- <0: s1 < s2 (lexicographically)
- >0: s1 > s2

### Integer to String
```c
void itoa(int n, char* str) {
    int i = 0;
    int sign = n;
    
    if (n < 0) n = -n;  // Handle negative
    
    // Extract digits in reverse
    do {
        str[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);
    
    if (sign < 0) str[i++] = '-';
    str[i] = '\0';
    
    reverse(str);  // Fix order
}
```

**Example:** `itoa(123, buf)` → `buf = "123"`

### String Copy
```c
void strcpy(char* dest, const char* src) {
    int i = 0;
    while (src[i] != 0) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = 0;  // Null-terminate
}
```

---

## Logger System

**File: logs/logger.c**

### Ring Buffer Implementation
```c
#define MAX_LOGS 20
#define LOG_LENGTH 64

char log_buffer[MAX_LOGS][LOG_LENGTH];
int log_head = 0;      // Next write position
int log_count = 0;     // Total logs written
```

### Adding a Log Entry
```c
void klog(char* message) {
    char temp[LOG_LENGTH];
    temp[0] = 0;
    
    // 1. Add timestamp
    get_timestamp(temp);  // "[HH:MM:SS AM/PM] "
    
    // 2. Append message
    int t_len = strlen(temp);
    int i = 0;
    while (message[i] && (t_len + i < LOG_LENGTH - 1)) {
        temp[t_len + i] = message[i];
        i++;
    }
    temp[t_len + i] = 0;
    
    // 3. Save to ring buffer
    strcpy(log_buffer[log_head], temp);
    
    // 4. Advance head (wrap around)
    log_head++;
    if (log_head >= MAX_LOGS) log_head = 0;
    log_count++;
}
```

### Displaying Logs
```c
void klog_dump() {
    terminal_writestring("--- SYSTEM LOGS ---\n");
    
    int start = 0;
    int count = log_count;
    
    // If wrapped, start from oldest
    if (log_count >= MAX_LOGS) {
        start = log_head;
        count = MAX_LOGS;
    } else {
        count = log_head;
    }
    
    // Print in chronological order
    for (int i = 0; i < count; i++) {
        int idx = (start + i) % MAX_LOGS;
        terminal_writestring(log_buffer[idx]);
        terminal_writestring("\n");
    }
}
```

---

## How to Demo

### Preparation
1. **Build the OS:**
   ```bash
    cd "323 OS"
   chmod +x build.sh
   ./build.sh
   ```

2. **Run in QEMU:**
   ```bash
   qemu-system-i386 -cdrom 323OS.iso -m 512M
   ```

### Demo Script

#### 1. Boot & Login
```
[System boots, shows initialization messages]
Username: admin
Password: 1234
[Dashboard appears with live clock]
```

#### 2. System Information
```
> sysinfo
OS: 323OS v3.6 | User: admin | 11/02/2026 03:45:23 PM BDT

> top
=== SYSTEM MONITOR ===
11/02/2026 03:45:30 PM BDT
Uptime: 0:01:15
CPU: i386 Compatible
Processes: 1 (kernel)
Total RAM: 512 MB
...
```

#### 3. File Operations
```
> edit hello.txt
[Editor opens]
Hello from 323OS!
This is a test file.
[Press Ctrl+S to save]

> ls
--- / ---
[FILE] hello.txt (44 bytes)

> cat hello.txt
--- hello.txt ---
Hello from 323OS!
This is a test file.
```

#### 4. Directory Management
```
> mkdir documents
Directory created.

> cd documents

> pwd
/documents/

> touch notes.txt

> ls
--- /documents/ ---
[FILE] notes.txt (0 bytes)

> cd ..

> tree
/
|-- [documents]
  |-- notes.txt
|-- hello.txt
```

#### 5. User Management
```
> users
--- REGISTERED USERS ---
- admin

> useradd bob secret123
User added successfully.

> users
--- REGISTERED USERS ---
- admin
- bob

> logout
[Returns to login screen]
```

#### 6. Calculator
```
> calc 15 + 27
Result: 42

> calc 100 / 4
Result: 25

> calc 17 % 5
Result: 2
```

#### 7. Theme Customization
```
> theme cyber
Theme applied.
[Screen changes to cyan text on blue background]

> theme matrix
Theme applied.
[Returns to green on black]
```

#### 8. System Logs
```
> logs
--- SYSTEM LOGS ---
[03:45:12 PM] Kernel booting...
[03:45:12 PM] Logger Initialized.
[03:45:13 PM] Interrupts Enabled.
[03:45:13 PM] RAMDisk initialized.
[03:45:13 PM] ATA disk driver initialized.
[03:45:14 PM] User logged in.
...
```

---

## Key Concepts Explained

### Why No Dynamic Memory?
323OS uses static arrays for simplicity. A real OS would implement:
- `malloc()` / `free()` using heap management
- Page frame allocator
- Virtual memory with paging

### Why No Multitasking?
True multitasking requires:
- Task switching (saving/restoring CPU state)
- Scheduler (deciding which task runs)
- Process isolation (separate memory spaces)

323OS runs a single kernel loop instead.

### Why Fixed File Sizes?
Real filesystems (ext4, NTFS) use:
- Block allocation tables
- Fragmentation handling
- Dynamic block allocation

323OS pre-allocates 4 sectors per file for simplicity.

### Security Limitations
- **No memory protection:** All code runs in ring 0
- **Plain text passwords:** Real systems use hashing (SHA-256, bcrypt)
- **No access control:** Any user can access any file

---

## Technical Achievements

### What 323OS Successfully Implements:
✅ Protected mode (GDT/IDT)  
✅ Hardware interrupt handling  
✅ Timer-driven events  
✅ Keyboard input with modifier keys  
✅ VGA text mode graphics  
✅ ATA disk I/O (LBA 28-bit addressing)  
✅ Persistent filesystem with directories  
✅ Multi-user authentication  
✅ Interactive text editor  
✅ Real-time clock integration  
✅ Command-line interface  
✅ System logging  

### Learning Outcomes:
- **Low-level programming:** Assembly and C integration
- **Hardware interaction:** Port I/O, memory-mapped I/O
- **OS architecture:** Layered design (hardware → drivers → kernel → userspace)
- **System programming:** Interrupt handling, device drivers
- **File systems:** Metadata management, sector allocation
- **Build systems:** Compilation, linking, bootable images

---

## Troubleshooting

### Common Issues

**Kernel doesn't boot:**
- Check multiboot header alignment
- Verify GRUB configuration
- Ensure kernel.bin is at `iso/boot/kernel.bin`

**Keyboard not working:**
- Verify IRQ1 is mapped to interrupt 33
- Check PIC remapping
- Ensure interrupts are enabled (`sti`)

**Disk read/write fails:**
- Verify QEMU has disk attached
- Check ATA wait loops don't timeout
- Ensure LBA calculations are correct

**Screen corruption:**
- Verify VGA buffer writes don't exceed bounds
- Check color bytes are formatted correctly
- Ensure cursor updates use correct coordinates

---

## Conclusion

323OS demonstrates fundamental operating system concepts in approximately 2000 lines of code. While simplified compared to production systems, it successfully:

1. **Boots independently** via GRUB
2. **Manages CPU protection** with GDT/IDT
3. **Handles hardware interrupts** from timer and keyboard
4. **Interacts with devices** (VGA, ATA disk, keyboard, RTC)
5. **Provides persistent storage** with a custom filesystem
6. **Implements user authentication** with disk-backed database
7. **Offers interactive interface** with 30+ commands

This codebase serves as an excellent educational resource for understanding how operating systems work at the lowest level, without the complexity of modern OS features like paging, scheduling, or networking.

---

**End of Documentation**

*For questions or clarifications, refer to inline comments in the source code or consult OS development resources like OSDev Wiki.*
