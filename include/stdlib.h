#ifndef MSL_STDLIB_H
#define MSL_STDLIB_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int quot;
    int rem;
} div_t;

void free(void* ptr);

int abs(int n);
div_t div(int numerator, int denominator);

void* bsearch(const void* key, const void* base, size_t num, size_t size,
              int (*compare)(const void*, const void*));

int mbtowc(wchar_t* dst, const char* src, size_t n);
size_t mbstowcs(wchar_t* dst, const char* src, size_t n);
size_t wcstombs(char* dst, const wchar_t* src, size_t n);

#ifdef __cplusplus
}
#endif

#endif
