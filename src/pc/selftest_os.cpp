// Self-test of the OS, MEM and BASE backends (`newschannel --selftest`).
//
// Nothing here allocates from the MEM1/MEM2 arenas: the game takes all of
// both in SystemInit, and `--boot` runs after the self-test.

#include <cstdlib>
#include <cstring>

#include <pc/os.h>
#include <revolution/base/PPCArch.h>
#include <revolution/mem.h>
#include <revolution/os.h>

#include "pc_selftest.h"

extern "C" OSTime OSCalendarTimeToTicks(const OSCalendarTime* cal);

namespace {

// --- MEM ----------------------------------------------------------------------

bool IsAligned(const void* p, u32 align) {
    return (reinterpret_cast<u32>(p) & (align - 1)) == 0;
}

void TestExpHeap() {
    const u32 size = 0x10000;
    u8* memory = static_cast<u8*>(std::malloc(size));

    // Option 1 clears allocations, option 4 makes the heap thread-safe.
    MEMHeapHandle heap = MEMCreateExpHeapEx(memory, size, 1 | 4);
    PC_CHECK(heap != NULL && reinterpret_cast<u8*>(heap) == memory);

    u32 total = MEMGetTotalFreeSizeForExpHeap(heap);
    PC_CHECK(total > size - 0x100 && total < size);
    PC_CHECK(MEMGetAllocatableSizeForExpHeapEx(heap, 4) == total);

    u8* a = static_cast<u8*>(MEMAllocFromExpHeapEx(heap, 100, 32));
    PC_CHECK(a != NULL);
    std::memset(a, 0xAA, 100);
    MEMFreeToExpHeap(heap, a);
    PC_CHECK(MEMGetTotalFreeSizeForExpHeap(heap) == total);
    a = static_cast<u8*>(MEMAllocFromExpHeapEx(heap, 100, 32));
    PC_CHECK(a != NULL && IsAligned(a, 32) && a >= memory && a + 100 <= memory + size);
    PC_CHECK(a[0] == 0 && a[99] == 0); // cleared (option 1)
    PC_CHECK(MEMGetTotalFreeSizeForExpHeap(heap) < total);

    // A negative alignment allocates from the end of the heap.
    u8* b = static_cast<u8*>(MEMAllocFromExpHeapEx(heap, 0x200, -32));
    PC_CHECK(b != NULL && IsAligned(b, 32) && b > a && b + 0x200 <= memory + size);
    PC_CHECK(b + 0x200 + 32 > memory + size);

    u8* c = static_cast<u8*>(MEMAllocFromExpHeapEx(heap, 1, 4));
    PC_CHECK(c != NULL && IsAligned(c, 4) && c > a && c < b);

    PC_CHECK(MEMAllocFromExpHeapEx(heap, size, 4) == NULL);

    // Freeing in any order merges the free blocks back into one.
    MEMFreeToExpHeap(heap, a);
    MEMFreeToExpHeap(heap, b);
    MEMFreeToExpHeap(heap, c);
    PC_CHECK(MEMGetTotalFreeSizeForExpHeap(heap) == total);
    PC_CHECK(MEMGetAllocatableSizeForExpHeapEx(heap, 4) == total);

    MEMAllocator allocator;
    MEMInitAllocatorForExpHeap(&allocator, heap, 32);
    void* d = MEMAllocFromAllocator(&allocator, 64);
    PC_CHECK(d != NULL && IsAligned(d, 32));
    MEMFreeToAllocator(&allocator, d);
    PC_CHECK(MEMGetTotalFreeSizeForExpHeap(heap) == total);

    PC_CHECK(MEMDestroyExpHeap(heap) == memory);
    std::free(memory);
}

void TestFrmHeap() {
    const u32 kStateId = 0x54455354u; // 'TEST'
    const u32 size = 0x4000;
    u8* memory = static_cast<u8*>(std::malloc(size));

    MEMHeapHandle heap = MEMCreateFrmHeapEx(memory, size, 0);
    PC_CHECK(heap != NULL);

    u32 total = MEMGetAllocatableSizeForFrmHeapEx(heap, 4);
    u8* a = static_cast<u8*>(MEMAllocFromFrmHeapEx(heap, 0x100, 16));
    PC_CHECK(a != NULL && IsAligned(a, 16));

    PC_CHECK(MEMRecordStateForFrmHeap(heap, kStateId));
    u8* b = static_cast<u8*>(MEMAllocFromFrmHeapEx(heap, 0x100, 4));
    u8* c = static_cast<u8*>(MEMAllocFromFrmHeapEx(heap, 0x80, -4));
    PC_CHECK(b != NULL && b > a && c == memory + size - 0x80);
    u32 used = MEMGetAllocatableSizeForFrmHeapEx(heap, 4);

    // Back to the recorded state: b and c are gone, a stays.
    PC_CHECK(MEMFreeByStateToFrmHeap(heap, kStateId));
    PC_CHECK(MEMGetAllocatableSizeForFrmHeapEx(heap, 4) > used);
    PC_CHECK(MEMAllocFromFrmHeapEx(heap, size, 4) == NULL);

    MEMFreeToFrmHeap(heap, MEM_FRM_HEAP_FREE_ALL);
    PC_CHECK(MEMGetAllocatableSizeForFrmHeapEx(heap, 4) == total);

    MEMDestroyFrmHeap(heap);
    std::free(memory);
}

void TestUnitHeap() {
    const u32 size = 0x400;
    u8* memory = static_cast<u8*>(std::malloc(size));

    MEMHeapHandle heap = MEMCreateUnitHeapEx(memory, size, 0x40, 16, 0);
    PC_CHECK(heap != NULL);

    void* blocks[16];
    int count = 0;
    void* block;
    while (count < 16 && (block = MEMAllocFromUnitHeap(heap)) != NULL) {
        PC_CHECK(IsAligned(block, 16));
        blocks[count++] = block;
    }
    PC_CHECK(count >= 8 && count < 16); // the heap is full
    PC_CHECK(blocks[1] == static_cast<u8*>(blocks[0]) + 0x40);

    MEMFreeToUnitHeap(heap, blocks[3]);
    PC_CHECK(MEMAllocFromUnitHeap(heap) == blocks[3]);

    MEMDestroyUnitHeap(heap);
    std::free(memory);
}

// --- arenas, registers --------------------------------------------------------

void TestArena() {
    u8* lo1 = static_cast<u8*>(OSGetMEM1ArenaLo());
    u8* hi1 = static_cast<u8*>(OSGetMEM1ArenaHi());
    u8* lo2 = static_cast<u8*>(OSGetMEM2ArenaLo());
    u8* hi2 = static_cast<u8*>(OSGetMEM2ArenaHi());

    // Not checked for an exact size: `--boot` may already have taken them.
    PC_CHECK(lo1 != NULL && hi1 >= lo1 && hi1 - lo1 <= 0x01800000);
    PC_CHECK(lo2 != NULL && hi2 >= lo2 && hi2 - lo2 <= 0x04000000);
    PC_CHECK(IsAligned(hi1, 32) && IsAligned(hi2, 32));

    u32 hid4 = PPCMfhid4();
    PPCMthid4(hid4 & ~0x60000000);
    PC_CHECK(PPCMfhid4() == (hid4 & ~0x60000000));
    PC_CHECK(PPCMfhid4() & 0x80000000);
    PPCMthid4(hid4);

    u8 from[64], to[64];
    std::memset(from, 0x5A, sizeof(from));
    std::memset(to, 0, sizeof(to));
    DCFlushRange(from, sizeof(from));
    LCLoadBlocks(to, from, 2);
    PC_CHECK(std::memcmp(from, to, sizeof(from)) == 0 && LCQueueLength() == 0);
}

// --- time ---------------------------------------------------------------------

void TestTime() {
    PC_CHECK(OSSecondsToTicks(1) == 60750000u);
    PC_CHECK(OSMillisecondsToTicks(1000) == 60750000u);
    PC_CHECK(PCOSTicksToNanoseconds(OSSecondsToTicks((OSTime)3)) == 3000000000LL);

    // Tick 0 is Saturday 2000-01-01 00:00:00.
    OSCalendarTime cal;
    OSTicksToCalendarTime(0, &cal);
    PC_CHECK(cal.year == 2000 && cal.mon == 0 && cal.mday == 1 && cal.hour == 0 && cal.min == 0 && cal.sec == 0);
    PC_CHECK(cal.wday == 6 && cal.yday == 0 && cal.msec == 0 && cal.usec == 0);

    // One day and 1.5 seconds later
    OSTicksToCalendarTime(OSSecondsToTicks((OSTime)86401) + OSMillisecondsToTicks((OSTime)500), &cal);
    PC_CHECK(cal.mday == 2 && cal.wday == 0 && cal.sec == 1 && cal.msec == 500 && cal.yday == 1);

    // Friday 2008-02-29 23:59:58 (a leap day) and back
    OSCalendarTime leap = {58, 59, 23, 29, 1, 2008, 0, 0, 250, 0};
    OSTime ticks = OSCalendarTimeToTicks(&leap);
    OSTicksToCalendarTime(ticks, &cal);
    PC_CHECK(cal.year == 2008 && cal.mon == 1 && cal.mday == 29 && cal.hour == 23 && cal.min == 59 && cal.sec == 58);
    PC_CHECK(cal.wday == 5 && cal.yday == 59 && cal.msec == 250);
    OSTicksToCalendarTime(ticks + OSSecondsToTicks((OSTime)2), &cal);
    PC_CHECK(cal.mon == 2 && cal.mday == 1 && cal.wday == 6 && cal.hour == 0);

    // Before 2000
    OSTicksToCalendarTime(-OSSecondsToTicks((OSTime)1), &cal);
    PC_CHECK(cal.year == 1999 && cal.mon == 11 && cal.mday == 31 && cal.hour == 23 && cal.sec == 59 && cal.wday == 5);

    // The clock: a plausible date, the tick is its low word, it advances
    // through a sleep by at least the time slept.
    OSTime before = OSGetTime();
    OSTicksToCalendarTime(before, &cal);
    PC_CHECK(cal.year >= 2024 && cal.year < 2100);
    PC_CHECK(OSDiffTick(OSGetTick(), static_cast<OSTick>(before)) >= 0);

    OSSleepTicks(OSMillisecondsToTicks((OSTime)20));
    OSTime slept = OSGetTime() - before;
    PC_CHECK(slept >= OSMillisecondsToTicks((OSTime)20) && slept < OSSecondsToTicks((OSTime)5));
}

// --- interrupts, mutexes ------------------------------------------------------

OSMutex sMutex;
int sCounter;
u8 sStack[3][0x4000];
OSThread sThread[3];

void* TryLockThread(void*) {
    return reinterpret_cast<void*>(OSTryLockMutex(&sMutex) ? 1 : 0);
}

void* CountThread(void* arg) {
    int n = reinterpret_cast<int>(arg);
    for (int i = 0; i < n; i++) {
        OSLockMutex(&sMutex);
        OSLockMutex(&sMutex);
        int value = sCounter;
        if ((i & 0xFF) == 0) {
            OSYieldThread();
        }
        sCounter = value + 1;
        OSUnlockMutex(&sMutex);
        OSUnlockMutex(&sMutex);
    }
    return arg;
}

void StartThread(int i, void* (*func)(void*), void* arg, OSPriority priority) {
    PC_CHECK(OSCreateThread(&sThread[i], func, arg, sStack[i] + sizeof(sStack[i]), sizeof(sStack[i]), priority, 0));
    PC_CHECK(OSIsThreadSuspended(&sThread[i]));
    PC_CHECK(OSResumeThread(&sThread[i]) == 1);
}

void TestMutex() {
    PC_CHECK(OSDisableInterrupts() == TRUE);
    PC_CHECK(OSDisableInterrupts() == FALSE); // already disabled
    PC_CHECK(OSRestoreInterrupts(FALSE) == FALSE);
    PC_CHECK(OSRestoreInterrupts(TRUE) == FALSE);
    PC_CHECK(OSEnableInterrupts() == TRUE);

    OSThread* self = OSGetCurrentThread();
    PC_CHECK(self != NULL && self == OSGetCurrentThread() && !OSIsThreadTerminated(self));

    // Recursion: the owner may lock again; another thread may not.
    OSInitMutex(&sMutex);
    OSLockMutex(&sMutex);
    OSLockMutex(&sMutex);
    PC_CHECK(sMutex.thread == self && sMutex.count == 2);
    PC_CHECK(OSTryLockMutex(&sMutex) && sMutex.count == 3);
    OSUnlockMutex(&sMutex);

    void* result = NULL;
    StartThread(0, TryLockThread, NULL, 16);
    PC_CHECK(OSJoinThread(&sThread[0], &result) && result == NULL); // could not lock
    PC_CHECK(OSIsThreadTerminated(&sThread[0]));

    OSUnlockMutex(&sMutex);
    PC_CHECK(sMutex.thread == self && sMutex.count == 1);
    OSUnlockMutex(&sMutex);
    PC_CHECK(sMutex.thread == NULL && sMutex.count == 0);

    // A thread that exits gives up the mutexes it holds.
    StartThread(0, TryLockThread, NULL, 16);
    PC_CHECK(OSJoinThread(&sThread[0], &result) && result == reinterpret_cast<void*>(1));
    PC_CHECK(sMutex.thread == NULL && sMutex.count == 0);

    // Mutual exclusion between three threads
    sCounter = 0;
    for (int i = 0; i < 3; i++) {
        StartThread(i, CountThread, reinterpret_cast<void*>(5000), 10 + i);
    }
    for (int i = 0; i < 3; i++) {
        PC_CHECK(OSJoinThread(&sThread[i], &result) && result == reinterpret_cast<void*>(5000));
    }
    PC_CHECK(sCounter == 15000);
}

// --- threads and message queues -----------------------------------------------

OSMessageQueue sRequests;
OSMessageQueue sReplies;
OSMessage sRequestBuffer[4];
OSMessage sReplyBuffer[2];

// Doubles every request; a request of 0 ends the thread.
void* WorkerThread(void*) {
    u32 handled = 0;
    for (;;) {
        OSMessage msg;
        OSReceiveMessage(&sRequests, &msg, OS_MESSAGE_BLOCK);
        u32 value = reinterpret_cast<u32>(msg);
        if (value == 0) {
            break;
        }
        OSSendMessage(&sReplies, reinterpret_cast<OSMessage>(value * 2), OS_MESSAGE_BLOCK);
        handled++;
    }
    return reinterpret_cast<void*>(handled);
}

void* ProducerThread(void*) {
    for (u32 i = 1; i <= 200; i++) {
        OSSendMessage(&sRequests, reinterpret_cast<OSMessage>(i), OS_MESSAGE_BLOCK);
    }
    OSSendMessage(&sRequests, NULL, OS_MESSAGE_BLOCK);
    return NULL;
}

void TestMessageQueue() {
    OSMessage msg;

    OSInitMessageQueue(&sRequests, sRequestBuffer, 4);
    OSInitMessageQueue(&sReplies, sReplyBuffer, 2);

    // Without blocking: empty and full queues say so, order is first in, first out.
    PC_CHECK(!OSReceiveMessage(&sRequests, &msg, OS_MESSAGE_NOBLOCK));
    for (u32 i = 1; i <= 4; i++) {
        PC_CHECK(OSSendMessage(&sRequests, reinterpret_cast<OSMessage>(i), OS_MESSAGE_NOBLOCK));
    }
    PC_CHECK(!OSSendMessage(&sRequests, reinterpret_cast<OSMessage>(5), OS_MESSAGE_NOBLOCK));
    PC_CHECK(OSReceiveMessage(&sRequests, &msg, OS_MESSAGE_NOBLOCK) && msg == reinterpret_cast<OSMessage>(1));
    PC_CHECK(OSJamMessage(&sRequests, reinterpret_cast<OSMessage>(9), OS_MESSAGE_NOBLOCK)); // to the front
    PC_CHECK(OSReceiveMessage(&sRequests, &msg, OS_MESSAGE_NOBLOCK) && msg == reinterpret_cast<OSMessage>(9));
    for (u32 i = 2; i <= 4; i++) {
        PC_CHECK(OSReceiveMessage(&sRequests, &msg, OS_MESSAGE_NOBLOCK) && msg == reinterpret_cast<OSMessage>(i));
    }

    // Round trip through two small queues: producer -> worker -> this thread.
    // Every side blocks on a full or an empty queue at some point.
    StartThread(0, WorkerThread, NULL, 8);
    StartThread(1, ProducerThread, NULL, 20);

    u32 sum = 0;
    bool inOrder = true;
    for (u32 i = 1; i <= 200; i++) {
        PC_CHECK(OSReceiveMessage(&sReplies, &msg, OS_MESSAGE_BLOCK));
        inOrder = inOrder && reinterpret_cast<u32>(msg) == i * 2;
        sum += reinterpret_cast<u32>(msg);
    }
    PC_CHECK(inOrder && sum == 200 * 201);

    void* handled = NULL;
    PC_CHECK(OSJoinThread(&sThread[0], &handled) && handled == reinterpret_cast<void*>(200));
    PC_CHECK(OSJoinThread(&sThread[1], NULL));
    PC_CHECK(sRequests.usedCount == 0 && sReplies.usedCount == 0);
}

// --- thread queues, suspension, alarms ----------------------------------------

OSThreadQueue sQueue;
volatile int sStage;

void* SleeperThread(void*) {
    BOOL enabled = OSDisableInterrupts();
    while (sStage < 1) {
        OSSleepThread(&sQueue); // gives up the interrupt lock while asleep
    }
    sStage = 2;
    OSRestoreInterrupts(enabled);

    OSSuspendThread(OSGetCurrentThread()); // until the test resumes it
    sStage = 3;
    return NULL;
}

void AlarmHandler(OSAlarm* alarm, OSContext*) {
    // Interrupt context: interrupts are disabled here.
    OSMessageQueue* queue = static_cast<OSMessageQueue*>(OSGetAlarmUserData(alarm));
    OSSendMessage(queue, reinterpret_cast<OSMessage>(OSDisableInterrupts() ? 0u : 1u), OS_MESSAGE_NOBLOCK);
}

void TestSleepAndAlarm() {
    OSInitThreadQueue(&sQueue);
    sStage = 0;
    StartThread(0, SleeperThread, NULL, 16);

    OSSleepTicks(OSMillisecondsToTicks((OSTime)5));
    PC_CHECK(sStage == 0);

    BOOL enabled = OSDisableInterrupts();
    sStage = 1;
    OSWakeupThread(&sQueue);
    OSRestoreInterrupts(enabled);

    for (int i = 0; i < 1000 && !(sStage == 2 && OSIsThreadSuspended(&sThread[0])); i++) {
        OSSleepTicks(OSMillisecondsToTicks((OSTime)1));
    }
    PC_CHECK(sStage == 2 && OSIsThreadSuspended(&sThread[0]));
    OSSleepTicks(OSMillisecondsToTicks((OSTime)5));
    PC_CHECK(sStage == 2); // still suspended

    PC_CHECK(OSResumeThread(&sThread[0]) == 1);
    PC_CHECK(OSJoinThread(&sThread[0], NULL) && sStage == 3);

    // A wait with a time-out on a queue nobody wakes
    OSTime before = OSGetTime();
    PC_CHECK(!PCOSSleepThreadUntil(&sQueue, before + OSMillisecondsToTicks((OSTime)10)));
    PC_CHECK(OSGetTime() - before >= OSMillisecondsToTicks((OSTime)10) && sQueue.head == NULL);

    // A one-shot alarm, a periodic alarm, a cancelled alarm
    OSMessage buffer[8];
    OSMessageQueue fired;
    OSInitMessageQueue(&fired, buffer, 8);

    OSAlarm alarm, cancelled;
    OSCreateAlarm(&alarm);
    OSSetAlarmUserData(&alarm, &fired);
    OSCreateAlarm(&cancelled);
    OSSetAlarmUserData(&cancelled, &fired);

    before = OSGetTime();
    OSSetAlarm(&cancelled, OSMillisecondsToTicks((OSTime)5), AlarmHandler);
    OSSetAlarm(&alarm, OSMillisecondsToTicks((OSTime)10), AlarmHandler);
    OSCancelAlarm(&cancelled);

    OSMessage msg;
    PC_CHECK(OSReceiveMessage(&fired, &msg, OS_MESSAGE_BLOCK) && msg == reinterpret_cast<OSMessage>(1));
    PC_CHECK(OSGetTime() - before >= OSMillisecondsToTicks((OSTime)10));
    OSSleepTicks(OSMillisecondsToTicks((OSTime)5));
    PC_CHECK(!OSReceiveMessage(&fired, &msg, OS_MESSAGE_NOBLOCK)); // only once, and not the cancelled one

    before = OSGetTime();
    OSSetPeriodicAlarm(&alarm, before + OSMillisecondsToTicks((OSTime)2), OSMillisecondsToTicks((OSTime)2), AlarmHandler);
    for (int i = 0; i < 3; i++) {
        PC_CHECK(OSReceiveMessage(&fired, &msg, OS_MESSAGE_BLOCK));
    }
    OSCancelAlarm(&alarm);
    PC_CHECK(OSGetTime() - before >= OSMillisecondsToTicks((OSTime)6));
    while (OSReceiveMessage(&fired, &msg, OS_MESSAGE_NOBLOCK)) {
    }
    OSSleepTicks(OSMillisecondsToTicks((OSTime)6));
    PC_CHECK(!OSReceiveMessage(&fired, &msg, OS_MESSAGE_NOBLOCK));
}

// --- reset and power buttons --------------------------------------------------

int sPowerCalls;
int sResetCalls;

void PowerCallback() {
    sPowerCalls++;
}

void ResetCallback() {
    sResetCalls++;
}

void TestButtons() {
    OSPowerCallback oldPower = OSSetPowerCallback(PowerCallback);
    OSResetCallback oldReset = OSSetResetCallback(ResetCallback);

    PC_CHECK(!OSGetResetButtonState());
    PC_CHECK(PCOSPressPowerButton() && sPowerCalls == 1);
    PCOSSetResetButton(TRUE);
    PCOSSetResetButton(FALSE);
    PC_CHECK(sResetCalls == 1);
    PC_CHECK(OSGetResetButtonState() && !OSGetResetButtonState()); // reported once

    PC_CHECK(OSSetPowerCallback(oldPower) == PowerCallback);
    PC_CHECK(OSSetResetCallback(oldReset) == ResetCallback);
}

} // namespace

void PCSelfTestMem() {
    TestExpHeap();
    TestFrmHeap();
    TestUnitHeap();
}

void PCSelfTestOS() {
    TestArena();
    TestTime();
    TestMutex();
    TestMessageQueue();
    TestSleepAndAlarm();
    TestButtons();
}
