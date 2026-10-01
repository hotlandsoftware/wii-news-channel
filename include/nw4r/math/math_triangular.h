#ifndef NW4R_MATH_TRIANGULAR_H
#define NW4R_MATH_TRIANGULAR_H

#include <types.h>
#include <nw4r/math/math_constant.h>

// Added for g3d (Task 10), from ogws math_triangular.h
#ifndef NW4R_MATH_IDX_TO_FIDX
#define NW4R_MATH_IDX_TO_FIDX(x) ((x) * (1.0f / 256.0f))
#define NW4R_MATH_DEG_TO_FIDX(x) ((x) * (256.0f / 360.0f))
#define NW4R_MATH_FIDX_TO_DEG(x) ((x) * (360.0f / 256.0f))
#define NW4R_MATH_RAD_TO_FIDX(x) ((x) * (128.0f / NW4R_MATH_PI))
#define NW4R_MATH_FIDX_TO_RAD(x) ((x) * (NW4R_MATH_PI / 128.0f))
#define NW4R_MATH_DEG_TO_RAD(x) ((x) * (NW4R_MATH_PI / 180.0f))
#define NW4R_MATH_RAD_TO_DEG(x) ((x) * (180.0f / NW4R_MATH_PI))
#endif

namespace nw4r {
namespace math {

f32 SinFIdx(f32 fidx);
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

// Added for lyt (Task 15)
inline f32 SinDeg(f32 deg) {
    return SinFIdx(deg * (256.0f / 360.0f));
}

inline f32 CosDeg(f32 deg) {
    return CosFIdx(deg * (256.0f / 360.0f));
}

inline f32 SinRad(f32 rad) {
    return SinFIdx(rad * (256.0f / (2.0f * 3.1415927f)));
}

// Added for g3d (Task 10), from ogws math_triangular.h
void SinCosFIdx(f32* pSin, f32* pCos, f32 fidx);

inline void SinCosDeg(f32* pSin, f32* pCos, f32 deg) {
    return SinCosFIdx(pSin, pCos, NW4R_MATH_DEG_TO_FIDX(deg));
}
inline void SinCosRad(f32* pSin, f32* pCos, f32 rad) {
    return SinCosFIdx(pSin, pCos, NW4R_MATH_RAD_TO_FIDX(rad));
}

f32 AtanFIdx(f32 x);

inline f32 Atan2Deg(f32 y, f32 x) {
    return NW4R_MATH_FIDX_TO_DEG(Atan2FIdx(y, x));
}

} // namespace math
} // namespace nw4r

#endif
