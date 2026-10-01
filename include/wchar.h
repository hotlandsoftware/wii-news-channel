#ifndef MSL_WCHAR_H
#define MSL_WCHAR_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t wcslen(const wchar_t* str);
wchar_t* wcscpy(wchar_t* dst, const wchar_t* src);

#ifdef __cplusplus
}
#endif

#endif
