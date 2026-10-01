#ifndef NW4R_MATH_TRIANGULAR_H
#define NW4R_MATH_TRIANGULAR_H

#include <types.h>

namespace nw4r {
namespace math {

f32 SinFIdx(f32 fidx);
f32 Atan2FIdx(f32 y, f32 x);

inline f32 SinRad(f32 rad) {
    return SinFIdx(rad * (256.0f / (2.0f * 3.1415927f)));
}

} // namespace math
} // namespace nw4r

#endif
