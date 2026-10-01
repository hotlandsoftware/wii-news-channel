#ifndef MSL_STDLIB_H
#define MSL_STDLIB_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t wcstombs(char* dst, const wchar_t* src, size_t n);

#ifdef __cplusplus
}
#endif

#endif
