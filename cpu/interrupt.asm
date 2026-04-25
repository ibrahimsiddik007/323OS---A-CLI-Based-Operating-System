; cpu/interrupt.asm - Low-Level Interrupt Handling
; This file catches hardware/CPU signals and routes them to C functions.

[extern isr_handler] ; C function in isr.c
[extern irq_handler] ; C function in isr.c

global isr0
global isr1
global isr8
global isr13
global isr32
global isr33

; --- CPU EXCEPTIONS ---

; 0: Divide By Zero
isr0:
    cli             ; Disable interrupts for safety
    push byte 0     ; Dummy error code
    push byte 0     ; Interrupt number
    jmp isr_common_stub

; 1: Debug Exception
isr1:
    cli
    push byte 0     ; Dummy error code
    push byte 1     ; Interrupt number
    jmp isr_common_stub

; 8: Double Fault (Severe CPU error)
isr8:
    cli
    push byte 8     ; Interrupt number (error code already pushed by CPU)
    jmp isr_common_stub

; 13: General Protection Fault (Memory access violation)
isr13:
    cli
    push byte 13
    jmp isr_common_stub

; --- HARDWARE INTERRUPTS (IRQs) ---

; 32: IRQ0 (System Timer)
isr32:
    cli
    push byte 0     ; Dummy error code
    push byte 32    ; Interrupt index (32 = IRQ0)
    jmp irq_common_stub

; 33: IRQ1 (Keyboard)
isr33:
    cli
    push byte 0
    push byte 33    ; Interrupt index (33 = IRQ1)
    jmp irq_common_stub

; --- COMMON STUBS (Context Saving) ---

; Generic handler for CPU exceptions
isr_common_stub:
    pusha           ; Save ALL general purpose registers (eax, ebx...)
    
    mov ax, ds      ; Save current data segment
    push eax
    
    mov ax, 0x10    ; Load Kernel Data Segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    call isr_handler ; Jump to C code!
    
    pop eax         ; Restore data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    popa            ; Restore all registers
    add esp, 8      ; Clean up stack (pop int_no and error_code)
    sti             ; Re-enable interrupts
    iret            ; Return from interrupt (CPU pops cs, eip, eflags)

; Generic handler for Hardware IRQs
irq_common_stub:
    pusha           ; Save state
    mov ax, ds
    push eax
    mov ax, 0x10    ; Switch to kernel mode
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    call irq_handler ; Jump to C dispatcher
    
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8
    sti
    iret            ; Return to whatever the CPU was doing