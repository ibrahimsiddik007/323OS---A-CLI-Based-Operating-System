; kernel/kernel.asm - The Bootstrap Assembly
; This is the very first code executed when 323OS starts.

bits 32 ; We are in 32-bit Protected Mode (handed over by GRUB)

; --- MULTIBOOT HEADER ---
; These values allow GRUB to recognize this file as a bootable kernel.
MBALIGN     equ  1 << 0     ; Align loaded modules on page boundaries
MEMINFO     equ  1 << 1     ; Provide a memory map to the kernel
FLAGS       equ  MBALIGN | MEMINFO
MAGIC       equ  0x1BADB002 ; The "Magic Number" that GRUB looks for
CHECKSUM    equ  -(MAGIC + FLAGS) ; Verified by GRUB to ensure no corruption

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

; --- KERNEL STACK ---
; We must reserve memory for the CPU to use for function calls and local variables.
section .bss
align 16
stack_bottom:
    resb 16384 ; Reserve 16 Kilobytes of uninitialized memory for the stack
stack_top:

; --- ENTRY POINT ---
section .text
global _start
extern kernel_main ; The C function in kernel.c

_start:
    ; 1. Setup the Stack Pointer (esp)
    ; In x86, the stack grows DOWNWARDS, so we point to the TOP of the reserved block.
    mov esp, stack_top

    ; 2. Call the C Kernel
    ; This transfers control from Assembly to our main C logic.
    call kernel_main

    ; 3. Safety Loop
    ; If kernel_main() ever returns (it shouldn't), we halt the CPU.
    cli         ; Disable all interrupts
.hang:
    hlt         ; Put CPU into low-power sleep
    jmp .hang   ; Infinite loop as a failsafe