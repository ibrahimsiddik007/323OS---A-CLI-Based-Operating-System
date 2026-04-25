/* linker.ld - The Master Map of 323OS */
/* This file tells the 'ld' linker exactly where to put pieces of code in memory. */

ENTRY(_start) /* The CPU execution begins at the _start symbol in kernel.asm */

SECTIONS
{
    /* 
     * We load the kernel at 1MB (0x100000). 
     * This is standard for 32-bit kernels to avoid legacy BIOS memory areas.
     */
    . = 1M;

    /* The .text section contains the executable machine code. */
    .text :
    {
        *(.multiboot) /* The Multiboot header MUST be at the very start of the file. */
        *(.text)      /* All other code from all compiled C and ASM files. */
    }

    /* .rodata contains Read-Only Data (like "Hello World" strings). */
    .rodata :
    {
        *(.rodata)
    }

    /* .data contains initialized variables (e.g. 'int x = 5;'). */
    .data :
    {
        *(.data)
    }

    /* .bss contains uninitialized data and the stack (e.g. 'char buffer[1024];'). */
    .bss :
    {
        *(COMMON)
        *(.bss)
    }
}