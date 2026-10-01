#ifndef NW4R_LYT_LAYOUT_H
#define NW4R_LYT_LAYOUT_H

#include <types.h>
#include <revolution/mem.h>

namespace nw4r {
namespace lyt {

class Layout {
public:
    static void SetAllocator(MEMAllocator* allocator) { mspAllocator = allocator; }

    static MEMAllocator* mspAllocator;
};

} // namespace lyt
} // namespace nw4r

#endif
