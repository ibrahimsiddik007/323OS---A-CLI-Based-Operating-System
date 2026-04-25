/* kernel/utils.h */
#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>

int strcmp(const char* s1, const char* s2);
size_t strlen(const char* str);
void append(char* s, char n);
void backspace(char* s);
void reverse(char* s);
void itoa(int n, char* str);
int atoi(char* str);
void strcpy(char* dest, const char* src);
void strcat(char* dest, const char* src);
char* skip_spaces(char* ptr);

#endif
