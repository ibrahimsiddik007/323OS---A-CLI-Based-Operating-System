/* cpu/timer.c - System Timer (PIT) Driver */
#include "timer.h"
#include "isr.h"
#include "ports.h"
#include "../drivers/display.h"

// Global tick counter - 'volatile' as it changes in an interrupt
volatile uint32_t tick = 0;

/* This function is called every time the timer fires (IRQ0) */
static void timer_callback(registers_t regs) {
    tick++; // Increment global time
}

/**
 * @brief Programs the PIT (Programmable Interval Timer) to a specific frequency.
 * 
 * @param freq Frequency in Hertz (e.g., 50 means 50 times per second)
 */
void init_timer(uint32_t freq) {
    // Register our callback function with the interrupt system
    register_interrupt_handler(IRQ0, timer_callback);

    // The PIT runs at 1.193182 MHz. We divide it to get our target frequency.
    uint32_t divisor = 1193180 / freq;

    // Send the command byte (0x36 = Channel 0, Square Wave, 16-bit divisor)
    outb(0x43, 0x36);

    // Send the divisor (Low byte then High byte)
    uint8_t l = (uint8_t)(divisor & 0xFF);
    uint8_t h = (uint8_t)( (divisor >> 8) & 0xFF );
    outb(0x40, l);
    outb(0x40, h);
}