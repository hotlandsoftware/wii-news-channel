#ifndef NW4R_MATH_TYPES_G3D_H
#define NW4R_MATH_TYPES_G3D_H

// Additions to math_types.h for nw4r::g3d (Task 10), from ogws
// include/nw4r/math/math_types.h. Included at the end of math_types.h.
//
// Our MTX34 does not derive from ogws's POD base _MTX34, so that is a
// separate POD type here (same layout); g3d code converts with casts.

#include <types.h>
#include <macros.h>
#include <revolution/mtx.h>
#include <nw4r/math/math_arithmetic.h>
#include <nw4r/math/math_triangular.h>

#ifndef ASM
#define ASM asm
#endif

namespace nw4r {
namespace math {

// VEC2/VEC3 derive from the C structs, which play the role of ogws's POD
// bases _VEC2/_VEC3.
typedef Vec2 _VEC2;
typedef Vec _VEC3;

struct _MTX34 {
    union {
        struct {
            f32 _00, _01, _02, _03;
            f32 _10, _11, _12, _13;
            f32 _20, _21, _22, _23;
        };

        f32 m[3][4];
        f32 a[3 * 4];
        Mtx mtx;
    };
};

/******************************************************************************
 *
 * MTX33 structure
 *
 ******************************************************************************/
struct _MTX33 {
    union {
        struct {
            f32 _00, _01, _02;
            f32 _10, _11, _12;
            f32 _20, _21, _22;
        };

        f32 m[3][3];
        f32 a[3 * 3];
    };
};

struct MTX33 : _MTX33 {
    MTX33() {}

    // clang-format off
    MTX33(f32 f00, f32 f01, f32 f02,
          f32 f10, f32 f11, f32 f12,
          f32 f20, f32 f21, f32 f22) {
        _00 = f00; _01 = f01; _02 = f02;
        _10 = f10; _11 = f11; _12 = f12;
        _20 = f20; _21 = f21; _22 = f22;
    }
    // clang-format on
};

/******************************************************************************
 *
 * QUAT structure
 *
 ******************************************************************************/
struct _QUAT {
    f32 x, y, z, w;
};

struct QUAT : _QUAT {
    QUAT() {}
    QUAT(f32 fx, f32 fy, f32 fz, f32 fw) {
        x = fx;
        y = fy;
        z = fz;
        w = fw;
    }

