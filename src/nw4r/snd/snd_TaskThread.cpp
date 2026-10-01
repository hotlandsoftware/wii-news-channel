#include <nw4r/snd/snd_TaskManager.h>
#include <nw4r/snd/snd_TaskThread.h>

namespace nw4r {
namespace snd {
namespace detail {

TaskThread& TaskThread::GetInstance() {
    static TaskThread instance;
    return instance;
}

bool TaskThread::Create(s32 priority, void* pStack, u32 stackSize) {
    if (mCreateFlag) {
        return true;
    }

    mCreateFlag = true;

    OSInitMessageQueue(&mMsgQueue, mMsgBuffer, MSG_QUEUE_CAPACITY);
    OSInitThreadQueue(&mThreadQueue);

    mStackEnd = pStack;

    BOOL success = OSCreateThread(&mThread, ThreadFunc, &GetInstance(),
                                  static_cast<u8*>(pStack) + stackSize,
                                  stackSize, priority, 0);

    if (success) {
        OSResumeThread(&mThread);
    }

    return success;
}

void TaskThread::SendWakeupMessage() {
    OSSendMessage(&mMsgQueue, reinterpret_cast<OSMessage>(MSG_EXECUTE), 0);
}

void* TaskThread::ThreadFunc(void* pArg) {
    TaskThread* p = static_cast<TaskThread*>(pArg);

    while (true) {
        OSMessage msg;
        OSReceiveMessage(&p->mMsgQueue, &msg, OS_MESSAGE_BLOCK);

        if (reinterpret_cast<u32>(msg) == MSG_EXECUTE) {
            TaskManager::GetInstance().Execute();
        } else if (reinterpret_cast<u32>(msg) == MSG_DONE) {
            break;
        }
    }

    return NULL;
}

} // namespace detail
} // namespace snd
} // namespace nw4r
