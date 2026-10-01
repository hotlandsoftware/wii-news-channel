#ifndef NW4R_MATH_ARITHMETIC_H
#define NW4R_MATH_ARITHMETIC_H

#include <types.h>
#include <nw4r/math/math_constant.h>

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

// Added for g3d (Task 10), from ogws math_arithmetic.h
namespace detail {

f32 FExp(f32 x);
f32 FLog(f32 x);

} // namespace detail

f32 FrSqrt(f32 x);

inline f32 FExp(f32 x) {
    return detail::FExp(x);
}

inline f32 FInv(register f32 x) {
    register f32 work0, work1;

    asm {
        fmr  work1, x
        fres work0, work1
    }

    return work0;
}

inline f32 FSqrt(f32 x) {
    return x <= 0.0f ? 0.0f : x * FrSqrt(x);
}

inline f32 FLog(f32 x) {
    if (x > 0.0f) {
        return detail::FLog(x);
    }

    return NW4R_MATH_QNAN;
}

// Fast casts (as ogws's OSFastCast.h inlines; our os/OSFastCast.h has macros)
inline f32 U16ToF32(u16 arg) {
    register u16* pArg = &arg;
    register f32 ret;
    asm { psq_l ret, 0(pArg), 1, 3 }
    return ret;
}

inline u16 F32ToU16(register f32 arg) {
    f32 a;
    register f32* ptr = &a;
    asm { psq_st arg, 0(ptr), 1, 3 }
    return *reinterpret_cast<u16*>(ptr);
}

inline f32 S16ToF32(s16 arg) {
    register s16* pArg = &arg;
    register f32 ret;
    asm { psq_l ret, 0(pArg), 1, 5 }
    return ret;
}

inline s16 F32ToS16(register f32 arg) {
    f32 a;
    register f32* ptr = &a;
    asm { psq_st arg, 0(ptr), 1, 5 }
    return *reinterpret_cast<s16*>(ptr);
}

inline u32 F32AsU32(f32 arg) {
    return *reinterpret_cast<u32*>(&arg);
}
inline f32 U32AsF32(u32 arg) {
    return *reinterpret_cast<f32*>(&arg);
}

inline s32 FGetExpPart(f32 x) {
    s32 s = F32AsU32(x);
    return ((s >> 23) & 0xFF) - 127;
}
inline f32 FGetMantPart(f32 x) {
    u32 u = F32AsU32(x);
    return U32AsF32((u & 0x807FFFFF) | 0x3F800000);
}

} // namespace math
} // namespace nw4r

#endif
