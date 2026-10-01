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
    ~VEC2() {}
};

struct VEC3 : public Vec {
    VEC3() {}
    VEC3(f32 fx, f32 fy, f32 fz) {
        x = fx;
        y = fy;
        z = fz;
    }
    ~VEC3() {}
};

struct MTX34 {
    union {
        struct {
            f32 _00, _01, _02, _03;
            f32 _10, _11, _12, _13;
            f32 _20, _21, _22, _23;
        };
        f32 m[3][4];
        f32 a[12];
        Mtx mtx;
    };

    MTX34() {}
    ~MTX34() {}

    operator f32*() { return a; }
    operator const f32*() const { return a; }
};

} // namespace math
} // namespace nw4r

#endif
