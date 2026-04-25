/* kernel/kernel.h */
#ifndef KERNEL_H
#define KERNEL_H

#include <stddef.h>
#include <stdint.h>
#include "../drivers/display.h" // <--- Import colors from here

void kernel_main(void);

/* Helpers */
long get_time_seconds(void);

/* System Commands */
void shutdown(void);
void reboot(void);
void cpuid_info(void);
void sysinfo(void);
void print_time(void);
void memdump(void);

#endif
