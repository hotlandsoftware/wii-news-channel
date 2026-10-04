// Shared by the os*.cpp files of the OS backend. Not for other backends: they
// use <pc/os.h> and the SDK API.

#ifndef PC_SDK_OS_INTERNAL_H
#define PC_SDK_OS_INTERNAL_H

#include <pthread.h>
#include <time.h>

#include <pc/os.h>
#include <revolution/os.h>

// --- os_thread.cpp: the kernel lock ------------------------------------------

// Waits on `cond`, releasing the kernel lock meanwhile. The caller must have
// interrupts disabled (it holds the kernel lock).
void PCOSKernelWait(pthread_cond_t* cond);

// The same with a time-out given as an OSGetTime() value. Returns false if
// the time was reached.
bool PCOSKernelWaitUntil(pthread_cond_t* cond, OSTime time);

// --- os_time.cpp --------------------------------------------------------------

// Converts an OSGetTime() value to a CLOCK_MONOTONIC time.
void PCOSTimeToMonotonic(OSTime time, struct timespec* out);

// --- os_arena.cpp -------------------------------------------------------------

// Allocates the emulated MEM1 and MEM2 blocks and sets the arenas (once).
void PCOSInitArena();

#endif
