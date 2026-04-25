/* drivers/display.h */
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stddef.h>

enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

void terminal_initialize(void);
void terminal_set_color(uint8_t color);
void terminal_putc(char c);
void terminal_writestring(const char* data);
void terminal_clear(void);

/* This is the new command causing issues if missing */
void terminal_clear_area(int start_row);

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y);
void update_cursor(int x, int y);

uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg);

extern size_t terminal_row;
extern size_t terminal_col;
extern uint8_t terminal_color;

#endif