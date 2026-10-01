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

    // The NW4R libraries (lyt) zero-initialise rectangles;
    // the game code's headers have an empty constructor.
#ifdef NW4R_UT_RECT_DEFAULT_ZERO
    Rect() : left(0.0f), top(0.0f), right(0.0f), bottom(0.0f) {}
#else
    Rect() {}
#endif
    ~Rect() {}
    Rect(f32 l, f32 t, f32 r, f32 b) : left(l), top(t), right(r), bottom(b) {}

    f32 GetWidth() const { return right - left; }
    f32 GetHeight() const { return bottom - top; }

    // Added for lyt (Task 15)
    void SetWidth(f32 width) { right = left + width; }
    void SetHeight(f32 height) { bottom = top + height; }

    void MoveTo(f32 x, f32 y) {
        right = x + GetWidth();
        left = x;

        bottom = y + GetHeight();
        top = y;
    }
};

} // namespace ut
} // namespace nw4r

#endif