    operator Quaternion*() {
        return reinterpret_cast<Quaternion*>(this);
    }
    operator const Quaternion*() const {
        return reinterpret_cast<const Quaternion*>(this);
    }
};

/******************************************************************************
 *
 * VEC3 functions
 *
 ******************************************************************************/
VEC3* VEC3Maximize(VEC3* pOut, const VEC3* pA, const VEC3* pB);
VEC3* VEC3Minimize(VEC3* pOut, const VEC3* pA, const VEC3* pB);
VEC3* VEC3TransformNormal(VEC3* pOut, const MTX34* pMtx, const VEC3* pVec);

inline f32 VEC3Dot(register const VEC3* pA, register const VEC3* pB) {
    register f32 dot;
    register f32 work0, work1, work2, work3;

    ASM {
        psq_l  work0, VEC3.y(pA), 0, 0
        psq_l  work1, VEC3.y(pB), 0, 0
        ps_mul work0, work0, work1

        psq_l   work3, VEC3.x(pA), 1, 0
        psq_l   work2, VEC3.x(pB), 1, 0
        ps_madd work1, work3, work2, work0

        ps_sum0 dot, work1, work0, work0
    }

    return dot;
}

inline f32 VEC3LenSq(register const VEC3* pVec) {
    register f32 work0, work1, work2;

    ASM {
        psq_l  work0, VEC3.x(pVec), 0, 0
        ps_mul work0, work0, work0

        lfs     work1, VEC3.z(pVec)
        ps_madd work2, work1, work1, work0

        ps_sum0 work2, work2, work0, work0
    }

    return work2;
}

inline VEC3* VEC3Scale(register VEC3* pOut, register const VEC3* pIn,
                       register f32 scale) {
    register f32 work0, work1;

    ASM {
        psq_l    work0, VEC3.x(pIn),  0, 0
        ps_muls0 work1, work0, scale
        psq_st   work1, VEC3.x(pOut), 0, 0

        psq_l    work0, VEC3.z(pIn),  1, 0
        ps_muls0 work1, work0, scale
        psq_st   work1, VEC3.z(pOut), 1, 0
    }

    return pOut;
}

inline VEC3* VEC3Sub(register VEC3* pOut, register const VEC3* pA,
                     register const VEC3* pB) {
    register f32 work0, work1, work2;

    ASM {
        psq_l  work0, VEC3.x(pA),   0, 0
        psq_l  work1, VEC3.x(pB),   0, 0
        ps_sub work2, work0, work1
        psq_st work2, VEC3.x(pOut), 0, 0

        psq_l  work0, VEC3.z(pA),   1, 0
        psq_l  work1, VEC3.z(pB),   1, 0
        ps_sub work2, work0, work1
        psq_st work2, VEC3.z(pOut), 1, 0
    }

    return pOut;
}

// VEC3 operators added for g3d (Task 10), as in ogws math_types.h
inline VEC3 VEC3::operator-(const VEC3& rhs) const {
    VEC3 out;
    VEC3Sub(&out, this, &rhs);
    return out;
}
inline VEC3 VEC3::operator*(f32 s) const {
    VEC3 out;
    VEC3Scale(&out, this, s);
    return out;
}
inline VEC3 VEC3::operator/(f32 s) const {
    f32 r = 1 / s;
    return *this * r;
}
inline VEC3& VEC3::operator-=(const VEC3& rhs) {
    VEC3Sub(this, this, &rhs);
    return *this;
}
inline VEC3& VEC3::operator*=(f32 s) {
    VEC3Scale(this, this, s);
    return *this;
}
inline VEC3& VEC3::operator/=(f32 s) {
    return *this *= (1 / s);
}

inline VEC3* VEC3Cross(VEC3* pOut, const VEC3* pA, const VEC3* pB) {
    PSVECCrossProduct(*pA, *pB, *pOut);
    return pOut;
}

inline f32 VEC3DistSq(const VEC3* pA, const VEC3* pB) {
    return PSVECSquareDistance(*pA, *pB);
}

inline f32 VEC3Len(const VEC3* pVec) {
    return PSVECMag(*pVec);
}

inline VEC3* VEC3Normalize(VEC3* pOut, const VEC3* pIn) {
    PSVECNormalize(*pIn, *pOut);
    return pOut;
}

inline VEC3* VEC3Transform(VEC3* pOut, const MTX34* pMtx, const VEC3* pVec) {
    PSMTXMultVec(*pMtx, *pVec, *pOut);
    return pOut;
}

inline VEC3* VEC3TransformCoord(VEC3* pOut, const MTX34* pMtx,
                                const VEC3* pVec) {
    PSMTXMultVec(*pMtx, *pVec, *pOut);
    return pOut;
}

/******************************************************************************
 *
 * MTX33 functions
 *
 ******************************************************************************/
MTX33* MTX33Identity(MTX33* pMtx);

/******************************************************************************
 *
 * MTX34 functions
 *
 ******************************************************************************/
MTX33* MTX34ToMTX33(MTX33* pOut, const MTX34* pIn);
u32 MTX34InvTranspose(MTX33* pOut, const MTX34* pIn);
MTX34* MTX34Zero(MTX34* pMtx);
MTX34* MTX34Scale(MTX34* pOut, const MTX34* pIn, const VEC3* pScale);
MTX34* MTX34Trans(MTX34* pOut, const MTX34* pIn, const VEC3* pTrans);
MTX34* MTX34RotAxisFIdx(MTX34* pMtx, const VEC3* pAxis, f32 fidx);

inline u32 MTX34Inv(MTX34* pOut, const MTX34* pIn) {
    return PSMTXInverse(*pIn, *pOut);
}

inline u32 MTX34InvTranspose(MTX34* pOut, const MTX34* pIn) {
    return PSMTXInvXpose(*pIn, *pOut);
}

inline MTX34* MTX34LookAt(MTX34* pMtx, const VEC3* pPos, const VEC3* pUp,
                          const VEC3* pTarget) {
    C_MTXLookAt(*pMtx, *pPos, *pUp, *pTarget);
    return pMtx;
}

inline MTX34* MTX34MultArray(MTX34* pOut, const MTX34* p1, const MTX34* pSrc,
                             u32 len) {
    PSMTXConcatArray(*p1, reinterpret_cast<const Mtx*>(pSrc),
                     reinterpret_cast<Mtx*>(pOut), len);
    return pOut;
}

inline MTX34* MTX34RotAxisRad(MTX34* pOut, const VEC3* pAxis, f32 frad) {
    return MTX34RotAxisFIdx(pOut, pAxis, NW4R_MATH_RAD_TO_FIDX(frad));
}

inline MTX34* MTX34RotXYZRad(MTX34* pMtx, f32 rx, f32 ry, f32 rz) {
    return MTX34RotXYZFIdx(pMtx, NW4R_MATH_RAD_TO_FIDX(rx),
                           NW4R_MATH_RAD_TO_FIDX(ry),
                           NW4R_MATH_RAD_TO_FIDX(rz));
}

inline MTX34* MTX34Scale(MTX34* pOut, const VEC3* pScale, const MTX34* pIn) {
    PSMTXScaleApply(*pIn, *pOut, pScale->x, pScale->y, pScale->z);
    return pOut;
}

inline QUAT* MTX34ToQUAT(QUAT* pQuat, const MTX34* pMtx) {
    C_QUATMtx(*pQuat, *pMtx);
    return pQuat;
}

inline MTX34* MTX34Trans(MTX34* pOut, const VEC3* pTrans, const MTX34* pIn) {
    PSMTXTransApply(*pIn, *pOut, pTrans->x, pTrans->y, pTrans->z);
    return pOut;
}

/******************************************************************************
 *
 * MTX44 functions
 *
 ******************************************************************************/
MTX44* MTX44Copy(MTX44* pDst, const MTX44* pSrc);

/******************************************************************************
 *
 * QUAT functions
 *
 ******************************************************************************/
inline MTX34* QUATToMTX34(MTX34* pMtx, const QUAT* pQuat) {
    PSMTXQuat(*pMtx, *pQuat);
    return pMtx;
}

inline QUAT* C_QUATSlerp(QUAT* pOut, const QUAT* p1, const QUAT* p2, f32 t) {
    ::C_QUATSlerp(*p1, *p2, *pOut, t);
    return pOut;
}

} // namespace math
} // namespace nw4r

#endif
