#ifndef REVOLUTION_OS_H
#define REVOLUTION_OS_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

void DCFlushRange(void* addr, u32 len);

void OSReport(const char* msg, ...);

typedef int OSHeapHandle;
extern volatile OSHeapHandle __OSCurrHeap;

void* OSGetArenaHi(void);
void* OSGetArenaLo(void);
void OSSetArenaLo(void* addr);
void* OSInitAlloc(void* arenaStart, void* arenaEnd, int maxHeaps);
OSHeapHandle OSCreateHeap(void* start, void* end);
OSHeapHandle OSSetCurrentHeap(OSHeapHandle heap);
void OSFreeToHeap(OSHeapHandle heap, void* ptr);

#define OSRoundUp32B(x) (((u32)(x) + 32 - 1) & ~(32 - 1))
#define OSRoundDown32B(x) (((u32)(x)) & ~(32 - 1))

#ifdef __cplusplus
}
#endif

#endif
