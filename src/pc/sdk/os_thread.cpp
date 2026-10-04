// OS threads, interrupts, mutexes and message queues on host threads.
//
// Design (see docs/pc_port.md, "OS backend"):
//
// - Every OSThread runs on its own host thread (pthread). The host schedules
//   them freely: thread priorities are stored but do not decide who runs.
// - "Interrupts disabled" is one process-wide lock, the kernel lock.
//   OSDisableInterrupts() takes it and OSRestoreInterrupts() releases it; the
//   state is per thread, as the MSR is on the Wii. All OS objects are only
//   changed with the lock held, so the game's OSThread, OSMutex and
//   OSMessageQueue structures are used exactly as the SDK uses them (owner,
//   count, wait queues) and need no host object of their own. They can be
//   zero-filled, copied or initialised before main() like on the Wii.
// - A thread that sleeps (OSSleepThread) waits on a condition variable, which
//   releases the kernel lock while it waits; OSWakeupThread() marks every
//   thread of the queue ready and wakes the waiters. Mutexes, message queues
//   and joining are the SDK's own algorithms on top of these two functions
//   (src/revolution/OS/OSMutex.c, OSMessage.c, OSThread.c).
// - A host thread that was not made by OSCreateThread() (the process's main
//   thread, a backend's helper thread) gets an implicit OSThread the first
//   time it asks for the current thread, so backend code works before main().
//
// Host state kept in the OSThread itself, in the saved-register area that the
// PC build does not use otherwise, the way OSInitContext() stores them:
//   context.srr0    entry function
//   context.gpr[3]  its parameter
//   context.gpr[1]  the initial stack pointer the game asked for (not used:
//                   host threads run on host stacks)
//   context.srr1    kHostNotStarted / kHostStarted

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sched.h>

#include "os_internal.h"

