#ifndef NW4R_MATH_ARITHMETIC_H
#define NW4R_MATH_ARITHMETIC_H

#include <types.h>

// Minimal version for lyt (Task 15), as in tp nw4hbm math/arithmetic.h.
namespace nw4r {
namespace math {

inline f32 FSelect(register f32 cond, register f32 ifPos, register f32 ifNeg) {
    register f32 ret;
    asm { fsel ret, cond, ifPos, ifNeg }
    return ret;
}

inline f32 FAbs(register f32 x) {
    register f32 ret;
    asm { fabs ret, x }
    return ret;
}

inline f32 FNAbs(register f32 x) {
    register f32 ret;
    asm { fnabs ret, x }
    return ret;
}

} // namespace math
} // namespace nw4r

#endif
