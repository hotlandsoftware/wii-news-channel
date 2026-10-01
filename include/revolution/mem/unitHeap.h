#ifndef UNITHEAP_H
#define UNITHEAP_H

// Unit heap (not in Petari/ogws; layout from mkw's rvl/mem/unitHeap.h).

#include <revolution/mem/heapCommon.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MEMiUntHeapMBlockHead MEMiUntHeapMBlockHead;

struct MEMiUntHeapMBlockHead {
    MEMiUntHeapMBlockHead* pMBlkHdNext;  // at 0x0
};

typedef struct MEMiUntMBlockList {
    MEMiUntHeapMBlockHead* head;  // at 0x0
} MEMiUntMBlockList;

// Placed in heap after base heap head
typedef struct MEMiUntHeapHead {
    MEMiUntMBlockList mbFreeList;  // at 0x0
    u32 mBlkSize;                  // at 0x4
} MEMiUntHeapHead;

MEMHeapHandle MEMCreateUnitHeapEx(void* startAddress, u32 heapSize, u32 memBlockSize, int alignment, u16 optFlag);
void* MEMDestroyUnitHeap(MEMHeapHandle heap);
void* MEMAllocFromUnitHeap(MEMHeapHandle heap);
void MEMFreeToUnitHeap(MEMHeapHandle heap, void* memBlock);
u32 MEMCountFreeBlockForUnitHeap(MEMHeapHandle heap);
u32 MEMCalcHeapSizeForUnitHeap(u32 memBlockSize, u32 memBlockNum, int alignment);

static inline MEMHeapHandle MEMCreateUnitHeap(void* startAddress, u32 heapSize, u32 memBlockSize) {
    return MEMCreateUnitHeapEx(startAddress, heapSize, memBlockSize, 4, 0);
}

#ifdef __cplusplus
}
#endif

#endif  // UNITHEAP_H
