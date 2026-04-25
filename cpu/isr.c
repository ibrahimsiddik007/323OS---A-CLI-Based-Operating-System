/* cpu/isr.c - Interrupt Service Routine Dispatcher */
#include "isr.h"
#include "idt.h"
#include "../drivers/display.h"
#include "../cpu/ports.h"
#include "../kernel/utils.h"

// Array of function pointers for specific interrupt handlers
isr_t interrupt_handlers[256];

/* External ASM stubs we jump to */
extern void isr0(); extern void isr1(); extern void isr8(); extern void isr13();
extern void isr32(); extern void isr33();

/* Map CPU exceptions (0-31) we care about) */
void isr_install() {
    // Division by zero
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);
    // Debug (placeholder) -> reuse isr1 stub
    idt_set_gate(1, (uint32_t)isr1, 0x08, 0x8E);
    // Double fault
    idt_set_gate(8, (uint32_t)isr8, 0x08, 0x8E);
    // General protection fault
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
}

/* Map hardware IRQs (PIC remapped to 32-47) */
void irq_install() {
    // Timer (IRQ0 -> int 32)
    idt_set_gate(32, (uint32_t)isr32, 0x08, 0x8E);
    // Keyboard (IRQ1 -> int 33)
    idt_set_gate(33, (uint32_t)isr33, 0x08, 0x8E);
}

/* Registers a function to be called when a specific interrupt occurs */
void register_interrupt_handler(uint8_t n, isr_t handler) {
    interrupt_handlers[n] = handler;
}

static void to_hex(uint32_t value, char* out) {
    const char* hex = "0123456789ABCDEF";
    out[0] = '0'; out[1] = 'x';
    for (int i = 0; i < 8; i++) {
        out[2 + i] = hex[(value >> (28 - i * 4)) & 0xF];
    }
    out[10] = 0;
}

/* Main dispatcher for CPU Exceptions (Int 0-31) */
void isr_handler(registers_t r) {
    char buf[16];
    terminal_writestring("CPU EXCEPTION: ");
    itoa(r.int_no, buf);
    terminal_writestring(buf);

    char hexbuf[12];
    to_hex(r.err_code, hexbuf);
    terminal_writestring("  ERR: ");
    terminal_writestring(hexbuf);

    to_hex(r.eip, hexbuf);
    terminal_writestring("  EIP: ");
    terminal_writestring(hexbuf);

    terminal_writestring("\nSystem Halted.");
    asm volatile("hlt"); // Stop the CPU on critical error
}

/* Main dispatcher for Hardware Interrupts (Int 32-47) */
void irq_handler(registers_t r) {
    // 1. Send End-Of-Interrupt (EOI) to PIC chips
    // If IRQ came from Slave PIC (Int 40-47), send to both
    if (r.int_no >= 40) outb(0xA0, 0x20);
    // Always send to Master PIC
    outb(0x20, 0x20);

    // 2. Call the specific handler if one is registered
    if (interrupt_handlers[r.int_no] != 0) {
        isr_t handler = interrupt_handlers[r.int_no];
        handler(r);
    }
}