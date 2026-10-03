#ifndef NEWS_WIICONNECT24_H
#define NEWS_WIICONNECT24_H

#include <types.h>

struct NewsHeader;
typedef struct MEMiHeapHead* MEMHeapHandle;

// WiiConnect24.cpp: news download requests, run on a worker thread. Each request returns the
// index of its slot in gWC24Tasks (-1 when the queue is full).
void WC24Init();
void WC24Calc();
void WC24Shutdown(u32 event);
s32 WC24RequestDownload(MEMHeapHandle heap, u32 tmpHeap, NewsHeader** files, u32* times,
                        u32* sizes, const char* url, u32 vfSize);
s32 WC24RequestUpdate(MEMHeapHandle heap, u32 tmpHeap, NewsHeader** files, u32* times,
                      u32* sizes, u32 mask);
s32 WC24RequestRegister(const char* url, u32 vfSize, u32 force, u8 interval, u16 count);
s32 WC24RequestUnregister();

#endif
