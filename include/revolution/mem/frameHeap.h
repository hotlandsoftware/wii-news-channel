#ifndef FRAMEHEAP_H
#define FRAMEHEAP_H

// From ogws (mem_frameHeap.h); Petari has no frame heap header.

#include <revolution/mem/heapCommon.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEM_FRM_HEAP_MIN_SIZE (sizeof(MEMiHeapHead) + sizeof(MEMiFrmHeapHead))

// Specify how to free memory
typedef enum {
    MEM_FRM_HEAP_FREE_TO_HEAD = (1 << 0),
    MEM_FRM_HEAP_FREE_TO_TAIL = (1 << 1),
    MEM_FRM_HEAP_FREE_ALL = MEM_FRM_HEAP_FREE_TO_HEAD | MEM_FRM_HEAP_FREE_TO_TAIL
} MEMiFrmFreeFlag;

typedef struct MEMiFrmHeapState {
    u32 id;                        // at 0x0
    u8* head;                      // at 0x4
    u8* tail;                      // at 0x8
    struct MEMiFrmHeapState* next; // at 0xC
} MEMiFrmHeapState;

// Placed in heap after base heap head
typedef struct MEMiFrmHeapHead {
    u8* head;                 // at 0x0
    u8* tail;                 // at 0x4
    MEMiFrmHeapState* states; // at 0x8
} MEMiFrmHeapHead;

MEMHeapHandle MEMCreateFrmHeapEx(void* start, u32 size, u16 opt);
MEMHeapHandle MEMDestroyFrmHeap(MEMHeapHandle heap);
void* MEMAllocFromFrmHeapEx(MEMHeapHandle heap, u32 size, s32 align);
void MEMFreeToFrmHeap(MEMHeapHandle heap, u32 flags);
u32 MEMGetAllocatableSizeForFrmHeapEx(MEMHeapHandle heap, s32 align);
BOOL MEMRecordStateForFrmHeap(MEMHeapHandle heap, u32 id);
BOOL MEMFreeByStateToFrmHeap(MEMHeapHandle heap, u32 id);
u32 MEMAdjustFrmHeap(MEMHeapHandle heap);
u32 MEMResizeForMBlockFrmHeap(MEMHeapHandle heap, void* memBlock, u32 size);

static inline MEMHeapHandle MEMCreateFrmHeap(void* start, u32 size) {
    return MEMCreateFrmHeapEx(start, size, 0);
}

static inline void* MEMAllocFromFrmHeap(MEMHeapHandle heap, u32 size) {
    return MEMAllocFromFrmHeapEx(heap, size, 4);
}

static inline u32 MEMGetAllocatableSizeForFrmHeap(MEMHeapHandle heap) {
    return MEMGetAllocatableSizeForFrmHeapEx(heap, 4);
}

#ifdef __cplusplus
}
#endif

#endif  // FRAMEHEAP_H
