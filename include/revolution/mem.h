#ifndef MEM_H
#define MEM_H

// MEMAllocator/MEMHeapHandle layouts are relied on by game code (HBMDataInfo,
// nw4r::lyt); Petari's match our original mem.h.
#include <revolution/mem/allocator.h>
#include <revolution/mem/expHeap.h>
#include <revolution/mem/frameHeap.h>
#include <revolution/mem/heapCommon.h>
#include <revolution/mem/list.h>

#endif // MEM_H
