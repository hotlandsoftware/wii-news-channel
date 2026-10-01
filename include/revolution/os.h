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

typedef struct OSCalendarTime {
    int sec;  // at 0x00
    int min;  // at 0x04
    int hour; // at 0x08
    int mday; // at 0x0C
    int mon;  // at 0x10
    int year; // at 0x14
    int wday; // at 0x18
    int yday; // at 0x1C
    int msec; // at 0x20
    int usec; // at 0x24
} OSCalendarTime;

typedef struct OSContext {
    u32 gprs[32];  // at 0x0
    u32 cr;        // at 0x80
    u32 lr;        // at 0x84
    u32 ctr;       // at 0x88
    u32 xer;       // at 0x8C
    f64 fprs[32];  // at 0x90
    u32 fpscr_pad; // at 0x190
    u32 fpscr;     // at 0x194
    u32 srr0;      // at 0x198
    u32 srr1;      // at 0x19C
    u16 mode;      // at 0x1A0
    u16 state;     // at 0x1A2
    u32 gqrs[8];   // at 0x1A4
    u32 psf_pad;   // at 0x1C4
    f64 psfs[32];  // at 0x1C8
} OSContext;

typedef void (*OSInterruptHandler)(s16 intr, OSContext* ctx);

void ICInvalidateRange(const void* buf, u32 len);

s32 OSDisableScheduler(void);
s32 OSEnableScheduler(void);

void OSResetSystem(u32 arg0, u32 arg1, u32 arg2);

#ifdef __cplusplus
}
#endif

#endif
