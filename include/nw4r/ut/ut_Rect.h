#ifndef NW4R_UT_RECT_H
#define NW4R_UT_RECT_H

#include <types.h>

namespace nw4r {
namespace ut {

struct Rect {
    f32 left;   // at 0x0
    f32 top;    // at 0x4
    f32 right;  // at 0x8
    f32 bottom; // at 0xC

    Rect() {}
    ~Rect() {}
    Rect(f32 l, f32 t, f32 r, f32 b) : left(l), top(t), right(r), bottom(b) {}

    f32 GetWidth() const { return right - left; }
    f32 GetHeight() const { return bottom - top; }
};

} // namespace ut
} // namespace nw4r

#endif
