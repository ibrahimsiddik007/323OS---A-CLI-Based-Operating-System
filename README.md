# 323OS

A small OS for x86 (32-bit) written in C and Assembly. It boots via GRUB, brings up basic CPU protections and interrupts, exposes a text-mode UI, a shell/editor, and a tiny persistent filesystem backed by an ATA disk image.

## Course Info
- Course: CSE323 : Operating System Design
- University: North South University
- Semester: Spring 2026
- Members: MD. Ibrahim Siddik, S.M. Atiqur Islam
- Faculty: Dr. Safat Siddiqui

## Highlights
- Boots with GRUB and a Multiboot-compliant kernel
- GDT/IDT setup with timer + keyboard interrupts
- VGA text-mode display driver
- ATA PIO driver with a tiny persistent filesystem
- Simple shell, editor, users, and logs


## Video
[![323OS Demo](https://img.youtube.com/vi/Q6ztMZQLFjI/hqdefault.jpg)](https://youtu.be/Q6ztMZQLFjI)
<br>
<a href="https://youtu.be/Q6ztMZQLFjI" target="_blank" rel="noopener">Watch this on YouTube</a>

## Build and Run

### Prerequisite
- `nasm`
- `gcc` with 32-bit support
- `ld`
- `grub-mkrescue`
- `qemu-system-x86_64`

### One-step build + launch
```bash
./build.sh
```

This builds the kernel, creates `323os.iso`, and launches QEMU. A persistent `disk.img` (10MB) is created on first run and reused afterward.

## Quick Demo
1. Run `./build.sh`
2. Log in (default in docs: `admin` / `1234`)
3. First boot only: `format`
4. Create a file: `edit test.txt` (Ctrl+S to save)
5. List files: `ls`
6. Reboot and confirm persistence with `ls`

## Shell Commands (partial)
- `ls` - list files on disk
- `edit <file>` - create/edit a text file (Ctrl+S to save)
- `cat <file>` - show file contents
- `rm <file>` - delete a file
- `format` - format disk (deletes all files)
- `useradd <name> <pass>` - add a user

## Project Layout
- cpu/ - GDT/IDT, timer, interrupts
- drivers/ - display, ATA, filesystem
- kernel/ - boot entry + kernel main
- logs/ - kernel logging
- docs/ - deeper technical notes

See the full technical write-up in [docs/COMPLETE_DOCUMENTATION.md](docs/COMPLETE_DOCUMENTATION.md) and disk details in [docs/DISK_STORAGE.md](docs/DISK_STORAGE.md).


