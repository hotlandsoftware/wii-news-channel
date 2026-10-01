#ifndef MSL_STDDEF_H
#define MSL_STDDEF_H

#include <types.h>

typedef long ptrdiff_t;

#ifndef offsetof
#define offsetof(type, member) ((size_t)&(((type*)0)->member))
#endif

#endif
