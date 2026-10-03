#ifndef NEWS_THREAD_H
#define NEWS_THREAD_H

#include <types.h>
#include <revolution/os/OSThread.h>

// An OS thread with its own 16 KB stack (Thread.cpp, 0x800492A0). It is
// started on construction and joined on destruction.
class Thread {
public:
    typedef void* (*Func)(void* arg);

    enum {
        STACK_SIZE = 0x4000,
        PRIORITY = 31,
    };

    Thread(Func func);
    ~Thread();

    void Restart(Func func);

    OSThread mThread;         // at 0x000
    u8 mStack[STACK_SIZE];    // at 0x318
};

#endif
