/* logs/logger.c - System Event Logging (Ring Buffer) */
#include "logger.h"
#include "../drivers/display.h"
#include "../kernel/utils.h"

// Circular buffer to store logs without running out of memory
#define MAX_LOGS 20
#define LOG_LENGTH 64
char log_buffer[MAX_LOGS][LOG_LENGTH];
int log_head = 0; // Index for the next log entry
int log_count = 0; // Total logs since boot

/* Initializes the logger - clears the buffer */
void logger_init() {
    for(int i=0; i<MAX_LOGS; i++) log_buffer[i][0] = 0;
    klog("Logger Initialized.");
}

/* Adds a message to the ring buffer */
void klog(char* message) {
    // Copy message into the current head position
    int i = 0;
    while(message[i] && i < LOG_LENGTH-1) {
        log_buffer[log_head][i] = message[i];
        i++;
    }
    log_buffer[log_head][i] = 0; // Null-terminate

    // Advance head and wrap around if at the end (Circular)
    log_head = (log_head + 1) % MAX_LOGS;
    log_count++;
}

/* Prints all stored logs to the screen */
void klog_dump() {
    terminal_writestring("--- SYSTEM LOGS ---\n");
    int start = (log_count > MAX_LOGS) ? log_head : 0;
    int limit = (log_count > MAX_LOGS) ? MAX_LOGS : log_head;

    for(int i=0; i<limit; i++) {
        int idx = (start + i) % MAX_LOGS;
        terminal_writestring(log_buffer[idx]);
        terminal_writestring("\n");
    }
}