#include <news/Thread.h>

Thread::Thread(Func func) {
    OSCreateThread(&mThread, func, NULL, mStack + STACK_SIZE, STACK_SIZE, PRIORITY, 0);
    OSResumeThread(&mThread);
}

Thread::~Thread() {
    OSJoinThread(&mThread, NULL);
}

void Thread::Restart(Func func) {
    OSJoinThread(&mThread, NULL);
    OSCreateThread(&mThread, func, NULL, mStack + STACK_SIZE, STACK_SIZE, PRIORITY, 0);
    OSResumeThread(&mThread);
}
