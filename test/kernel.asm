[bits 32]
[org 0x100000]

start:
    mov si, msg
.print:
    lodsb
    cmp al, 0
    je .done
    mov ah, 0x0E       ; teletype print function
    int 0x10           ; BIOS interrupt
    jmp .print
.done:
    hlt                 ; halt CPU

msg db 'Hello, SheaLOS!',0
