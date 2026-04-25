/* drivers/display.c */
#include "display.h"
#include "../cpu/ports.h" 

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
volatile uint16_t* vga_buffer = (uint16_t*) 0xB8000;

size_t terminal_col = 0;
size_t terminal_row = 0;
uint8_t terminal_color; 

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}

void update_cursor(int x, int y) {
    uint16_t pos = y * VGA_WIDTH + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t) (pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
    const size_t index = y * VGA_WIDTH + x;
    vga_buffer[index] = vga_entry(c, color);
}

void terminal_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            vga_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
    terminal_col = 0;
    terminal_row = 0;
    update_cursor(0,0);
}

/* THE PARTIAL CLEAR FUNCTION */
void terminal_clear_area(int start_row) {
    for (size_t y = start_row; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            vga_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
    terminal_col = 0;
    terminal_row = start_row;
    update_cursor(0, terminal_row);
}

void terminal_initialize(void) {
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_clear();
}

void terminal_set_color(uint8_t color) {
    terminal_color = color;
}

void terminal_putc(char c) {
    if (c == '\n') {
        terminal_col = 0;
        terminal_row++;
    } else if (c == '\b') {
        if (terminal_col > 0) {
            terminal_col--;
            const size_t index = terminal_row * VGA_WIDTH + terminal_col;
            vga_buffer[index] = vga_entry(' ', terminal_color);
        }
    } else {
        const size_t index = terminal_row * VGA_WIDTH + terminal_col;
        vga_buffer[index] = vga_entry(c, terminal_color);
        terminal_col++;
    }

    if (terminal_col >= VGA_WIDTH) {
        terminal_col = 0;
        terminal_row++;
    }
    if (terminal_row >= VGA_HEIGHT) {
        /* Scroll substitute: just clear workspace if we hit bottom */
        terminal_clear_area(5); 
    }
    update_cursor(terminal_col, terminal_row);
}

void terminal_writestring(const char* data) {
    for (size_t i = 0; data[i] != 0; i++)
        terminal_putc(data[i]);
}
