/* cpu/idt.c - Interrupt Descriptor Table Implementation */
#include "idt.h"
#include "ports.h"
#include "../kernel/utils.h"

/* Custom memset as we don't use standard C library */
void* local_memset(void* ptr, int value, size_t num) {
    unsigned char* p = (unsigned char*)ptr;
    while (num--) *p++ = (unsigned char)value;
    return ptr;
}

// 256 entries for all possible CPU/Hardware interrupts
struct idt_entry_struct idt_entries[256];
struct idt_ptr_struct   idt_ptr;

/* Sets up a single entry in the IDT */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt_entries[num].base_low = base & 0xFFFF;
    idt_entries[num].base_high = (base >> 16) & 0xFFFF;
    idt_entries[num].sel     = sel;   // Usually 0x08 (Kernel Code Segment)
    idt_entries[num].always0 = 0;
    idt_entries[num].flags   = flags | 0x60; // 0x60 makes it accessible to Ring 3
}

/* PIC Remapping: Moving IRQs from 0-15 to 32-47 to avoid CPU conflicts */
void pic_remap() {
    outb(0x20, 0x11); // Start PIC initialization
    outb(0xA0, 0x11);
    
    outb(0x21, 0x20); // Master PIC offset = 32
    outb(0xA1, 0x28); // Slave PIC offset = 40
    
    outb(0x21, 0x04); // Cascade setup
    outb(0xA1, 0x02);
    
    outb(0x21, 0x01); // 8086 mode
    outb(0xA1, 0x01);
    
    outb(0x21, 0xFC);   // Only IRQ0 (timer) and IRQ1 (keyboard) unmasked
    outb(0xA1, 0xFF);   // All slave PIC IRQs masked
}

/* Prepares the CPU to handle interrupts */
void init_idt() {
    idt_ptr.limit = sizeof(struct idt_entry_struct) * 256 - 1;
    idt_ptr.base  = (uint32_t)&idt_entries;

    local_memset(&idt_entries, 0, sizeof(struct idt_entry_struct) * 256);
    
    pic_remap(); // Crucial: Remap IRQs before loading

    // Assembly installers defined in interrupt.asm
    extern void isr_install();
    extern void irq_install();
    isr_install(); // Install CPU exception handlers (0-31)
    irq_install(); // Install Hardware handlers (32-47)

    // Load the IDT into the CPU
    asm volatile("lidt (%0)" : : "r" (&idt_ptr));
}