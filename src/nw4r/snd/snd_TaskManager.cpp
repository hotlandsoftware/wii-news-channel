#include <nw4r/snd/snd_TaskManager.h>

namespace nw4r {
namespace snd {
namespace detail {

u8 TaskManager::sTaskArea[TASK_AREA_SIZE];

TaskManager& TaskManager::GetInstance() {
    static TaskManager instance;
    return instance;
}

TaskManager::TaskManager() : mCurrentTask(NULL) {
    OSInitMutex(&mMutex);
    mHeapHandle = MEMCreateUnitHeapEx(sTaskArea, sizeof(sTaskArea), TASK_SIZE,
                                      4, 0);
}

void* TaskManager::Alloc() {
    ut::AutoInterruptLock lock;

    void* pTask = MEMAllocFromUnitHeap(mHeapHandle);
    while (pTask == NULL) {
        ExecuteSingle();
        pTask = MEMAllocFromUnitHeap(mHeapHandle);
    }

    return pTask;
}

void TaskManager::AppendTask(Task* pTask, TaskPriority priority) {
    ut::AutoInterruptLock lock;
    mTaskList[priority].PushBack(pTask);
}

void TaskManager::Execute() {
    while (ExecuteSingle()) {
        ;
    }
}

bool TaskManager::ExecuteSingle() {
    OSLockMutex(&mMutex);

    mCurrentTask = PopTask(PRIORITY_HIGH);
    if (mCurrentTask == NULL) {
        mCurrentTask = PopTask(PRIORITY_MIDDLE);
        if (mCurrentTask == NULL) {
            mCurrentTask = PopTask(PRIORITY_LOW);
            if (mCurrentTask == NULL) {
                OSUnlockMutex(&mMutex);
                return false;
            }
        }
    }

    mCurrentTask->Execute();
    Free(mCurrentTask);
    mCurrentTask = NULL;

    OSUnlockMutex(&mMutex);
    return true;
}

void TaskManager::CancelByTaskId(u32 taskId) {
    ut::AutoInterruptLock lock;

    for (int i = 0; i < PRIORITY_MAX; i++) {
        NW4R_UT_LINKLIST_FOREACH_SAFE (it, mTaskList[i], {
            if (taskId == it->mTaskId) {
                mTaskList[i].Erase(it);
                Free(&*it);
            }
        })
    }

    if (mCurrentTask != NULL && taskId == mCurrentTask->mTaskId) {
        mCurrentTask->Cancel();
    }
}

} // namespace detail
} // namespace snd
} // namespace nw4r