namespace {

const u32 kHostNotStarted = 0;
const u32 kHostStarted = 1;

// Host stacks are larger than the Wii's: x86 frames of an unoptimised build
// and host libraries (name resolution, audio) need more than the game planned.
const u32 kMinHostStack = 1024 * 1024;

const OSPriority kDefaultPriority = 16;

pthread_mutex_t sKernelLock = PTHREAD_MUTEX_INITIALIZER;
// Signalled whenever a thread may have become runnable (wake-up, resume).
pthread_cond_t sWakeCond = PTHREAD_COND_INITIALIZER;
// Never signalled: OSSleepTicks() and OSYieldThread() wait on it with a time-out.
pthread_cond_t sSleepCond = PTHREAD_COND_INITIALIZER;

thread_local bool tInterruptsDisabled;
thread_local OSThread* tCurrentThread;
// The OSThread of a host thread that OSCreateThread() did not make.
thread_local OSThread tImplicitThread;

OSThreadQueue sActiveThreadQueue;
s32 sReschedule;

// EnqueueTail, DequeueItem and DequeueHead are also used for an OSMutex.
#define EnqueueTail(queue, thread, link)                                       \
    do {                                                                       \
        auto prev_ = (queue)->tail;                                            \
        if (prev_ == NULL)                                                     \
            (queue)->head = (thread);                                          \
        else                                                                   \
            prev_->link.next = (thread);                                       \
        (thread)->link.prev = prev_;                                           \
        (thread)->link.next = NULL;                                            \
        (queue)->tail = (thread);                                              \
    } while (0)

#define EnqueuePrio(queue, thread, link)                                       \
    do {                                                                       \
        OSThread* prev_;                                                       \
        OSThread* next_;                                                       \
        for (next_ = (queue)->head; next_ && next_->priority <= (thread)->priority; next_ = next_->link.next)  \
            ;                                                                  \
        if (next_ == NULL)                                                     \
            EnqueueTail(queue, thread, link);                                  \
        else {                                                                 \
            (thread)->link.next = next_;                                       \
            prev_ = next_->link.prev;                                          \
            next_->link.prev = (thread);                                       \
            (thread)->link.prev = prev_;                                       \
            if (prev_ == NULL)                                                 \
                (queue)->head = (thread);                                      \
            else                                                               \
                prev_->link.next = (thread);                                   \
        }                                                                      \
    } while (0)

#define DequeueItem(queue, item, link)                                         \
    do {                                                                       \
        auto next_ = (item)->link.next;                                        \
        auto prev_ = (item)->link.prev;                                        \
        if (next_ == NULL)                                                     \
            (queue)->tail = prev_;                                             \
        else                                                                   \
            next_->link.prev = prev_;                                          \
        if (prev_ == NULL)                                                     \
            (queue)->head = next_;                                             \
        else                                                                   \
            prev_->link.next = next_;                                          \
    } while (0)

#define DequeueHead(queue, item, link)                                         \
    do {                                                                       \
        (item) = (queue)->head;                                                \
        auto next_ = (item)->link.next;                                        \
        if (next_ == NULL)                                                     \
            (queue)->tail = NULL;                                              \
        else                                                                   \
            next_->link.prev = NULL;                                           \
        (queue)->head = next_;                                                 \
    } while (0)

bool IsThreadActive(OSThread* thread) {
    if (thread->state == 0) {
        return false;
    }
    for (OSThread* active = sActiveThreadQueue.head; active; active = active->linkActive.next) {
        if (thread == active) {
            return true;
        }
    }
    return false;
}

// Interrupts must be disabled. Returns when the current thread is not
// suspended (any more).
void WaitWhileSuspended(OSThread* thread) {
    while (thread->suspend > 0) {
        thread->state = OS_THREAD_STATE_READY;
        PCOSKernelWait(&sWakeCond);
    }
    thread->state = OS_THREAD_STATE_RUNNING;
}

// As __OSUnlockAllMutex (OSMutex.c): a thread that exits gives up its mutexes.
void UnlockAllMutex(OSThread* thread) {
    OSMutex* mutex;
    while (thread->queueMutex.head) {
        DequeueHead(&thread->queueMutex, mutex, link);
        mutex->count = 0;
        mutex->thread = NULL;
        OSWakeupThread(&mutex->queue);
    }
}

// The end of a thread, with interrupts disabled. `thread` must not be touched
// after the kernel lock is released: a joiner may reuse it at once.
void FinishThread(OSThread* thread, void* value) {
    if (thread->attr & 1) {
        if (IsThreadActive(thread)) {
            DequeueItem(&sActiveThreadQueue, thread, linkActive);
        }
        thread->state = 0;
    } else {
        thread->state = OS_THREAD_STATE_MORIBUND;
        thread->value = value;
    }

    UnlockAllMutex(thread);
    OSWakeupThread(&thread->queueJoin);
}

void* HostThreadEntry(void* arg) {
    OSThread* thread = static_cast<OSThread*>(arg);
    tCurrentThread = thread;

    OSDisableInterrupts();
    WaitWhileSuspended(thread);
    void* (*func)(void*) = reinterpret_cast<void* (*)(void*)>(thread->context.srr0);
    void* param = reinterpret_cast<void*>(thread->context.gpr[3]);
    OSRestoreInterrupts(TRUE);

    OSExitThread(func(param));
    return NULL;
}

// Interrupts must be disabled.
void StartHostThread(OSThread* thread) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    // Joining is done with the OS's own bookkeeping (OSJoinThread).
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    u32 stackSize = static_cast<u32>(reinterpret_cast<u8*>(thread->stackBase) - reinterpret_cast<u8*>(thread->stackEnd));
    pthread_attr_setstacksize(&attr, stackSize * 4 > kMinHostStack ? stackSize * 4 : kMinHostStack);

    thread->context.srr1 = kHostStarted;
    pthread_t host;
    int error = pthread_create(&host, &attr, HostThreadEntry, thread);
    pthread_attr_destroy(&attr);
    if (error != 0) {
        OSPanic(__FILE__, __LINE__, "OSResumeThread: cannot create a host thread (%s)", std::strerror(error));
    }
}

} // namespace

// --- the kernel lock ----------------------------------------------------------

void PCOSKernelWait(pthread_cond_t* cond) {
    pthread_cond_wait(cond, &sKernelLock);
}

bool PCOSKernelWaitUntil(pthread_cond_t* cond, OSTime time) {
    struct timespec deadline;
    PCOSTimeToMonotonic(time, &deadline);
    return pthread_cond_clockwait(cond, &sKernelLock, CLOCK_MONOTONIC, &deadline) != ETIMEDOUT;
}

