#include <nw4r/math/math_arithmetic.h>

// FrSqrt as ogws math_arithmetic.cpp. The FExp/FLog tables and functions are
// not referenced in this DOL (dead-stripped), so they are left out here.

namespace nw4r {
namespace math {

f32 FrSqrt(register f32 x) {
#ifdef TARGET_PC
    return 1.0f / sqrtf(x);
#else
    register f32 rsqrt;
    register f32 c_half = 0.5f, c_three = 3.0f;
    register f32 work0, work1;

    asm {
        // Estimate reciprocal square root
        frsqrte rsqrt, x

        // Refine estimate using Newton-Raphson method
        fmuls work0, rsqrt, rsqrt
        fmuls work1, rsqrt, c_half
        fnmsubs work0, work0, x, c_three
        fmuls work1, work0, work1
    }

    return work1;
#endif
}

u32 CntBit1(u32 x) {
    x = x - ((x >> 1) & 0x55555555);
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    x = (x + (x >> 4)) & 0x0F0F0F0F;
    x = x + (x >> 8);
    x = x + (x >> 16);
    return x & 0x3F;
}

} // namespace math
} // namespace nw4r
