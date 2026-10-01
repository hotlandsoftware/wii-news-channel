#ifndef STRING_H
#define STRING_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

void* memset(void* dst, int val, size_t n);
char* strncpy(char* dst, const char* src, size_t n);
void* memcpy(void* dst, const void* src, size_t n);
void* memchr(const void* src, int val, size_t n);
size_t strlen(const char* str);
char* strcpy(char* dst, const char* src);
char* strchr(const char* str, int c);

void* memmove(void* dst, const void* src, size_t n);
int memcmp(const void* src1, const void* src2, size_t n);
char* strcat(char* dst, const char* src);
char* strncat(char* dst, const char* src, size_t n);
int strcmp(const char* str1, const char* str2);
int strncmp(const char* str1, const char* str2, size_t n);
char* strrchr(const char* str, int chr);
char* strpbrk(const char* str, const char* set);
size_t strspn(const char* str, const char* set);
size_t strcspn(const char* str, const char* set);
char* strstr(const char* str, const char* pat);

#ifdef __cplusplus
}
#endif

#endif
