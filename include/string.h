#ifndef STRING_H
#define STRING_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

void* memset(void* dst, int val, size_t n);
char* strncpy(char* dst, const char* src, size_t n);

#ifdef __cplusplus
}
#endif

#endif
