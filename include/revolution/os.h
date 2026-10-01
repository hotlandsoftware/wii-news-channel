#ifndef REVOLUTION_OS_H
#define REVOLUTION_OS_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

void DCFlushRange(void* addr, u32 len);

#ifdef __cplusplus
}
#endif

#endif
