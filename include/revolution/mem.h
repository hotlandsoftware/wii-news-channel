#ifndef REVOLUTION_MEM_H
#define REVOLUTION_MEM_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MEMiHeapHead* MEMHeapHandle;

typedef struct MEMAllocator MEMAllocator;

typedef void* (*MEMFuncAllocatorAlloc)(MEMAllocator* allocator, u32 size);
typedef void (*MEMFuncAllocatorFree)(MEMAllocator* allocator, void* block);

typedef struct MEMAllocatorFunc {
    MEMFuncAllocatorAlloc pfAlloc;
    MEMFuncAllocatorFree pfFree;
} MEMAllocatorFunc;

struct MEMAllocator {
    const MEMAllocatorFunc* pFunc; // at 0x0
    void* pHeap;                   // at 0x4
    u32 heapParam1;                // at 0x8
    u32 heapParam2;                // at 0xC
};

u32 MEMGetTotalFreeSizeForExpHeap(MEMHeapHandle heap);
void MEMFreeToAllocator(MEMAllocator* allocator, void* block);

#ifdef __cplusplus
}
#endif

#endif
