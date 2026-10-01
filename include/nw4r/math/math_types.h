#ifndef NW4R_MATH_TYPES_H
#define NW4R_MATH_TYPES_H

#include <types.h>
#include <revolution/mtx.h>

namespace nw4r {
namespace math {

struct VEC2 : public Vec2 {
    VEC2() {}
    VEC2(f32 fx, f32 fy) {
        x = fx;
        y = fy;
    }
};

struct VEC3 : public Vec {
    VEC3() {}
    VEC3(f32 fx, f32 fy, f32 fz) {
        x = fx;
        y = fy;
        z = fz;
    }
};

} // namespace math
} // namespace nw4r

#endif
