; cpu/gdt.asm - Assembly Helper for GDT & IDT
; These functions are called from C but must be in Assembly to access CPU registers.

global gdt_flush
global idt_flush

; @brief Loads the GDT into the CPU and reloads segment registers.
; Parameter [esp+4]: Address of the GDT pointer structure (gdt_ptr_t).
gdt_flush:
    mov eax, [esp+4]  ; Get the pointer passed from the C function
    lgdt [eax]        ; Load GDT (CPU instruction)

    ; We must update all segment registers to use the new GDT entries.
    ; 0x10 is the offset (index 2 * 8 bytes) for the Kernel Data Segment.
    mov ax, 0x10      
    mov ds, ax        ; Data Segment
    mov es, ax        ; Extra Segment
    mov fs, ax        ; F Segment
    mov gs, ax        ; G Segment
    mov ss, ax        ; Stack Segment (Now uses kernel stack)
    
    ; Far Jump: This is the ONLY way to reload the CS (Code Segment) register.
    ; 0x08 is the offset (index 1 * 8 bytes) for the Kernel Code Segment.
    jmp 0x08:.flush   
.flush:
    ret

; @brief Loads the Interrupt Descriptor Table into the CPU.
idt_flush:
    mov eax, [esp+4]  ; Address of idt_ptr_t
    lidt [eax]        ; Load IDT (CPU instruction)
    ret