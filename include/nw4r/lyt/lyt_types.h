#ifndef NW4R_LYT_TYPES_H
#define NW4R_LYT_TYPES_H

#include <types.h>

namespace nw4r {
namespace lyt {

struct Size {
    f32 width;  // at 0x0
    f32 height; // at 0x4

    Size() : width(0.0f), height(0.0f) {}
    Size(f32 w, f32 h) : width(w), height(h) {}
};

} // namespace lyt
} // namespace nw4r

#endif
