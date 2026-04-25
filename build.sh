#!/bin/bash
set -e
echo "--- Building 323OS v3.1 (MultiTasking) ---"

rm -f kernel/*.o kernel/*.bin iso/boot/*.bin cpu/*.o drivers/*.o logs/*.o *.iso

# 1. Assemble
echo "[*] Assembling..."
nasm -f elf32 kernel/kernel.asm -o kernel/kernel_asm.o
nasm -f elf32 cpu/gdt.asm -o cpu/gdt_asm.o
nasm -f elf32 cpu/interrupt.asm -o cpu/interrupt_asm.o

# 2. Compile C
echo "[*] Compiling C..."
gcc -m32 -ffreestanding -c kernel/kernel.c -o kernel/kernel_c.o
gcc -m32 -ffreestanding -c kernel/utils.c -o kernel/utils.o
gcc -m32 -ffreestanding -c drivers/display.c -o drivers/display.o
gcc -m32 -ffreestanding -c drivers/ata.c -o drivers/ata.o
gcc -m32 -ffreestanding -c drivers/filesystem.c -o drivers/filesystem.o
gcc -m32 -ffreestanding -c logs/logger.c -o logs/logger.o
gcc -m32 -ffreestanding -c cpu/timer.c -o cpu/timer.o
gcc -m32 -ffreestanding -c cpu/gdt.c -o cpu/gdt_c.o
gcc -m32 -ffreestanding -c cpu/idt.c -o cpu/idt_c.o
gcc -m32 -ffreestanding -c cpu/isr.c -o cpu/isr_c.o

# 3. Link
echo "[*] Linking..."
ld -m elf_i386 -T linker.ld -o kernel/kernel.bin \
    kernel/kernel_asm.o \
    kernel/kernel_c.o \
    kernel/utils.o \
    drivers/display.o \
    drivers/ata.o \
    drivers/filesystem.o \
    logs/logger.o \
    cpu/timer.o \
    cpu/gdt_asm.o \
    cpu/gdt_c.o \
    cpu/idt_c.o \
    cpu/isr_c.o \
    cpu/interrupt_asm.o

# 4. Verify & ISO
if grub-file --is-x86-multiboot kernel/kernel.bin; then
    echo "[*] Multiboot Confirmed"
else
    echo "[!] File is not Multiboot compliant"
    exit 1
fi

echo "[*] Creating ISO..."
mkdir -p iso/boot/grub
cp kernel/kernel.bin iso/boot/
cat > iso/boot/grub/grub.cfg << EOF
menuentry "323OS" {
    multiboot /boot/kernel.bin
}
EOF
grub-mkrescue -o 323os.iso iso 2> /dev/null

# 5. Create persistent disk image if it doesn't exist
if [ ! -f disk.img ]; then
    echo "[*] Creating persistent disk image..."
    dd if=/dev/zero of=disk.img bs=1M count=10 2> /dev/null
fi

echo "[*] Launching 323OS..."
qemu-system-x86_64 -cdrom 323os.iso -drive file=disk.img,format=raw -m 512M
