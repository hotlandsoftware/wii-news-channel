#ifndef MSL_WCHAR_H
#define MSL_WCHAR_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t wcslen(const wchar_t* str);
wchar_t* wcscpy(wchar_t* dst, const wchar_t* src);
wchar_t* wcscat(wchar_t* dst, const wchar_t* src);
struct _FILE;
int fwide(struct _FILE* file, int mode);
int swprintf(wchar_t* s, size_t n, const wchar_t* format, ...);

#ifdef __cplusplus
}
#endif

#endif