extern "C" {

// --- interrupts ---------------------------------------------------------------

BOOL OSDisableInterrupts(void) {
    if (tInterruptsDisabled) {
        return FALSE;
    }
    pthread_mutex_lock(&sKernelLock);
    tInterruptsDisabled = true;
    return TRUE;
}

BOOL OSRestoreInterrupts(BOOL level) {
    BOOL enabled = tInterruptsDisabled ? FALSE : TRUE;
    if (level && tInterruptsDisabled) {
        tInterruptsDisabled = false;
        pthread_mutex_unlock(&sKernelLock);
    } else if (!level && !tInterruptsDisabled) {
        pthread_mutex_lock(&sKernelLock);
        tInterruptsDisabled = true;
    }
    return enabled;
}

BOOL OSEnableInterrupts(void) {
    return OSRestoreInterrupts(TRUE);
}

// --- threads ------------------------------------------------------------------

void OSInitThreadQueue(OSThreadQueue* queue) {
    queue->head = queue->tail = NULL;
}

OSThread* OSGetCurrentThread(void) {
    OSThread* thread = tCurrentThread;
    if (thread == NULL) {
        // A host thread the OS did not create: the main thread (as the default
        // thread of __OSThreadInit) or a backend's own thread.
        thread = &tImplicitThread;
        thread->state = OS_THREAD_STATE_RUNNING;
        thread->attr = 1;
        thread->suspend = 0;
        thread->priority = thread->base = kDefaultPriority;
        thread->value = reinterpret_cast<void*>(-1);
        thread->context.srr1 = kHostStarted;
        tCurrentThread = thread;
    }
    return thread;
}

BOOL OSIsThreadSuspended(OSThread* thread) {
    return (0 < thread->suspend) ? TRUE : FALSE;
}

BOOL OSIsThreadTerminated(OSThread* thread) {
    return (thread->state == OS_THREAD_STATE_MORIBUND || thread->state == 0) ? TRUE : FALSE;
}

// The host scheduler cannot be stopped; the count is kept for its return value.
s32 OSDisableScheduler(void) {
    BOOL enabled = OSDisableInterrupts();
    s32 count = sReschedule++;
    OSRestoreInterrupts(enabled);
    return count;
}

s32 OSEnableScheduler(void) {
    BOOL enabled = OSDisableInterrupts();
    s32 count = sReschedule--;
    OSRestoreInterrupts(enabled);
    return count;
}

void OSYieldThread(void) {
    BOOL enabled = OSDisableInterrupts();
    OSThread* current = OSGetCurrentThread();
    WaitWhileSuspended(current);

    // Another thread runs with its own interrupt state, so the lock is given
    // up even if the caller has interrupts disabled.
    tInterruptsDisabled = false;
    pthread_mutex_unlock(&sKernelLock);
    sched_yield();
    pthread_mutex_lock(&sKernelLock);
    tInterruptsDisabled = true;

    OSRestoreInterrupts(enabled);
}

BOOL OSCreateThread(OSThread* thread, void* (*func)(void*), void* param, void* stack, u32 stackSize, OSPriority priority,
                    u16 attr) {
    if (priority < 0 || 31 < priority) {
        return FALSE;
    }

    std::memset(&thread->context, 0, sizeof(thread->context));
    thread->state = OS_THREAD_STATE_READY;
    thread->attr = static_cast<u16>(attr & 1);
    thread->priority = thread->base = priority;
    thread->suspend = 1;
    thread->value = reinterpret_cast<void*>(-1);
    thread->queue = NULL;
    thread->link.next = thread->link.prev = NULL;
    thread->mutex = NULL;
    OSInitThreadQueue(&thread->queueJoin);
    thread->queueMutex.head = thread->queueMutex.tail = NULL;

    thread->context.srr0 = reinterpret_cast<u32>(func);
    thread->context.gpr[3] = reinterpret_cast<u32>(param);
    thread->context.gpr[1] = (reinterpret_cast<u32>(stack) & ~7u) - 8;
    thread->context.srr1 = kHostNotStarted;

    thread->stackBase = static_cast<u8*>(stack);
    thread->stackEnd = reinterpret_cast<u32*>(static_cast<u8*>(stack) - stackSize);
    *thread->stackEnd = 0xDEADBABE;
    thread->error = 0;
    thread->specific[0] = thread->specific[1] = NULL;

    BOOL enabled = OSDisableInterrupts();
    EnqueueTail(&sActiveThreadQueue, thread, linkActive);
    OSRestoreInterrupts(enabled);
    return TRUE;
}

void OSExitThread(void* value) {
    OSDisableInterrupts();
    FinishThread(OSGetCurrentThread(), value);
    tCurrentThread = NULL;
    OSRestoreInterrupts(TRUE);
    pthread_exit(NULL);
}

BOOL OSJoinThread(OSThread* thread, void** value) {
    BOOL enabled = OSDisableInterrupts();

    if (!(thread->attr & 1) && thread->state != OS_THREAD_STATE_MORIBUND && thread->queueJoin.head == NULL) {
        OSSleepThread(&thread->queueJoin);
        if (!IsThreadActive(thread)) {
            OSRestoreInterrupts(enabled);
            return FALSE;
        }
    }

    if (thread->state == OS_THREAD_STATE_MORIBUND) {
        if (value) {
            *value = thread->value;
        }
        DequeueItem(&sActiveThreadQueue, thread, linkActive);
        thread->state = 0;
        OSRestoreInterrupts(enabled);
        return TRUE;
    }

    OSRestoreInterrupts(enabled);
    return FALSE;
}

void OSDetachThread(OSThread* thread) {
    BOOL enabled = OSDisableInterrupts();

    thread->attr |= 1;
    if (thread->state == OS_THREAD_STATE_MORIBUND) {
        DequeueItem(&sActiveThreadQueue, thread, linkActive);
        thread->state = 0;
    }

    OSWakeupThread(&thread->queueJoin);
    OSRestoreInterrupts(enabled);
}

s32 OSResumeThread(OSThread* thread) {
    BOOL enabled = OSDisableInterrupts();
    s32 suspendCount = thread->suspend--;

    if (thread->suspend < 0) {
        thread->suspend = 0;
    } else if (thread->suspend == 0) {
        if (thread->context.srr1 == kHostNotStarted && thread->state == OS_THREAD_STATE_READY) {
            StartHostThread(thread);
        }
        pthread_cond_broadcast(&sWakeCond);
    }

    OSRestoreInterrupts(enabled);
    return suspendCount;
}

// A host thread cannot be frozen from outside. Suspending the current thread
// blocks at once; another thread stops at its next scheduling point (when it
// would sleep, wake up, yield, or finish a sleep).
s32 OSSuspendThread(OSThread* thread) {
    BOOL enabled = OSDisableInterrupts();
    s32 suspendCount = thread->suspend++;

    if (suspendCount == 0 && thread == OSGetCurrentThread()) {
        WaitWhileSuspended(thread);
    }

    OSRestoreInterrupts(enabled);
    return suspendCount;
}

void OSSleepThread(OSThreadQueue* queue) {
    BOOL enabled = OSDisableInterrupts();
    OSThread* current = OSGetCurrentThread();

    current->state = OS_THREAD_STATE_WAITING;
    current->queue = queue;
    EnqueuePrio(queue, current, link);

    do {
        PCOSKernelWait(&sWakeCond);
    } while (current->state == OS_THREAD_STATE_WAITING);

    current->queue = NULL;
    WaitWhileSuspended(current);
    OSRestoreInterrupts(enabled);
}

BOOL PCOSSleepThreadUntil(OSThreadQueue* queue, OSTime time) {
    BOOL woken = TRUE;
    BOOL enabled = OSDisableInterrupts();
    OSThread* current = OSGetCurrentThread();

    current->state = OS_THREAD_STATE_WAITING;
    current->queue = queue;
    EnqueuePrio(queue, current, link);

    while (current->state == OS_THREAD_STATE_WAITING) {
        if (!PCOSKernelWaitUntil(&sWakeCond, time) && current->state == OS_THREAD_STATE_WAITING) {
            DequeueItem(queue, current, link);
            woken = FALSE;
            break;
        }
    }

    current->queue = NULL;
    WaitWhileSuspended(current);
    OSRestoreInterrupts(enabled);
    return woken;
}

void OSWakeupThread(OSThreadQueue* queue) {
    BOOL enabled = OSDisableInterrupts();
    OSThread* thread;

    if (queue->head) {
        while (queue->head) {
            DequeueHead(queue, thread, link);
            thread->state = OS_THREAD_STATE_READY;
        }
        pthread_cond_broadcast(&sWakeCond);
    }

    OSRestoreInterrupts(enabled);
}

// Priorities are recorded (and order the wait queues) but the host decides
// which thread runs.
BOOL OSSetThreadPriority(OSThread* thread, OSPriority priority) {
    if (priority < 0 || priority > 31) {
        return FALSE;
    }

    BOOL enabled = OSDisableInterrupts();
    thread->base = priority;
    thread->priority = priority;
    OSRestoreInterrupts(enabled);
    return TRUE;
}

OSPriority OSGetThreadPriority(OSThread* thread) {
    return thread->base;
}

void OSSleepTicks(OSTime ticks) {
    BOOL enabled = OSDisableInterrupts();
    OSThread* current = OSGetCurrentThread();

    if (ticks > 0) {
        OSTime end = OSGetTime() + ticks;
        while (PCOSKernelWaitUntil(&sSleepCond, end)) {
            // spurious wake-up: wait for the rest
        }
    }

    WaitWhileSuspended(current);
    OSRestoreInterrupts(enabled);
}

// --- mutexes (as in src/revolution/OS/OSMutex.c, without priority inheritance) -

void OSInitMutex(OSMutex* mutex) {
    OSInitThreadQueue(&mutex->queue);
    mutex->thread = NULL;
    mutex->count = 0;
}

void OSLockMutex(OSMutex* mutex) {
    BOOL enabled = OSDisableInterrupts();
    OSThread* currentThread = OSGetCurrentThread();

    for (;;) {
        OSThread* ownerThread = mutex->thread;

        if (ownerThread == NULL) {
            mutex->thread = currentThread;
            mutex->count++;
            EnqueueTail(&currentThread->queueMutex, mutex, link);
            break;
        } else if (ownerThread == currentThread) {
            mutex->count++;
            break;
        } else {
            currentThread->mutex = mutex;
            OSSleepThread(&mutex->queue);
            currentThread->mutex = NULL;
        }
    }

    OSRestoreInterrupts(enabled);
}

void OSUnlockMutex(OSMutex* mutex) {
    BOOL enabled = OSDisableInterrupts();
    OSThread* currentThread = OSGetCurrentThread();

    if (mutex->thread == currentThread && --mutex->count == 0) {
        DequeueItem(&currentThread->queueMutex, mutex, link);
        mutex->thread = NULL;
        OSWakeupThread(&mutex->queue);
    }

    OSRestoreInterrupts(enabled);
}

BOOL OSTryLockMutex(OSMutex* mutex) {
    BOOL locked;
    BOOL enabled = OSDisableInterrupts();
    OSThread* currentThread = OSGetCurrentThread();

    if (mutex->thread == NULL) {
        mutex->thread = currentThread;
        mutex->count++;
        EnqueueTail(&currentThread->queueMutex, mutex, link);
        locked = TRUE;
    } else if (mutex->thread == currentThread) {
        mutex->count++;
        locked = TRUE;
    } else {
        locked = FALSE;
    }

    OSRestoreInterrupts(enabled);
    return locked;
}

// --- message queues (as in src/revolution/OS/OSMessage.c) ---------------------

void OSInitMessageQueue(OSMessageQueue* mq, OSMessage* msgArray, s32 msgCount) {
    OSInitThreadQueue(&mq->queueSend);
    OSInitThreadQueue(&mq->queueReceive);
    mq->msgArray = msgArray;
    mq->msgCount = msgCount;
    mq->firstIndex = 0;
    mq->usedCount = 0;
}

BOOL OSSendMessage(OSMessageQueue* mq, OSMessage msg, s32 flags) {
    BOOL enabled = OSDisableInterrupts();

    while (mq->msgCount <= mq->usedCount) {
        if (!(flags & OS_MESSAGE_BLOCK)) {
            OSRestoreInterrupts(enabled);
            return FALSE;
        }
        OSSleepThread(&mq->queueSend);
    }

    s32 lastIndex = (mq->firstIndex + mq->usedCount) % mq->msgCount;
    mq->msgArray[lastIndex] = msg;
    mq->usedCount++;

    OSWakeupThread(&mq->queueReceive);
    OSRestoreInterrupts(enabled);
    return TRUE;
}

BOOL OSReceiveMessage(OSMessageQueue* mq, OSMessage* msg, s32 flags) {
    BOOL enabled = OSDisableInterrupts();

    while (mq->usedCount == 0) {
        if (!(flags & OS_MESSAGE_BLOCK)) {
            OSRestoreInterrupts(enabled);
            return FALSE;
        }
        OSSleepThread(&mq->queueReceive);
    }

    if (msg != NULL) {
        *msg = mq->msgArray[mq->firstIndex];
    }

    mq->firstIndex = (mq->firstIndex + 1) % mq->msgCount;
    mq->usedCount--;

    OSWakeupThread(&mq->queueSend);
    OSRestoreInterrupts(enabled);
    return TRUE;
}

BOOL OSJamMessage(OSMessageQueue* mq, OSMessage msg, s32 flags) {
    BOOL enabled = OSDisableInterrupts();

    while (mq->msgCount <= mq->usedCount) {
        if (!(flags & OS_MESSAGE_BLOCK)) {
            OSRestoreInterrupts(enabled);
            return FALSE;
        }
        OSSleepThread(&mq->queueSend);
    }

    mq->firstIndex = (mq->firstIndex + mq->msgCount - 1) % mq->msgCount;
    mq->msgArray[mq->firstIndex] = msg;
    mq->usedCount++;

    OSWakeupThread(&mq->queueReceive);
    OSRestoreInterrupts(enabled);
    return TRUE;
}

} // extern "C"
