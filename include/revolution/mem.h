#ifndef REVOLUTION_MEM_H
#define REVOLUTION_MEM_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MEMiHeapHead* MEMHeapHandle;

typedef struct MEMAllocator {
    const struct MEMAllocatorFunc* func; // at 0x0
    void* heap;                          // at 0x4
    u32 heapParam1;                      // at 0x8
    u32 heapParam2;                      // at 0xC
} MEMAllocator;

#ifdef __cplusplus
}
#endif

#endif
