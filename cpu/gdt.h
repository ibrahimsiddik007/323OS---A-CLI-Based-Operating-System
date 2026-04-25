/* cpu/gdt.h */
#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/* Defines a GDT entry. We use __attribute__((packed)) to tell GCC not to change any alignment */
struct gdt_entry_struct {
    uint16_t limit_low;     // The lower 16 bits of the limit
    uint16_t base_low;      // The lower 16 bits of the base
    uint8_t  base_middle;   // The next 8 bits of the base
    uint8_t  access;        // Access flags, determine what ring this segment can be used in
    uint8_t  granularity;
    uint8_t  base_high;     // The last 8 bits of the base
} __attribute__((packed));

typedef struct gdt_entry_struct gdt_entry_t;

/* Pointer structure to give to the lgdt instruction */
struct gdt_ptr_struct {
    uint16_t limit;               // The upper 16 bits of all selector limits
    uint32_t base;                // The address of the first gdt_entry_t struct
} __attribute__((packed));

typedef struct gdt_ptr_struct gdt_ptr_t;

/* Initialization function */
void init_gdt();

#endif
