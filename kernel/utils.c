/* kernel/utils.c - Common String and Memory Helpers */
#include "utils.h"

/* Integer to String conversion (Base 10) */
void itoa(int n, char* str) {
    int i, sign;
    if ((sign = n) < 0) n = -n; // Handle negative numbers
    i = 0;
    do {
        str[i++] = n % 10 + '0'; // Extract last digit
    } while ((n /= 10) > 0);   // Move to next digit
    if (sign < 0) str[i++] = '-';
    str[i] = '\0';
    // The string is currently reversed, so we must reverse it back
    reverse(str);
}

/* Reverses a string in-place */
void reverse(char* s) {
    int i, j;
    char c;
    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/* Returns the length of a string */
size_t strlen(const char* s) {
    size_t i = 0;
    while (s[i] != '\0') i++;
    return i;
}

/* Compares two strings. Returns 0 if identical. */
int strcmp(const char* s1, const char* s2) {
    int i;
    for (i = 0; s1[i] == s2[i]; i++) {
        if (s1[i] == '\0') return 0;
    }
    return s1[i] - s2[i];
}

/* Appends a character to the end of a string */
void append(char* s, char n) {
    int len = strlen(s);
    s[len] = n;
    s[len+1] = '\0';
}

/* Removes the last character from a string */
void backspace(char* s) {
    int len = strlen(s);
    if (len > 0) s[len-1] = '\0';
}

/* Converts a string to an integer (base 10, simple) */
int atoi(char* str) {
    int res = 0;
    int sign = 1;
    int i = 0;
    if (str[0] == '-') { sign = -1; i++; }
    for (; str[i] != '\0'; ++i) {
        if (str[i] >= '0' && str[i] <= '9')
            res = res * 10 + str[i] - '0';
    }
    return sign * res;
}

/* Copies a C-string from src to dest */
void strcpy(char* dest, const char* src) {
    int i = 0;
    while (src[i] != 0) { dest[i] = src[i]; i++; }
    dest[i] = 0;
}

/* Concatenates src to the end of dest */
void strcat(char* dest, const char* src) {
    int i = strlen(dest);
    int j = 0;
    while (src[j] != 0) { dest[i++] = src[j++]; }
    dest[i] = 0;
}

/* Skip leading spaces in a string and return pointer to first non-space */
char* skip_spaces(char* ptr) {
    while (*ptr == ' ') ptr++;
    return ptr;
}