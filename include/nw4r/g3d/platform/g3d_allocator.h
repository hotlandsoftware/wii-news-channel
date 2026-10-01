#ifndef NW4R_G3D_PLATFORM_ALLOCATOR_H
#define NW4R_G3D_PLATFORM_ALLOCATOR_H
#include <types.h>
#include <macros.h>

#include <revolution/mem.h>

// revolution/mem/heapCommon.h defines RoundUp/RoundDown macros, which would
// break calls to ut::RoundUp/ut::RoundDown.
#undef RoundUp
#undef RoundDown

namespace nw4r {
namespace g3d {
namespace detail {

inline void* AllocFromAllocator(MEMAllocator* pAllocator, u32 size) {
    return MEMAllocFromAllocator(pAllocator, size);
}
inline void FreeToAllocator(MEMAllocator* pAllocator, void* pBlock) {
    return MEMFreeToAllocator(pAllocator, pBlock);
}

} // namespace detail
} // namespace g3d
} // namespace nw4r

#endif
