#ifndef NW4R_MATH_TRIANGULAR_H
#define NW4R_MATH_TRIANGULAR_H

#include <types.h>

namespace nw4r {
namespace math {

f32 Atan2FIdx(f32 y, f32 x);

f32 CosFIdx(f32 fidx);

inline f32 Atan2Rad(f32 y, f32 x) {
    return Atan2FIdx(y, x) * (3.1415927f / 128.0f);
}

inline f32 CosRad(f32 rad) {
    return CosFIdx(rad * (128.0f / 3.1415927f));
}

struct MTX34;

MTX34* MTX34RotXYZFIdx(MTX34* out, f32 fx, f32 fy, f32 fz);

inline MTX34* MTX34RotXYZDeg(MTX34* out, f32 dx, f32 dy, f32 dz) {
    return MTX34RotXYZFIdx(out, dx * (256.0f / 360.0f), dy * (256.0f / 360.0f),
                           dz * (256.0f / 360.0f));
}

} // namespace math
} // namespace nw4r

#endif
