#ifndef NW4R_UT_LOCKED_CACHE_H
#define NW4R_UT_LOCKED_CACHE_H

// From ogws include/nw4r/ut/ut_LockedCache.h (added for g3d, Task 10)
#include <types.h>
#include <revolution/os.h>

namespace nw4r {
namespace ut {
namespace LC {

void Enable();
void Disable();

bool Lock();
void Unlock();

void LoadBlocks(void* pDst, void* pSrc, u32 blocks);
void StoreBlocks(void* pDst, void* pSrc, u32 blocks);
void StoreData(void* pDst, void* pSrc, u32 size);

inline void* GetBase() {
    return reinterpret_cast<void*>(0xE0000000);
}

inline void QueueWait(u32 len) {
    LCQueueWait(len);
}

inline void QueueWaitEx(u32 len) {
    while (LCQueueLength() != len) {
        OSYieldThread();
    }
}

} // namespace LC
} // namespace ut
} // namespace nw4r

#endif
