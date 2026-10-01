#ifndef MTX_H
#define MTX_H

#include <types.h>
#include <macros.h>

#ifdef __cplusplus
extern "C" {
#endif

// CONFLICT (Petari): Petari's Vec is an anonymous struct and Vec2 lives in
// kpad.h. Our game code (nw4r::math::VEC2/VEC3 derive from these) uses the
// tagged structs below, with Vec2 declared here.
typedef struct Vec2 {
    f32 x, y;
} Vec2, *Vec2Ptr;

typedef struct Vec {
    f32 x, y, z;
} Vec, *VecPtr, Point3d;

typedef struct S16Vec {
    s16 x;
    s16 y;
    s16 z;
} S16Vec, *S16VecPtr;

typedef struct Quaternion {
    f32 x, y, z, w;
} Quaternion, *QuaternionPtr;

typedef f32 Mtx23[2][3];
typedef f32 (*Mtx23P)[3];

typedef f32 Mtx[3][4];
typedef f32 (*MtxPtr)[4];

typedef f32 Mtx44[4][4];
typedef f32 (*Mtx44Ptr)[4];

typedef f32 Mtx33[3][3];
typedef f32 (*Mtx3P)[3];

#define MTXDegToRad(a) ((a)*0.01745329252f)
#define MTXRadToDeg(a) ((a)*57.29577951f)

#define MTXRowCol(m, r, c) ((m)[(r)][(c)])

/* C Matrix */
void C_MTXIdentity(Mtx);
void C_MTXCopy(const Mtx, Mtx);
void C_MTXOrtho(Mtx44, f32, f32, f32, f32, f32, f32);
void C_MTXPerspective(Mtx44, f32, f32, f32, f32);

/* Paired Single Matrix */
void PSMTXIdentity(Mtx);
void PSMTXCopy(const Mtx, Mtx);
void PSMTXConcat(const Mtx, const Mtx, Mtx);
void PSMTXMultVecSR(const Mtx, const Vec*, Vec*);
void PSMTXMultVecArraySR(const Mtx, const Vec*, Vec*, u32);

u32 PSMTXInverse(const Mtx, Mtx);

void PSMTXMultVec(const Mtx, const Vec*, Vec*);

void PSMTXTransApply(const Mtx src, Mtx dst, f32 xT, f32 yT, f32 zT);
void PSMTXScaleApply(const Mtx, Mtx, f32, f32, f32);
void PSMTXScale(Mtx, f32, f32, f32);
void PSMTXRotRad(Mtx m, char axis, f32 rad) ATTRIBUTE_WEAK;
void PSMTXRotAxisRad(Mtx dst, const Vec*, f32);
void PSMTXRotTrig(Mtx, char, f32, f32);

void PSMTXTrans(Mtx m, f32 xT, f32 yT, f32 zT);
void PSMTX44Identity(Mtx44 m);
void PSMTX44Copy(const Mtx44 m, Mtx44 dst);

/* Paired Single Quaternion */
void PSQUATMultiply(const Quaternion*, const Quaternion*, Quaternion*);
void PSMTXQuat(Mtx dst, const Quaternion* quat);
f32 PSQUATDotProduct(const Quaternion*, const Quaternion*);

/* C Quaternion */
void C_QUATMtx(Quaternion* r, const Mtx m);
void C_QUATSlerp(const Quaternion* p, const Quaternion* q, Quaternion* r, f32 t);

/* C Vector */

f32 C_VECMag(const Vec*);

/* Paired Single Vector */
void PSVECAdd(const Vec*, const Vec*, Vec*);
void PSVECSubtract(const Vec*, const Vec*, Vec*);
void PSVECScale(const Vec*, Vec*, f32);
void PSVECNormalize(const Vec*, Vec*);

f32 PSVECMag(const Vec*);
f32 PSVECDotProduct(const Vec*, const Vec*);
void PSVECCrossProduct(const Vec*, const Vec*, Vec*);
f32 PSVECDistance(const Vec*, const Vec*);

void C_MTXLightPerspective(Mtx m, f32 fovY, f32 aspect, f32 scaleS, f32 scaleT, f32 transS, f32 transT);
void C_MTXLightOrtho(Mtx m, f32 t, f32 b, f32 l, f32 r, f32 scaleS, f32 scaleT, f32 transS, f32 transT);

/* Added for g3d (Task 10) */
void C_MTXLookAt(Mtx m, const Point3d* camPos, const Vec* camUp, const Point3d* target);
void C_MTXFrustum(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f);
void C_MTXLightFrustum(Mtx m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 scaleS, f32 scaleT, f32 transS,
                       f32 transT);
void PSMTXConcatArray(const Mtx a, const Mtx srcBase, Mtx dstBase, u32 count);
u32 PSMTXInvXpose(const Mtx src, Mtx inv);
void PSMTXMultVecArray(const Mtx m, const Vec* srcBase, Vec* dstBase, u32 count);
void PSMTXReorder(const Mtx src, f32 (*dest)[4]);
f32 PSVECSquareDistance(const Vec* a, const Vec* b);
f32 PSVECSquareMag(const Vec* v);

/* SMG1 uses the paired single versions of matrix / vector operations */
#define VECNormalize PSVECNormalize
#define VECMag PSVECMag

// Release-build SDK mappings (from our original mtx.h, plus the usual ones)
#define MTXIdentity PSMTXIdentity
#define MTXCopy PSMTXCopy
#define MTXConcat PSMTXConcat
#define MTXInverse PSMTXInverse
#define MTXTrans PSMTXTrans
#define MTXTransApply PSMTXTransApply
#define MTXScale PSMTXScale
#define MTXScaleApply PSMTXScaleApply
#define MTXMultVec PSMTXMultVec
#define MTXMultVecSR PSMTXMultVecSR
#define MTXQuat PSMTXQuat
#define MTXOrtho C_MTXOrtho
#define MTXPerspective C_MTXPerspective
#define VECAdd PSVECAdd
#define VECSubtract PSVECSubtract
#define VECScale PSVECScale
#define VECDotProduct PSVECDotProduct
#define VECCrossProduct PSVECCrossProduct
#define VECDistance PSVECDistance

#ifdef __cplusplus
}
#endif

#endif  // MTX_H
