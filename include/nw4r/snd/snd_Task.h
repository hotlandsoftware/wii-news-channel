#ifndef NW4R_SND_TASK_H
#define NW4R_SND_TASK_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_ut.h>

namespace nw4r {
namespace snd {
namespace detail {

// This NW4R revision's Task (cf. TP's nw4hbm): tasks carry an ID and are
// allocated from TaskManager's unit heap
class Task : private ut::NonCopyable {
    friend class TaskManager;

public:
    Task() : mTaskId(0) {}
    explicit Task(u32 taskId) : mTaskId(taskId) {}

    virtual ~Task() {}          // at 0x8
    virtual void Execute() = 0; // at 0xC
    virtual void Cancel() = 0;  // at 0x10

    u32 GetTaskId() const {
        return mTaskId;
    }

public:
    NW4R_UT_LINKLIST_NODE_DECL(); // at 0x4

private:
    u32 mTaskId; // at 0xC
};

NW4R_UT_LINKLIST_TYPEDEF_DECL(Task);

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
