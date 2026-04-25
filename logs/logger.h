/* logs/logger.h */
#ifndef LOGGER_H
#define LOGGER_H

void logger_init(void);
void klog(char* message); /* Kernel Log */
void klog_dump(void);     /* Print all logs (dmesg) */

#endif
