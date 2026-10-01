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

#ifdef __cplusplus
}
#endif

#endif
