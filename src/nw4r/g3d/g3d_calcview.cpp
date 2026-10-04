#include <nw4r/g3d.h>

#include <algorithm>

namespace nw4r {
namespace g3d {

/******************************************************************************
 *
 * Paired-single math implementation
 *
 ******************************************************************************/
namespace {

// Allows struct offsets inside assembly
using nw4r::math::MTX34;
using nw4r::math::VEC3;

#ifdef TARGET_PC
// C versions of the paired-single routines in the #else branch. They follow
// the instructions one by one: ps_madd, ps_msub and fmadds are fused (one
// rounding), so they are fmaf() here; the fres + Newton step is the exact
// reciprocal (docs/pc_port.md, R4).

// The part GetModelLocalAxisY2 and GetModelLocalAxisY3 share: the cofactors of
// the parent's rotation that make up column 1 of its inverse, and the
// determinant.
struct PCLocalAxisWork {
    f32 f9_0, f9_1;     // P01*P20 - P00*P21, P02 - P22 (slot 1: no cofactor)
    f32 f11_0, f11_1;   // P21*P02 - P01*P22, P22*P00 - P02*P20
    f32 zero_0, zero_1; // ps_sub f6, f6, f6
    f32 det;
};

inline void PCLocalAxisSetup(PCLocalAxisWork* pWork, const math::MTX34* pP) {
    const f32 f12_0 = __builtin_fmaf(pP->_11, pP->_22, -(pP->_21 * pP->_12));
    const f32 f10_0 = __builtin_fmaf(pP->_01, pP->_12, -(pP->_11 * pP->_02));

    pWork->f11_0 = __builtin_fmaf(pP->_21, pP->_02, -(pP->_01 * pP->_22));
    pWork->f11_1 = __builtin_fmaf(pP->_22, pP->_00, -(pP->_02 * pP->_20));

    pWork->f9_0 = __builtin_fmaf(pP->_01, pP->_20, -(pP->_00 * pP->_21));
    // Slot 1 of f0 and f4 is the 1.0f that a single-element psq_l loads
    pWork->f9_1 = pP->_02 - pP->_22;

    pWork->zero_0 = pP->_02 - pP->_02;
    pWork->zero_1 = pP->_00 - pP->_00;

    f32 det = pP->_00 * f12_0;
    det = __builtin_fmaf(pP->_10, pWork->f11_0, det);
    det = __builtin_fmaf(pP->_20, f10_0, det);
    pWork->det = det;
}

void GetModelLocalAxisY2(math::VEC3* pVec, const math::MTX34* pModelMtx,
                         const math::MTX34* pParentModelMtx) {
    PCLocalAxisWork w;
    PCLocalAxisSetup(&w, pParentModelMtx);

    pVec->z = w.zero_0;

    // bne after ps_cmpo0: the branch is also taken when the operands are
    // unordered
    if (!(w.det != w.zero_0)) {
        pVec->x = w.zero_0;
        pVec->y = w.zero_1;
        return;
    }

    const f32 inv = 1.0f / w.det;
    const f32 f11_0 = w.f11_0 * inv;
    const f32 f11_1 = w.f11_1 * inv;
    const f32 f9_0 = w.f9_0 * inv;

    f32 x = pModelMtx->_00 * f11_0;
    f32 y = pModelMtx->_10 * f11_0;
    x = __builtin_fmaf(pModelMtx->_01, f11_1, x);
    y = __builtin_fmaf(pModelMtx->_11, f11_1, y);
    x = __builtin_fmaf(pModelMtx->_02, f9_0, x);
    y = __builtin_fmaf(pModelMtx->_12, f9_0, y);

    pVec->x = x;
    pVec->y = y;
}

void GetModelLocalAxisY3(math::VEC3* pVec, const math::MTX34* pModelMtx,
                         const math::MTX34* pParentModelMtx) {
    PCLocalAxisWork w;
    PCLocalAxisSetup(&w, pParentModelMtx);

    if (!(w.det != w.zero_0)) {
        pVec->x = w.zero_0;
        pVec->y = w.zero_1;
        pVec->z = w.zero_0;
        return;
    }

    const f32 inv = 1.0f / w.det;
    const f32 f11_0 = w.f11_0 * inv;
    const f32 f11_1 = w.f11_1 * inv;
    const f32 f9_0 = w.f9_0 * inv;
    const f32 f9_1 = w.f9_1 * inv;

    f32 x = pModelMtx->_00 * f11_0;
    f32 y = pModelMtx->_10 * f11_0;
    x = __builtin_fmaf(pModelMtx->_01, f11_1, x);
    y = __builtin_fmaf(pModelMtx->_11, f11_1, y);
    x = __builtin_fmaf(pModelMtx->_02, f9_0, x);
    y = __builtin_fmaf(pModelMtx->_12, f9_0, y);

    // The original multiplies row 2 by f9 (ps_mul f1, f1, f9), not by f11 as
    // it does for rows 0 and 1, so z is not the third component of the same
    // matrix product. Kept exactly as the original computes it.
    f32 z = pModelMtx->_20 * f9_0 + pModelMtx->_21 * f9_1; // ps_mul, ps_sum0
    z = __builtin_fmaf(pModelMtx->_22, f9_0, z);

    pVec->x = x;
    pVec->y = y;
    pVec->z = z;
}

// The three SetMdlViewMtxSR leave column 3 (the translation) untouched.
inline void SetMdlViewMtxSR(math::MTX34* pViewPos, const math::VEC3& rRY,
                            f32 s) {
    const f32 x = rRY.x * s;
    const f32 y = rRY.y * s;

    pViewPos->_00 = y;
    pViewPos->_01 = x;
    pViewPos->_10 = -x;
    pViewPos->_11 = y;
    pViewPos->_02 = 0.0f;
    pViewPos->_12 = 0.0f;
    pViewPos->_20 = 0.0f;
    pViewPos->_21 = 0.0f;
    pViewPos->_22 = s;
}

inline void SetMdlViewMtxSR(math::MTX34* pViewPos, const math::VEC3& rRY,
                            f32 sx, f32 sy, f32 sz) {
    const f32 x = rRY.x;
    const f32 y = rRY.y;

    pViewPos->_00 = y * sx;
    pViewPos->_01 = x * sy;
    pViewPos->_10 = -x * sx;
    pViewPos->_11 = y * sy;
    pViewPos->_02 = 0.0f;
    pViewPos->_12 = 0.0f;
    pViewPos->_20 = 0.0f;
    pViewPos->_21 = 0.0f;
    pViewPos->_22 = sz;
}

inline void SetMdlViewMtxSR(math::MTX34* pViewPos, const math::VEC3& rRX,
                            const math::VEC3& rRY, const math::VEC3& rRZ,
                            f32 sx, f32 sy, f32 sz) {
    pViewPos->_00 = rRX.x * sx;
    pViewPos->_01 = rRY.x * sy;
    pViewPos->_10 = rRX.y * sx;
    pViewPos->_11 = rRY.y * sy;
    pViewPos->_02 = rRZ.x * sz;
    pViewPos->_12 = rRZ.y * sz;
    pViewPos->_20 = rRX.z * sx;
    pViewPos->_21 = rRY.z * sy;
    pViewPos->_22 = rRZ.z * sz;
}
#else
asm void GetModelLocalAxisY2(register math::VEC3* pVec,
                             register const math::MTX34* pModelMtx,
                             register const math::MTX34* pParentModelMtx) {
    // clang-format off
    nofralloc

    psq_l      f2, MTX34._10(pParentModelMtx), 1, 0
    psq_l      f3, MTX34._11(pParentModelMtx), 0, 0
    psq_l      f0, MTX34._00(pParentModelMtx), 1, 0
    psq_l      f1, MTX34._01(pParentModelMtx), 0, 0
    ps_merge10 f7, f3, f2
    psq_l      f5, MTX34._21(pParentModelMtx), 0, 0
    psq_l      f4, MTX34._20(pParentModelMtx), 1, 0
    ps_merge10 f6,  f1, f0
    ps_mul     f12, f5, f7
    ps_merge10 f8,  f5, f4
    ps_mul     f10, f3, f6
    ps_mul     f9,  f0, f5
    ps_mul     f11, f1, f8
    ps_msub    f12, f3, f8, f12
    ps_msub    f10, f1, f7, f10
    ps_msub    f11, f5, f6, f11
    ps_mul     f7,  f0, f12
    ps_sub     f6,  f6, f6
    ps_msub    f9,  f1, f4,  f9
    ps_madd    f7,  f2, f11, f7
    stfs       f6,  VEC3.z(pVec)
    ps_madd    f7,  f4, f10, f7
    ps_cmpo0   cr0, f7, f6
    bne        _lbl_80068168
    psq_st     f6,  VEC3.x(pVec), 0, 0

    blr

_lbl_80068168:
    fres       f0,  f7
    psq_l      f1,  MTX34._10(pModelMtx), 0, 0
    psq_l      f2,  MTX34._02(pModelMtx), 1, 0
    psq_l      f3,  MTX34._12(pModelMtx), 1, 0
    ps_add     f6,  f0,  f0
    ps_mul     f5,  f0,  f0
    ps_nmsub   f0,  f7,  f5, f6
    ps_merge00 f6,  f2,  f3
    ps_muls0   f11, f11, f0
    ps_muls0   f9,  f9,  f0
    psq_l f0,  MTX34._00(pModelMtx), 0, 0
    ps_merge00 f4, f0, f1
    ps_merge11 f5, f0, f1
    ps_muls0   f0, f4, f11
    ps_madds1  f0, f5, f11, f0
    ps_madds0  f0, f6, f9,  f0
    psq_st     f0, VEC3.x(pVec), 0, 0

    blr
    // clang-format on
}

asm void GetModelLocalAxisY3(register math::VEC3* pVec,
                             register const math::MTX34* pModelMtx,
                             register const math::MTX34* pParentModelMtx) {
    // clang-format off
    nofralloc

    psq_l      f2, MTX34._10(pParentModelMtx), 1, 0
    psq_l      f3, MTX34._11(pParentModelMtx), 0, 0
    psq_l      f0, MTX34._00(pParentModelMtx), 1, 0
    psq_l      f1, MTX34._01(pParentModelMtx), 0, 0
    ps_merge10 f7, f3, f2
    psq_l      f5, MTX34._21(pParentModelMtx), 0, 0
    psq_l      f4, MTX34._20(pParentModelMtx), 1, 0
    ps_merge10 f6,  f1, f0
    ps_mul     f12, f5, f7
    ps_merge10 f8,  f5, f4
    ps_mul     f10, f3, f6
    ps_mul     f9,  f0, f5
    ps_mul     f11, f1, f8
    ps_msub    f12, f3, f8, f12
    ps_msub    f10, f1, f7, f10
    ps_msub    f11, f5, f6, f11
    ps_mul     f7,  f0, f12
    ps_sub     f6,  f6, f6
    ps_msub    f9,  f1, f4, f9
    ps_madd    f7,  f2, f11, f7
    ps_madd    f7,  f4, f10, f7
    ps_cmpo0   cr0, f7, f6
    bne        _lbl_80068218
    psq_st     f6, VEC3.x(pVec), 0, 0
    stfs       f6, VEC3.z(pVec)

    blr

_lbl_80068218:
    fres       f0, f7
    psq_l      f1, MTX34._10(pModelMtx), 0, 0
    psq_l      f2, MTX34._02(pModelMtx), 1, 0
    psq_l      f3, MTX34._12(pModelMtx), 1, 0
    ps_add     f6, f0, f0
    ps_mul     f5, f0, f0
    ps_nmsub   f0, f7, f5, f6
    ps_merge00 f6, f2, f3
    psq_l      f2, MTX34._22(pModelMtx), 1, 0
    ps_muls0   f11, f11, f0
    ps_muls0   f9,  f9,  f0
    psq_l      f0, MTX34._00(pModelMtx), 0, 0
    ps_merge00 f4, f0, f1
    ps_merge11 f5, f0, f1
    psq_l      f1, MTX34._20(pModelMtx), 0, 0
    ps_muls0   f0, f4, f11
    ps_mul     f1, f1, f9
    ps_madds1  f0, f5, f11, f0
    ps_sum0    f1, f1, f1,  f1
    ps_madds0  f0, f6, f9,  f0
    fmadds     f1, f2, f9,  f1
    psq_st     f0, VEC3.x(pVec), 0, 0
    psq_st     f1, VEC3.z(pVec), 1, 0

    blr
    // clang-format on
}

inline void SetMdlViewMtxSR(register math::MTX34* pViewPos,
                            register const math::VEC3& rRY, register f32 s) {
    register f32 work0, work1;
    register f32 c_zero = 0.0f;

    ASM (
        psq_l      work0,  VEC3.x(rRY), 0, 0
        ps_muls0   work0,  work0, s
        ps_merge10 work1,  work0, work0
        psq_st     work1,  MTX34._00(pViewPos), 0, 0
        ps_neg     work1,  work0
        ps_merge01 work1,  work1, work0
        psq_st     work1,  MTX34._10(pViewPos), 0, 0
        stfs       c_zero, MTX34._02(pViewPos)
        stfs       c_zero, MTX34._12(pViewPos)
        psq_st     c_zero, MTX34._20(pViewPos), 0, 0
        stfs       s,      MTX34._22(pViewPos)
    )
}

inline void SetMdlViewMtxSR(register math::MTX34* pViewPos,
                            register const math::VEC3& rRY, register f32 sx,
                            register f32 sy, register f32 sz) {

    register f32 work0, work1, work2, work3;
    register f32 c_zero = 0.0f;

    ASM (
        ps_merge00 work0,  sx, sy
        psq_l      work1,  VEC3.x(rRY), 0, 0
        ps_merge10 work3,  work1, work1
        ps_mul     work3,  work3, work0
        psq_st     work3,  MTX34._00(pViewPos), 0, 0
        ps_neg     work2,  work1
        ps_merge01 work3,  work2, work1
        ps_mul     work3,  work3, work0
        psq_st     work3,  MTX34._10(pViewPos), 0, 0
        stfs       c_zero, MTX34._02(pViewPos)
        stfs       c_zero, MTX34._12(pViewPos)
        psq_st     c_zero, MTX34._20(pViewPos), 0, 0
        stfs       sz,     MTX34._22(pViewPos)
    )
}

inline void SetMdlViewMtxSR(register math::MTX34* pViewPos,
                            register const math::VEC3& rRX,
                            register const math::VEC3& rRY,
                            register const math::VEC3& rRZ, register f32 sx,
                            register f32 sy, register f32 sz) {

    register f32 work0, work1, work2, work3;

    ASM (
        psq_l      work0, VEC3.x(rRX), 0, 0
        psq_l      work1, VEC3.x(rRY), 0, 0
        ps_muls0   work0, work0, sx
        ps_muls0   work1, work1, sy
        ps_merge00 work2, work0, work1
        ps_merge11 work3, work0, work1
        psq_st     work2, MTX34._00(pViewPos), 0, 0
        psq_st     work3, MTX34._10(pViewPos), 0, 0
        psq_l      work0, VEC3.x(rRZ),         0, 0
        ps_muls0   work0, work0, sz
        stfs       work0, MTX34._02(pViewPos)
        ps_merge11 work0, work0, work0
        stfs       work0, MTX34._12(pViewPos)
        lfs        work0, VEC3.z(rRX)
        fmuls      work0, work0, sx
        stfs       work0, MTX34._20(pViewPos)
        lfs        work0, VEC3.z(rRY)
        fmuls      work0, work0, sy
        stfs       work0, MTX34._21(pViewPos)
        lfs        work0, VEC3.z(rRZ)
        fmuls      work0, work0, sz
        stfs       work0, MTX34._22(pViewPos)
    )
}
#endif // TARGET_PC

} // namespace

#ifdef TARGET_PC
// Entry points for the self-test (src/pc/selftest_g3d.cpp): the routines above
// are local to this file.
void PCTestGetModelLocalAxisY2(math::VEC3* pVec, const math::MTX34* pModelMtx,
                               const math::MTX34* pParentModelMtx) {
    GetModelLocalAxisY2(pVec, pModelMtx, pParentModelMtx);
}

void PCTestGetModelLocalAxisY3(math::VEC3* pVec, const math::MTX34* pModelMtx,
                               const math::MTX34* pParentModelMtx) {
    GetModelLocalAxisY3(pVec, pModelMtx, pParentModelMtx);
}

void PCTestSetMdlViewMtxSR(math::MTX34* pViewPos, const math::VEC3& rRY,
                           f32 s) {
    SetMdlViewMtxSR(pViewPos, rRY, s);
}

void PCTestSetMdlViewMtxSR(math::MTX34* pViewPos, const math::VEC3& rRY,
                           f32 sx, f32 sy, f32 sz) {
    SetMdlViewMtxSR(pViewPos, rRY, sx, sy, sz);
}

void PCTestSetMdlViewMtxSR(math::MTX34* pViewPos, const math::VEC3& rRX,
                           const math::VEC3& rRY, const math::VEC3& rRZ,
                           f32 sx, f32 sy, f32 sz) {
    SetMdlViewMtxSR(pViewPos, rRX, rRY, rRZ, sx, sy, sz);
}
#endif

/******************************************************************************
 *
 * Billboard implementation
 *
 ******************************************************************************/
namespace {

inline f32 GetMtx34Scale(const math::MTX34& rMtx, int col) {
    return math::FSqrt(rMtx.m[0][col] * rMtx.m[0][col] +
                       rMtx.m[1][col] * rMtx.m[1][col] +
                       rMtx.m[2][col] * rMtx.m[2][col]);
}

void Calc_BILLBOARD_STD(math::MTX34* pViewPos,
                        const math::MTX34* pModelMtxArray, bool uniformScale,
                        const math::MTX34* pViewMtx, const ResMdl mdl,
                        const u32 id) {

#pragma unused(pViewMtx)
#pragma unused(mdl)

    math::VEC3 vy(pViewPos->_01, pViewPos->_11, 0.0f);
    math::VEC3Normalize(&vy, &vy);

    if (uniformScale) {
        f32 s = GetMtx34Scale(pModelMtxArray[id], 0);
        SetMdlViewMtxSR(pViewPos, vy, s);
    } else {
        f32 sx = GetMtx34Scale(pModelMtxArray[id], 0);
        f32 sy = GetMtx34Scale(pModelMtxArray[id], 1);
        f32 sz = GetMtx34Scale(pModelMtxArray[id], 2);
        SetMdlViewMtxSR(pViewPos, vy, sx, sy, sz);
    }
}

void Calc_BILLBOARD_PERSP_STD(math::MTX34* pViewPos,
                              const math::MTX34* pModelMtxArray,
                              bool uniformScale, const math::MTX34* pViewMtx,
                              const ResMdl mdl, const u32 id) {

#pragma unused(pViewMtx)
#pragma unused(mdl)

    math::VEC3 vx;
    math::VEC3 vy(pViewPos->_01, pViewPos->_11, pViewPos->_21);
    math::VEC3 vz(-pViewPos->_03, -pViewPos->_13, -pViewPos->_23);

    math::VEC3Normalize(&vz, &vz);
    math::VEC3Cross(&vx, &vy, &vz);

    math::VEC3Normalize(&vx, &vx);
    math::VEC3Cross(&vy, &vz, &vx);

    if (uniformScale) {
        f32 s = GetMtx34Scale(pModelMtxArray[id], 0);
        SetMdlViewMtxSR(pViewPos, vx, vy, vz, s, s, s);
    } else {
        f32 sx = GetMtx34Scale(pModelMtxArray[id], 0);
        f32 sy = GetMtx34Scale(pModelMtxArray[id], 1);
        f32 sz = GetMtx34Scale(pModelMtxArray[id], 2);
        SetMdlViewMtxSR(pViewPos, vx, vy, vz, sx, sy, sz);
    }
}

void Calc_BILLBOARD_ROT(math::MTX34* pViewPos,
                        const math::MTX34* pModelMtxArray, bool uniformScale,
                        const math::MTX34* pViewMtx, const ResMdl mdl,
                        const u32 id) {

#pragma unused(pViewMtx)

    math::VEC3 vy;

    int nodeID = mdl.GetResMdlInfo().GetNodeIDFromMtxID(id);

    if (nodeID >= 0) {
        ResNode node = mdl.GetResNode(nodeID);
        ResNode parent = node.GetParentNode();

        if (parent.IsValid()) {
            u32 parentMtxID = parent.GetMtxID();
            GetModelLocalAxisY2(&vy, &pModelMtxArray[id],
                                &pModelMtxArray[parentMtxID]);
        } else {
            vy.x = pModelMtxArray[id]._01;
            vy.y = pModelMtxArray[id]._11;
            vy.z = 0.0f;
        }
    } else {
        vy.x = pModelMtxArray[id]._01;
        vy.y = pModelMtxArray[id]._11;
        vy.z = 0.0f;
    }

    math::VEC3Normalize(&vy, &vy);

    if (uniformScale) {
        f32 s = GetMtx34Scale(pModelMtxArray[id], 0);
        SetMdlViewMtxSR(pViewPos, vy, s);
    } else {
        f32 sx = GetMtx34Scale(pModelMtxArray[id], 0);
        f32 sy = GetMtx34Scale(pModelMtxArray[id], 1);
        f32 sz = GetMtx34Scale(pModelMtxArray[id], 2);
        SetMdlViewMtxSR(pViewPos, vy, sx, sy, sz);
    }
}

void Calc_BILLBOARD_PERSP_ROT(math::MTX34* pViewPos,
                              const math::MTX34* pModelMtxArray,
                              bool uniformScale, const math::MTX34* pViewMtx,
                              const ResMdl mdl, const u32 id) {

#pragma unused(pViewMtx)

    math::VEC3 vx;
    math::VEC3 vy;
    math::VEC3 vz(-pViewPos->_03, -pViewPos->_13, -pViewPos->_23);

    int nodeID = mdl.GetResMdlInfo().GetNodeIDFromMtxID(id);

    if (nodeID >= 0) {
        ResNode node = mdl.GetResNode(nodeID);
        ResNode parent = node.GetParentNode();

        if (parent.IsValid()) {
            u32 parentMtxID = parent.GetMtxID();
            GetModelLocalAxisY3(&vy, &pModelMtxArray[id],
                                &pModelMtxArray[parentMtxID]);
        } else {
            vy.x = pModelMtxArray[id]._01;
            vy.y = pModelMtxArray[id]._11;
            vy.z = pModelMtxArray[id]._21;
        }
    } else {
        vy.x = pModelMtxArray[id]._01;
        vy.y = pModelMtxArray[id]._11;
        vy.z = pModelMtxArray[id]._21;
    }

    math::VEC3Normalize(&vz, &vz);
    math::VEC3Cross(&vx, &vy, &vz);

    math::VEC3Normalize(&vx, &vx);
    math::VEC3Cross(&vy, &vz, &vx);

    if (uniformScale) {
        f32 s = GetMtx34Scale(pModelMtxArray[id], 0);
        SetMdlViewMtxSR(pViewPos, vx, vy, vz, s, s, s);
    } else {
        f32 sx = GetMtx34Scale(pModelMtxArray[id], 0);
        f32 sy = GetMtx34Scale(pModelMtxArray[id], 1);
        f32 sz = GetMtx34Scale(pModelMtxArray[id], 2);
        SetMdlViewMtxSR(pViewPos, vx, vy, vz, sx, sy, sz);
    }
}

void Calc_BILLBOARD_Y(math::MTX34* pViewPos, const math::MTX34* pModelMtxArray,
                      bool uniformScale, const math::MTX34* pViewMtx,
                      const ResMdl mdl, const u32 id) {

#pragma unused(pViewMtx)
#pragma unused(mdl)

    math::VEC3 vz;
    math::VEC3 vy(pViewPos->_01, pViewPos->_11, pViewPos->_21);
    math::VEC3 vx(vy.y, -vy.x, 0.0f);

    f32 sy = GetMtx34Scale(pModelMtxArray[id], 1);
    f32 invSY = math::FInv(sy);

    vy *= invSY;

    math::VEC3Normalize(&vx, &vx);
    math::VEC3Cross(&vz, &vx, &vy);

    if (uniformScale) {
        SetMdlViewMtxSR(pViewPos, vx, vy, vz, sy, sy, sy);
    } else {
        f32 sx = GetMtx34Scale(pModelMtxArray[id], 0);
        f32 sz = GetMtx34Scale(pModelMtxArray[id], 2);
        SetMdlViewMtxSR(pViewPos, vx, vy, vz, sx, sy, sz);
    }
}

void Calc_BILLBOARD_PERSP_Y(math::MTX34* pViewPos,
                            const math::MTX34* pModelMtxArray,
                            bool uniformScale, const math::MTX34* pViewMtx,
                            const ResMdl mdl, const u32 id) {

#pragma unused(pViewMtx)
#pragma unused(mdl)

    math::VEC3 vx;
    math::VEC3 vy(pViewPos->_01, pViewPos->_11, pViewPos->_21);
    math::VEC3 vz(-pViewPos->_03, -pViewPos->_13, -pViewPos->_23);

    f32 sy = GetMtx34Scale(pModelMtxArray[id], 1);
    f32 invSY = math::FInv(sy);

    vy *= invSY;
    math::VEC3Cross(&vx, &vy, &vz);

    math::VEC3Normalize(&vx, &vx);
    math::VEC3Cross(&vz, &vx, &vy);

    if (uniformScale) {
        SetMdlViewMtxSR(pViewPos, vx, vy, vz, sy, sy, sy);
    } else {
        f32 sx = GetMtx34Scale(pModelMtxArray[id], 0);
        f32 sz = GetMtx34Scale(pModelMtxArray[id], 2);
        SetMdlViewMtxSR(pViewPos, vx, vy, vz, sx, sy, sz);
    }
}

typedef void (*BillBoardFunc)(math::MTX34* pViewPos,
                              const math::MTX34* pModelMtxArray,
                              bool uniformScale, const math::MTX34* pViewMtx,
                              const ResMdl mdl, const u32 id);

const BillBoardFunc bbFunc[ResNodeData::NUM_BILLBOARD] = {
    NULL, // BILLBOARD_OFF is ignored

    Calc_BILLBOARD_STD,       //
    Calc_BILLBOARD_PERSP_STD, //

    Calc_BILLBOARD_ROT,       //
    Calc_BILLBOARD_PERSP_ROT, //

    Calc_BILLBOARD_Y,      //
    Calc_BILLBOARD_PERSP_Y //
};

} // namespace

/******************************************************************************
 *
 * CalcView
 *
 ******************************************************************************/
void CalcView(math::MTX34* pViewPosArray, math::MTX33* pViewNrmArray,
              const math::MTX34* pModelMtxArray,
              const u32* pModelMtxAttribArray, u32 numMtx,
              const math::MTX34* pViewMtx, const ResMdl mdl,
              math::MTX34* pViewTexMtxArray) {

    if (numMtx <= 0) {
        return;
    }

    math::MTX34* pVArray;
    math::MTX33* pNArray;
    math::MTX34* pCurTArray;

    u32 sizeVArray = align32(numMtx * sizeof(math::MTX34));
    u32 sizeNArray = align32(numMtx * sizeof(math::MTX33));
    u32 sizeTArray = align32(numMtx * sizeof(math::MTX34));

    pVArray = pViewPosArray;

    if (numMtx > 1) {
        math::MTX34MultArray(pVArray, pViewMtx, pModelMtxArray, numMtx);
    } else {
        math::MTX34Mult(pVArray, pViewMtx, pModelMtxArray);
    }

    math::MTX34* pBbMtxArray = detail::workmem::GetBillboardMtxTemporary();

    for (u32 i = 0; i < numMtx; i++) {
        u32 attr = pModelMtxAttribArray[i];

        ResNodeData::Billboard bbType =
            detail::WorldMtxAttr::GetBillboard(attr);

        if (bbType != ResNodeData::BILLBOARD_OFF) {
            math::MTX34& rVMtx = pVArray[i];

            bbFunc[bbType](&rVMtx, pModelMtxArray,
                           detail::WorldMtxAttr::IsAllScaleUniform(attr),
                           pViewMtx, mdl, i);

            s32 nodeID = mdl.GetResMdlInfo().GetNodeIDFromMtxID(i);
            ResNode node = mdl.GetResNode(static_cast<u32>(nodeID));

            if (node.IsValid() && node.GetChildNode().IsValid()) {
                math::MTX34 invWorld;

                u32 ret = math::MTX34Inv(&invWorld, &pModelMtxArray[i]);
                if (ret == TRUE) {
                    math::MTX34Mult(&pBbMtxArray[i], &pVArray[i], &invWorld);
                } else {
                    math::MTX34Identity(&pBbMtxArray[i]);
                }
            }
        } else {
            s32 nodeID = mdl.GetResMdlInfo().GetNodeIDFromMtxID(i);
            if (nodeID < 0) {
                continue;
            }

            ResNode node = mdl.GetResNode(static_cast<u32>(nodeID));
            if (!node.IsValid()) {
                continue;
            }

            if (!(node.ref().flags & ResNodeData::FLAG_BILLBOARD_PARENT)) {
                continue;
            }

            u32 bbMtxID = mdl.GetResNode(node.ref().bbref_nodeid).GetMtxID();

            math::MTX34Mult(&pVArray[i], &pBbMtxArray[bbMtxID],
                            &pModelMtxArray[i]);
        }
    }

    pNArray = pViewNrmArray;
    pCurTArray = pViewTexMtxArray;

    if (pNArray != NULL) {
        for (u32 i = 0; i < numMtx; i++) {
            math::MTX34& rVMtx = pVArray[i];
            math::MTX33& rNMtx = pNArray[i];
            u32 attr = pModelMtxAttribArray[i];

            if (detail::WorldMtxAttr::IsAllScaleUniform(attr)) {
                if (pCurTArray != NULL) {
                    math::MTX34Copy(&pCurTArray[i], &rVMtx);
                    pCurTArray[i]._03 = pCurTArray[i]._13 = pCurTArray[i]._23 =
                        0.0f;
                }

                math::MTX34ToMTX33(&rNMtx, &rVMtx);
            } else {
                if (pCurTArray != NULL) {
                    math::MTX34InvTranspose(&pCurTArray[i], &rVMtx);
                    math::MTX34ToMTX33(&rNMtx, &pCurTArray[i]);
                } else {
                    math::MTX34InvTranspose(&rNMtx, &rVMtx);
                }
            }
        }
    }

    DC::FlushRangeNoSync(pVArray, sizeVArray);

    if (pNArray != NULL) {
        DC::FlushRangeNoSync(pViewNrmArray, sizeNArray);

        if (pCurTArray != NULL) {
            DC::FlushRangeNoSync(pViewTexMtxArray, sizeTArray);
        }
    }
}

void CalcView_LC(math::MTX34* pViewPosArray, math::MTX33* pViewNrmArray,
                 const math::MTX34* pModelMtxArray,
                 const u32* pModelMtxAttribArray, u32 numMtx,
                 const math::MTX34* pViewMtx, const ResMdl mdl,
                 math::MTX34* pViewTexMtxArray) {

    if (numMtx <= 0) {
        return;
    }

    detail::MtxCacheMap* pMtxCache =
        static_cast<detail::MtxCacheMap*>(ut::LC::GetBase());

    math::MTX34* pCurVArray =
        reinterpret_cast<math::MTX34*>(pMtxCache->currViewArray);
    math::MTX33* pCurNArray =
        reinterpret_cast<math::MTX33*>(pMtxCache->currNrmArray);
    math::MTX34* pCurTArray =
        reinterpret_cast<math::MTX34*>(pMtxCache->currTexArray);

    math::MTX34* pPrevVArray =
        reinterpret_cast<math::MTX34*>(pMtxCache->prevViewArray);
    math::MTX33* pPrevNArray =
        reinterpret_cast<math::MTX33*>(pMtxCache->prevNrmArray);
    math::MTX34* pPrevTArray =
        reinterpret_cast<math::MTX34*>(pMtxCache->prevTexArray);

    u32 sizeVArray = align32(numMtx * sizeof(math::MTX34));
    DC::InvalidateRange(pViewPosArray, sizeVArray);

    u32 queueLen = 1;
    u32 sizeNArray, sizeTArray;

    if (pViewNrmArray != NULL) {
        queueLen++;

        sizeNArray = align32(numMtx * sizeof(math::MTX33));
        DC::InvalidateRange(pViewNrmArray, sizeNArray);

        if (pViewTexMtxArray != NULL) {
            queueLen++;

            sizeTArray = align32(numMtx * sizeof(math::MTX34));
            DC::InvalidateRange(pViewTexMtxArray, sizeTArray);
        }
    }

    math::MTX34* pBbMtxArray = detail::workmem::GetBillboardMtxTemporary();

    ut::LC::QueueWait(0);

    u32 baseMtx;
    for (baseMtx = 0; baseMtx < numMtx; baseMtx += 40) {
        u32 remain = numMtx - baseMtx;

        u32 numBlocks, numBlocksNrm;
        if (remain > 40) {
            remain = 40;
            numBlocks = 60;
            numBlocksNrm = 45;
        } else {
            numBlocks = align32(remain * sizeof(math::MTX34)) / 32;
            numBlocksNrm = align32(remain * sizeof(math::MTX33)) / 32;
        }

        ut::LC::QueueWait(queueLen);

        if (remain > 1) {
            math::MTX34MultArray(pCurVArray, pViewMtx, &pModelMtxArray[baseMtx],
                                 remain);
        } else {
            math::MTX34Mult(pCurVArray, pViewMtx, &pModelMtxArray[baseMtx]);
        }

        for (u32 i = 0; i < remain; i++) {
            u32 attr = pModelMtxAttribArray[baseMtx + i];

            ResNodeData::Billboard bbType =
                detail::WorldMtxAttr::GetBillboard(attr);

            if (bbType != ResNodeData::BILLBOARD_OFF) {
                math::MTX34& rVMtx = pCurVArray[i];

                bbFunc[bbType](&rVMtx, pModelMtxArray,
                               detail::WorldMtxAttr::IsAllScaleUniform(attr),
                               pViewMtx, mdl, baseMtx + i);

                s32 nodeID =
                    mdl.GetResMdlInfo().GetNodeIDFromMtxID(baseMtx + i);

                ResNode node = mdl.GetResNode(static_cast<u32>(nodeID));

                if (node.IsValid() && node.GetChildNode().IsValid()) {
                    math::MTX34 invWorld;

                    u32 ret =
                        math::MTX34Inv(&invWorld, &pModelMtxArray[baseMtx + i]);

                    if (ret == TRUE) {
                        math::MTX34Mult(&pBbMtxArray[baseMtx + i],
                                        &pCurVArray[i], &invWorld);
                    } else {
                        math::MTX34Identity(&pBbMtxArray[baseMtx + i]);
                    }
                }
            } else {
                s32 nodeID =
                    mdl.GetResMdlInfo().GetNodeIDFromMtxID(baseMtx + i);
                if (nodeID < 0) {
                    continue;
                }

                ResNode node = mdl.GetResNode(static_cast<u32>(nodeID));
                if (!node.IsValid()) {
                    continue;
                }

                if (!(node.ref().flags & ResNodeData::FLAG_BILLBOARD_PARENT)) {
                    continue;
                }

                u32 bbMtxID =
                    mdl.GetResNode(node.ref().bbref_nodeid).GetMtxID();

                math::MTX34Mult(&pCurVArray[i], &pBbMtxArray[bbMtxID],
                                &pModelMtxArray[baseMtx + i]);
            }
        }

        ut::LC::StoreBlocks(&pViewPosArray[baseMtx], pCurVArray, numBlocks);

        if (pViewNrmArray != NULL) {
            for (u32 i = 0; i < remain; i++) {
                math::MTX34& rVMtx = pCurVArray[i];
                math::MTX33& rNMtx = pCurNArray[i];
                u32 attr = pModelMtxAttribArray[baseMtx + i];

                if (detail::WorldMtxAttr::IsAllScaleUniform(attr)) {
                    if (pViewTexMtxArray != NULL) {
                        math::MTX34Copy(&pCurTArray[i], &rVMtx);

                        pCurTArray[i]._03 = pCurTArray[i]._13 =
                            pCurTArray[i]._23 = 0.0f;
                    }

                    math::MTX34ToMTX33(&rNMtx, &rVMtx);
                } else {
                    if (pViewTexMtxArray != NULL) {
                        math::MTX34InvTranspose(&pCurTArray[i], &rVMtx);
                        math::MTX34ToMTX33(&rNMtx, &pCurTArray[i]);
                    } else {
                        math::MTX34InvTranspose(&rNMtx, &rVMtx);
                    }
                }
            }
        }

        if (pViewNrmArray != NULL) {
            ut::LC::StoreBlocks(&pViewNrmArray[baseMtx], pCurNArray,
                                numBlocksNrm);

            if (pViewTexMtxArray != NULL) {
                ut::LC::StoreBlocks(&pViewTexMtxArray[baseMtx], pCurTArray,
                                    numBlocks);
            }
        }

        std::swap(pCurVArray, pPrevVArray);
        std::swap(pCurNArray, pPrevNArray);
        std::swap(pCurTArray, pPrevTArray);
    }
}

void CalcView_LC_DMA_ModelMtx(math::MTX34* pViewPosArray,
                              math::MTX33* pViewNrmArray,
                              const math::MTX34* pModelMtxArray,
                              const u32* pModelMtxAttribArray, u32 numMtx,
                              const math::MTX34* pViewMtx, const ResMdl mdl,
                              math::MTX34* pViewTexMtxArray) {

    if (numMtx <= 0) {
        return;
    }

    detail::MtxCacheMap* pMtxCache =
        static_cast<detail::MtxCacheMap*>(ut::LC::GetBase());

    math::MTX34* pCurMArray =
        reinterpret_cast<math::MTX34*>(pMtxCache->currDmaArray);
    math::MTX34* pCurVArray =
        reinterpret_cast<math::MTX34*>(pMtxCache->currViewArray);
    math::MTX33* pCurNArray =
        reinterpret_cast<math::MTX33*>(pMtxCache->currNrmArray);
    math::MTX34* pCurTArray =
        reinterpret_cast<math::MTX34*>(pMtxCache->currTexArray);

    math::MTX34* pPrevMArray =
        reinterpret_cast<math::MTX34*>(pMtxCache->prevDmaArray);
    math::MTX34* pPrevVArray =
        reinterpret_cast<math::MTX34*>(pMtxCache->prevViewArray);
    math::MTX33* pPrevNArray =
        reinterpret_cast<math::MTX33*>(pMtxCache->prevNrmArray);
    math::MTX34* pPrevTArray =
        reinterpret_cast<math::MTX34*>(pMtxCache->prevTexArray);

    u32 sizeVArray = align32(numMtx * sizeof(math::MTX34));
    DC::InvalidateRange(pViewPosArray, sizeVArray);

    u32 queueLen = 1;
    u32 sizeNArray, sizeTArray;

    if (pViewNrmArray != NULL) {
        queueLen++;

        sizeNArray = align32(numMtx * sizeof(math::MTX33));
        DC::InvalidateRange(pViewNrmArray, sizeNArray);

        if (pViewTexMtxArray != NULL) {
            queueLen++;

            sizeTArray = align32(numMtx * sizeof(math::MTX34));
            DC::InvalidateRange(pViewTexMtxArray, sizeTArray);
        }
    }

    math::MTX34* pBbMtxArray = detail::workmem::GetBillboardMtxTemporary();

    ut::LC::QueueWait(0);

    u32 numBlocks;
    if (numMtx > 40) {
        numBlocks = 60;
    } else {
        numBlocks = align32(numMtx * sizeof(math::MTX34)) / 32;
    }

    ut::LC::LoadBlocks(pCurMArray, const_cast<math::MTX34*>(pModelMtxArray),
                       numBlocks);

    if (numMtx > 20) {
        ut::LC::QueueWaitEx(0);
    } else {
        ut::LC::QueueWait(0);
    }

    u32 baseMtx;
    for (baseMtx = 0; baseMtx < numMtx; baseMtx += 40) {
        u32 remain = numMtx - baseMtx;

        u32 numBlocks, numBlocksNrm;
        if (remain > 40) {
            remain = 40;
            numBlocks = 60;
            numBlocksNrm = 45;
        } else {
            numBlocks = align32(remain * sizeof(math::MTX34)) / 32;
            numBlocksNrm = align32(remain * sizeof(math::MTX33)) / 32;
        }

        ut::LC::QueueWait(queueLen);

        if (baseMtx + 40 < numMtx) {
            u32 n = numMtx - baseMtx - 40;

            u32 blocks;
            if (n > 40) {
                blocks = 60;
            } else {
                blocks = align32(n * sizeof(math::MTX34)) / 32;
            }

            ut::LC::LoadBlocks(
                pPrevMArray,
                const_cast<math::MTX34*>(&pModelMtxArray[baseMtx + 40]),
                blocks);
        }

        if (remain > 1) {
            math::MTX34MultArray(pCurVArray, pViewMtx, pCurMArray, remain);
        } else {
            math::MTX34Mult(pCurVArray, pViewMtx, pCurMArray);
        }

        for (u32 i = 0; i < remain; i++) {
            u32 attr = pModelMtxAttribArray[baseMtx + i];

            ResNodeData::Billboard bbType =
                detail::WorldMtxAttr::GetBillboard(attr);

            if (bbType != ResNodeData::BILLBOARD_OFF) {
                math::MTX34& rVMtx = pCurVArray[i];

                bbFunc[bbType](&rVMtx, pModelMtxArray,
                               detail::WorldMtxAttr::IsAllScaleUniform(attr),
                               pViewMtx, mdl, baseMtx + i);

                s32 nodeID =
                    mdl.GetResMdlInfo().GetNodeIDFromMtxID(baseMtx + i);

                ResNode node = mdl.GetResNode(static_cast<u32>(nodeID));

                if (node.IsValid() && node.GetChildNode().IsValid()) {
                    math::MTX34 invWorld;

                    u32 ret =
                        math::MTX34Inv(&invWorld, &pModelMtxArray[baseMtx + i]);

                    if (ret == TRUE) {
                        math::MTX34Mult(&pBbMtxArray[baseMtx + i],
                                        &pCurVArray[i], &invWorld);
                    } else {
                        math::MTX34Identity(&pBbMtxArray[baseMtx + i]);
                    }
                }
            } else {
                s32 nodeID =
                    mdl.GetResMdlInfo().GetNodeIDFromMtxID(baseMtx + i);
                if (nodeID < 0) {
                    continue;
                }

                ResNode node = mdl.GetResNode(static_cast<u32>(nodeID));
                if (!node.IsValid()) {
                    continue;
                }

                if (!(node.ref().flags & ResNodeData::FLAG_BILLBOARD_PARENT)) {
                    continue;
                }

                u32 bbMtxID =
                    mdl.GetResNode(node.ref().bbref_nodeid).GetMtxID();

                math::MTX34Mult(&pCurVArray[i], &pBbMtxArray[bbMtxID],
                                &pModelMtxArray[baseMtx + i]);
            }
        }

        ut::LC::StoreBlocks(&pViewPosArray[baseMtx], pCurVArray, numBlocks);

        if (pViewNrmArray != NULL) {
            for (u32 i = 0; i < remain; i++) {
                math::MTX34& rVMtx = pCurVArray[i];
                math::MTX33& rNMtx = pCurNArray[i];
                u32 attr = pModelMtxAttribArray[baseMtx + i];

                if (detail::WorldMtxAttr::IsAllScaleUniform(attr)) {
                    if (pViewTexMtxArray != NULL) {
                        math::MTX34Copy(&pCurTArray[i], &rVMtx);

                        pCurTArray[i]._03 = pCurTArray[i]._13 =
                            pCurTArray[i]._23 = 0.0f;
                    }

                    math::MTX34ToMTX33(&rNMtx, &rVMtx);
                } else {
                    if (pViewTexMtxArray != NULL) {
                        math::MTX34InvTranspose(&pCurTArray[i], &rVMtx);
                        math::MTX34ToMTX33(&rNMtx, &pCurTArray[i]);
                    } else {
                        math::MTX34InvTranspose(&rNMtx, &rVMtx);
                    }
                }
            }
        }

        if (pViewNrmArray != NULL) {
            ut::LC::StoreBlocks(&pViewNrmArray[baseMtx], pCurNArray,
                                numBlocksNrm);

            if (pViewTexMtxArray != NULL) {
                ut::LC::StoreBlocks(&pViewTexMtxArray[baseMtx], pCurTArray,
                                    numBlocks);
            }
        }

        std::swap(pCurMArray, pPrevMArray);
        std::swap(pCurVArray, pPrevVArray);
        std::swap(pCurNArray, pPrevNArray);
        std::swap(pCurTArray, pPrevTArray);
    }
}

} // namespace g3d
} // namespace nw4r
