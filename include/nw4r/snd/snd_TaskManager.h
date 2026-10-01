#ifndef NW4R_SND_TASK_MANAGER_H
#define NW4R_SND_TASK_MANAGER_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_Task.h>

#include <nw4r/snd/snd_ut.h>

#include <revolution/mem.h>
#include <revolution/os.h>

namespace nw4r {
namespace snd {
namespace detail {

// This NW4R revision's TaskManager (cf. TP's nw4hbm)
class TaskManager {
public:
    enum TaskPriority {
        PRIORITY_LOW = 0,
        PRIORITY_MIDDLE = 1,
        PRIORITY_HIGH = 2,
        PRIORITY_MAX
    };

    static const int TASK_SIZE = 0x40;
    static const int TASK_AREA_SIZE = 0x2000 + 0x44;

public:
    static TaskManager& GetInstance();

    void* Alloc();
    void Free(void* pTask) {
        ut::AutoInterruptLock lock;
        MEMFreeToUnitHeap(mHeapHandle, pTask);
    }

    void AppendTask(Task* pTask, TaskPriority priority);

    void Execute();
    bool ExecuteSingle() DECOMP_DONT_INLINE;

    void CancelByTaskId(u32 taskId);

private:
    TaskManager();

    Task* PopTask(TaskPriority priority) {
        ut::AutoInterruptLock lock;

        if (mTaskList[priority].IsEmpty()) {
            return NULL;
        }

        Task& rTask = mTaskList[priority].GetFront();
        mTaskList[priority].PopFront();
        return &rTask;
    }

private:
    static u8 sTaskArea[TASK_AREA_SIZE];

    OSMutex mMutex;                   // at 0x0
    MEMHeapHandle mHeapHandle;        // at 0x18
    Task* mCurrentTask;               // at 0x1C
    TaskList mTaskList[PRIORITY_MAX]; // at 0x20
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
