/* cpu/gdt.c - Global Descriptor Table Implementation */
#include "gdt.h"

// The GDT array and the pointer that the CPU's LGDT instruction will use
gdt_entry_t gdt_entries[5];
gdt_ptr_t   gdt_ptr;

// Assembly function defined in gdt.asm to reload segment registers
extern void gdt_flush(uint32_t); 

/**
 * @brief Logic to fill a GDT entry (gate) with address and permission data.
 * 
 * @param num    The index in the GDT (0-4)
 * @param base   The starting memory address (0 for flat model)
 * @param limit  The size of the segment (0xFFFFFFFF for 4GB)
 * @param access The permission byte (Ring 0 vs Ring 3)
 * @param gran   Granularity (Page-level vs Byte-level)
 */
static void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    // Set base address bits (split across 3 fields in the descriptor)
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    // Set segment limit (size) bits
    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;

    // Set flags (Granularity, Size) and Access rights
    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access      = access;
}

/* Initializes the GDT with 5 mandatory segments */
void init_gdt() {
    // GDT Pointer points to the array and defines its size
    gdt_ptr.limit = (sizeof(gdt_entry_t) * 5) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    // 0: Null Segment (Required by hardware, never used)
    gdt_set_gate(0, 0, 0, 0, 0);

    // 1: Kernel Code Segment (Full 4GB access, Executable, Ring 0)
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    // 2: Kernel Data Segment (Full 4GB access, Writable, Ring 0)
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // 3: User Code Segment (Full 4GB access, Executable, Ring 3 - Limited)
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    // 4: User Data Segment (Full 4GB access, Writable, Ring 3 - Limited)
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    // Physically load the GDT into the CPU register
    gdt_flush((uint32_t)&gdt_ptr);
}