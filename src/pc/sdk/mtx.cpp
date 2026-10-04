// MTX: matrix, vector and quaternion library.
//
// The SDK implements most of this library twice: C_MTX*() in C and PSMTX*() in
// paired-single assembly, and the release headers map the generic names to
// the PS versions. This file is the C side for both sets of names.
//
// - The PS* functions follow the algorithms of the SDK's C versions. What a
//   caller can observe of the assembly versions is kept: results may be
//   written over an operand, a singular matrix returns 0 and leaves the
//   output alone, PSMTXInvXpose and PSMTXTranspose clear the fourth column,
//   an unknown axis letter writes nothing.
// - The reciprocal and reciprocal-square-root estimates (fres, frsqrte and a
//   Newton step) are the exact operations here, and products are rounded
//   individually where the assembly fuses them (ps_madd). Results can differ
//   from the console in the last bit.
// - The functions that are C in the SDK as well (C_MTXLookAt, C_MTXFrustum,
//   C_QUATSlerp, ...) are the code of src/revolution/MTX.

#include <revolution/mtx.h>

#include <math.h>

extern "C" {

/******************************************************************************
 *
 * Mtx (3x4)
 *
 ******************************************************************************/
void C_MTXIdentity(Mtx m) {
    m[0][0] = 1.0f;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[0][3] = 0.0f;
    m[1][0] = 0.0f;
    m[1][1] = 1.0f;
    m[1][2] = 0.0f;
    m[1][3] = 0.0f;
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = 1.0f;
    m[2][3] = 0.0f;
}

void PSMTXIdentity(Mtx m) {
    C_MTXIdentity(m);
}

void C_MTXCopy(const Mtx src, Mtx dst) {
    if (src == dst) {
        return;
    }

    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            dst[r][c] = src[r][c];
        }
    }
}

void PSMTXCopy(const Mtx src, Mtx dst) {
    C_MTXCopy(src, dst);
}

void PSMTXConcat(const Mtx a, const Mtx b, Mtx ab) {
    Mtx tmp;

    for (int r = 0; r < 3; r++) {
        tmp[r][0] = a[r][0] * b[0][0] + a[r][1] * b[1][0] + a[r][2] * b[2][0];
        tmp[r][1] = a[r][0] * b[0][1] + a[r][1] * b[1][1] + a[r][2] * b[2][1];
        tmp[r][2] = a[r][0] * b[0][2] + a[r][1] * b[1][2] + a[r][2] * b[2][2];
        tmp[r][3] = a[r][0] * b[0][3] + a[r][1] * b[1][3] + a[r][2] * b[2][3] + a[r][3];
    }

    C_MTXCopy(tmp, ab);
}

void PSMTXConcatArray(const Mtx a, const Mtx* srcBase, Mtx* dstBase, u32 count) {
    for (u32 i = 0; i < count; i++) {
        PSMTXConcat(a, srcBase[i], dstBase[i]);
    }
}

void PSMTXTranspose(const Mtx src, Mtx xPose) {
    Mtx tmp;

    for (int r = 0; r < 3; r++) {
        tmp[r][0] = src[0][r];
        tmp[r][1] = src[1][r];
        tmp[r][2] = src[2][r];
        tmp[r][3] = 0.0f;
    }

    C_MTXCopy(tmp, xPose);
}

// The inverse of the upper 3x3 part of src in out (rows 0 to 2, columns 0 to
// 2), transposed if asked. Returns false if src is singular.
static bool PCMtxInverse33(const Mtx src, f32 out[3][3], bool transpose) {
    f32 det = src[0][0] * src[1][1] * src[2][2] + src[0][1] * src[1][2] * src[2][0] +
              src[0][2] * src[1][0] * src[2][1] - src[2][0] * src[1][1] * src[0][2] -
              src[1][0] * src[0][1] * src[2][2] - src[0][0] * src[2][1] * src[1][2];

    if (det == 0.0f) {
        return false;
    }

    det = 1.0f / det;

    f32 inv[3][3];
    inv[0][0] = (src[1][1] * src[2][2] - src[2][1] * src[1][2]) * det;
    inv[0][1] = -(src[0][1] * src[2][2] - src[2][1] * src[0][2]) * det;
    inv[0][2] = (src[0][1] * src[1][2] - src[1][1] * src[0][2]) * det;

    inv[1][0] = -(src[1][0] * src[2][2] - src[2][0] * src[1][2]) * det;
    inv[1][1] = (src[0][0] * src[2][2] - src[2][0] * src[0][2]) * det;
    inv[1][2] = -(src[0][0] * src[1][2] - src[1][0] * src[0][2]) * det;

    inv[2][0] = (src[1][0] * src[2][1] - src[2][0] * src[1][1]) * det;
    inv[2][1] = -(src[0][0] * src[2][1] - src[2][0] * src[0][1]) * det;
    inv[2][2] = (src[0][0] * src[1][1] - src[1][0] * src[0][1]) * det;

    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            out[r][c] = transpose ? inv[c][r] : inv[r][c];
        }
    }

    return true;
}

u32 PSMTXInverse(const Mtx src, Mtx inv) {
    f32 rot[3][3];

    if (!PCMtxInverse33(src, rot, false)) {
        return 0;
    }

    Mtx tmp;

    for (int r = 0; r < 3; r++) {
        tmp[r][0] = rot[r][0];
        tmp[r][1] = rot[r][1];
        tmp[r][2] = rot[r][2];
        tmp[r][3] = -rot[r][0] * src[0][3] - rot[r][1] * src[1][3] - rot[r][2] * src[2][3];
    }

    C_MTXCopy(tmp, inv);
    return 1;
}

u32 PSMTXInvXpose(const Mtx src, Mtx invX) {
    f32 rot[3][3];

    if (!PCMtxInverse33(src, rot, true)) {
        return 0;
    }

    for (int r = 0; r < 3; r++) {
        invX[r][0] = rot[r][0];
        invX[r][1] = rot[r][1];
        invX[r][2] = rot[r][2];
        invX[r][3] = 0.0f;
    }

    return 1;
}

void PSMTXRotTrig(Mtx m, char axis, f32 sinA, f32 cosA) {
    switch (axis | 0x20) {
    case 'x':
        m[0][0] = 1.0f;
        m[0][1] = 0.0f;
        m[0][2] = 0.0f;
        m[0][3] = 0.0f;
        m[1][0] = 0.0f;
        m[1][1] = cosA;
        m[1][2] = -sinA;
        m[1][3] = 0.0f;
        m[2][0] = 0.0f;
        m[2][1] = sinA;
        m[2][2] = cosA;
        m[2][3] = 0.0f;
        break;

    case 'y':
        m[0][0] = cosA;
        m[0][1] = 0.0f;
        m[0][2] = sinA;
        m[0][3] = 0.0f;
        m[1][0] = 0.0f;
        m[1][1] = 1.0f;
        m[1][2] = 0.0f;
        m[1][3] = 0.0f;
        m[2][0] = -sinA;
        m[2][1] = 0.0f;
        m[2][2] = cosA;
        m[2][3] = 0.0f;
        break;

    case 'z':
        m[0][0] = cosA;
        m[0][1] = -sinA;
        m[0][2] = 0.0f;
        m[0][3] = 0.0f;
        m[1][0] = sinA;
        m[1][1] = cosA;
        m[1][2] = 0.0f;
        m[1][3] = 0.0f;
        m[2][0] = 0.0f;
        m[2][1] = 0.0f;
        m[2][2] = 1.0f;
        m[2][3] = 0.0f;
        break;

    default:
        break;
    }
}

void PSMTXRotRad(Mtx m, char axis, f32 rad) {
    // The SDK calls the double-precision functions
    const f32 sinA = static_cast<f32>(sin(static_cast<f64>(rad)));
    const f32 cosA = static_cast<f32>(cos(static_cast<f64>(rad)));

    PSMTXRotTrig(m, axis, sinA, cosA);
}

void PSMTXRotAxisRad(Mtx m, const Vec* axis, f32 rad) {
    const f32 s = static_cast<f32>(sin(static_cast<f64>(rad)));
    const f32 c = static_cast<f32>(cos(static_cast<f64>(rad)));
    const f32 t = 1.0f - c;

    Vec n;
    PSVECNormalize(axis, &n);

    const f32 x = n.x;
    const f32 y = n.y;
    const f32 z = n.z;

    m[0][0] = t * x * x + c;
    m[0][1] = t * x * y - s * z;
    m[0][2] = t * x * z + s * y;
    m[0][3] = 0.0f;

    m[1][0] = t * x * y + s * z;
    m[1][1] = t * y * y + c;
    m[1][2] = t * y * z - s * x;
    m[1][3] = 0.0f;

    m[2][0] = t * x * z - s * y;
    m[2][1] = t * y * z + s * x;
    m[2][2] = t * z * z + c;
    m[2][3] = 0.0f;
}

void PSMTXTrans(Mtx m, f32 xT, f32 yT, f32 zT) {
    m[0][0] = 1.0f;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[0][3] = xT;
    m[1][0] = 0.0f;
    m[1][1] = 1.0f;
    m[1][2] = 0.0f;
    m[1][3] = yT;
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = 1.0f;
    m[2][3] = zT;
}

void PSMTXTransApply(const Mtx src, Mtx dst, f32 xT, f32 yT, f32 zT) {
    C_MTXCopy(src, dst);

    dst[0][3] = src[0][3] + xT;
    dst[1][3] = src[1][3] + yT;
    dst[2][3] = src[2][3] + zT;
}

void PSMTXScale(Mtx m, f32 xS, f32 yS, f32 zS) {
    m[0][0] = xS;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[0][3] = 0.0f;
    m[1][0] = 0.0f;
    m[1][1] = yS;
    m[1][2] = 0.0f;
    m[1][3] = 0.0f;
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = zS;
    m[2][3] = 0.0f;
}

void PSMTXScaleApply(const Mtx src, Mtx dst, f32 xS, f32 yS, f32 zS) {
    for (int c = 0; c < 4; c++) {
        dst[0][c] = src[0][c] * xS;
        dst[1][c] = src[1][c] * yS;
        dst[2][c] = src[2][c] * zS;
    }
}

void PSMTXQuat(Mtx m, const Quaternion* q) {
    const f32 s = 2.0f / (q->x * q->x + q->y * q->y + q->z * q->z + q->w * q->w);

    const f32 xs = q->x * s;
    const f32 ys = q->y * s;
    const f32 zs = q->z * s;

    const f32 wx = q->w * xs;
    const f32 wy = q->w * ys;
    const f32 wz = q->w * zs;

    const f32 xx = q->x * xs;
    const f32 xy = q->x * ys;
    const f32 xz = q->x * zs;

    const f32 yy = q->y * ys;
    const f32 yz = q->y * zs;
    const f32 zz = q->z * zs;

    m[0][0] = 1.0f - (yy + zz);
    m[0][1] = xy - wz;
    m[0][2] = xz + wy;
    m[0][3] = 0.0f;

    m[1][0] = xy + wz;
    m[1][1] = 1.0f - (xx + zz);
    m[1][2] = yz - wx;
    m[1][3] = 0.0f;

    m[2][0] = xz - wy;
    m[2][1] = yz + wx;
    m[2][2] = 1.0f - (xx + yy);
    m[2][3] = 0.0f;
}

// Column-major copy of src (the SDK's ROMtx, f32[4][3]): dest receives 12
// floats, column after column. The header declares dest as rows of 4.
void PSMTXReorder(const Mtx src, f32 (*dest)[4]) {
    f32* out = &dest[0][0];

    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 3; r++) {
            *out++ = src[r][c];
        }
    }
}

void C_MTXLookAt(Mtx m, const Point3d* camPos, const Vec* camUp, const Point3d* target) {
    Vec vLook, vRight, vUp;

    vLook.x = camPos->x - target->x;
    vLook.y = camPos->y - target->y;
    vLook.z = camPos->z - target->z;
    PSVECNormalize(&vLook, &vLook);
    PSVECCrossProduct(camUp, &vLook, &vRight);
    PSVECNormalize(&vRight, &vRight);
    PSVECCrossProduct(&vLook, &vRight, &vUp);

    m[0][0] = vRight.x;
    m[0][1] = vRight.y;
    m[0][2] = vRight.z;
    m[0][3] = -(camPos->x * vRight.x + camPos->y * vRight.y + camPos->z * vRight.z);
    m[1][0] = vUp.x;
    m[1][1] = vUp.y;
    m[1][2] = vUp.z;
    m[1][3] = -(camPos->x * vUp.x + camPos->y * vUp.y + camPos->z * vUp.z);
    m[2][0] = vLook.x;
    m[2][1] = vLook.y;
    m[2][2] = vLook.z;
    m[2][3] = -(camPos->x * vLook.x + camPos->y * vLook.y + camPos->z * vLook.z);
}

// 1 / tan(fovY / 2), fovY in degrees; the SDK calls the double-precision tan()
static f32 PCCotHalfFovY(f32 fovY) {
    f32 angle = fovY * 0.5f;
    angle = MTXDegToRad(angle);

    const f32 cot = static_cast<f32>(tan(static_cast<f64>(angle)));
    return 1.0f / cot;
}

void C_MTXLightFrustum(Mtx m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 scaleS, f32 scaleT, f32 transS,
                       f32 transT) {
    f32 tmp;

    tmp = 1 / (r - l);
    m[0][0] = (scaleS * (2 * n * tmp));
    m[0][1] = 0;
    m[0][2] = (scaleS * (tmp * (r + l))) - transS;
    m[0][3] = 0;
    tmp = 1 / (t - b);
    m[1][0] = 0;
    m[1][1] = (scaleT * (2 * n * tmp));
    m[1][2] = (scaleT * (tmp * (t + b))) - transT;
    m[1][3] = 0;
    m[2][0] = 0;
    m[2][1] = 0;
    m[2][2] = -1;
    m[2][3] = 0;
}

void C_MTXLightPerspective(Mtx m, f32 fovY, f32 aspect, f32 scaleS, f32 scaleT, f32 transS,
                           f32 transT) {
    const f32 cot = PCCotHalfFovY(fovY);

    m[0][0] = (cot / aspect) * scaleS;
    m[0][1] = 0.0f;
    m[0][2] = -transS;
    m[0][3] = 0.0f;
    m[1][0] = 0.0f;
    m[1][1] = cot * scaleT;
    m[1][2] = -transT;
    m[1][3] = 0.0f;
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = -1.0f;
    m[2][3] = 0.0f;
}

void C_MTXLightOrtho(Mtx m, f32 t, f32 b, f32 l, f32 r, f32 scaleS, f32 scaleT, f32 transS,
                     f32 transT) {
    f32 tmp;

    tmp = 1.0f / (r - l);
    m[0][0] = (2.0f * tmp * scaleS);
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[0][3] = ((-(r + l) * tmp) * scaleS) + transS;
    tmp = 1.0f / (t - b);
    m[1][0] = 0.0f;
    m[1][1] = (2.0f * tmp) * scaleT;
    m[1][2] = 0.0f;
    m[1][3] = ((-(t + b) * tmp) * scaleT) + transT;
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = 0.0f;
    m[2][3] = 1.0f;
}

/******************************************************************************
 *
 * Mtx times Vec
 *
 ******************************************************************************/
void PSMTXMultVec(const Mtx m, const Vec* src, Vec* dst) {
    const Vec v = *src;

    dst->x = m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z + m[0][3];
    dst->y = m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z + m[1][3];
    dst->z = m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3];
}

void PSMTXMultVecArray(const Mtx m, const Vec* srcBase, Vec* dstBase, u32 count) {
    for (u32 i = 0; i < count; i++) {
        PSMTXMultVec(m, &srcBase[i], &dstBase[i]);
    }
}

void PSMTXMultVecSR(const Mtx m, const Vec* src, Vec* dst) {
    const Vec v = *src;

    dst->x = m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z;
    dst->y = m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z;
    dst->z = m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z;
}

void PSMTXMultVecArraySR(const Mtx m, const Vec* srcBase, Vec* dstBase, u32 count) {
    for (u32 i = 0; i < count; i++) {
        PSMTXMultVecSR(m, &srcBase[i], &dstBase[i]);
    }
}

/******************************************************************************
 *
 * Mtx44
 *
 ******************************************************************************/
void PSMTX44Identity(Mtx44 m) {
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            m[r][c] = r == c ? 1.0f : 0.0f;
        }
    }
}

void PSMTX44Copy(const Mtx44 src, Mtx44 dst) {
    if (src == dst) {
        return;
    }

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            dst[r][c] = src[r][c];
        }
    }
}

void C_MTXFrustum(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f) {
    f32 tmp;

    tmp = 1 / (r - l);
    m[0][0] = (2 * n * tmp);
    m[0][1] = 0;
    m[0][2] = (tmp * (r + l));
    m[0][3] = 0;
    tmp = 1 / (t - b);
    m[1][0] = 0;
    m[1][1] = (2 * n * tmp);
    m[1][2] = (tmp * (t + b));
    m[1][3] = 0;
    m[2][0] = 0;
    m[2][1] = 0;
    tmp = 1 / (f - n);
    m[2][2] = (-n * tmp);
    m[2][3] = (tmp * -(f * n));
    m[3][0] = 0;
    m[3][1] = 0;
    m[3][2] = -1;
    m[3][3] = 0;
}

void C_MTXPerspective(Mtx44 m, f32 fovY, f32 aspect, f32 n, f32 f) {
    const f32 cot = PCCotHalfFovY(fovY);
    f32 tmp;

    m[0][0] = cot / aspect;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[0][3] = 0.0f;

    m[1][0] = 0.0f;
    m[1][1] = cot;
    m[1][2] = 0.0f;
    m[1][3] = 0.0f;

    m[2][0] = 0.0f;
    m[2][1] = 0.0f;

    tmp = 1.0f / (f - n);
    m[2][2] = -(n)*tmp;
    m[2][3] = -(f * n) * tmp;

    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = -1.0f;
    m[3][3] = 0.0f;
}

void C_MTXOrtho(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f) {
    f32 tmp;

    tmp = 1.0f / (r - l);
    m[0][0] = 2.0f * tmp;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[0][3] = -(r + l) * tmp;

    tmp = 1.0f / (t - b);
    m[1][0] = 0.0f;
    m[1][1] = 2.0f * tmp;
    m[1][2] = 0.0f;
    m[1][3] = -(t + b) * tmp;

    m[2][0] = 0.0f;
    m[2][1] = 0.0f;

    tmp = 1.0f / (f - n);
    m[2][2] = -(1.0f) * tmp;
    m[2][3] = -(f)*tmp;

    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;
}

/******************************************************************************
 *
 * Vec
 *
 ******************************************************************************/
void PSVECAdd(const Vec* a, const Vec* b, Vec* ab) {
    ab->x = a->x + b->x;
    ab->y = a->y + b->y;
    ab->z = a->z + b->z;
}

void PSVECSubtract(const Vec* a, const Vec* b, Vec* a_b) {
    a_b->x = a->x - b->x;
    a_b->y = a->y - b->y;
    a_b->z = a->z - b->z;
}

void PSVECScale(const Vec* src, Vec* dst, f32 scale) {
    dst->x = src->x * scale;
    dst->y = src->y * scale;
    dst->z = src->z * scale;
}

f32 PSVECSquareMag(const Vec* v) {
    return (v->x * v->x) + (v->y * v->y) + (v->z * v->z);
}

f32 C_VECMag(const Vec* v) {
    return sqrtf(PSVECSquareMag(v));
}

f32 PSVECMag(const Vec* v) {
    return sqrtf(PSVECSquareMag(v));
}

// A zero vector gives NaNs, as in the SDK (0 times an infinite reciprocal).
void PSVECNormalize(const Vec* src, Vec* unit) {
    const f32 mag = 1.0f / sqrtf(PSVECSquareMag(src));

    unit->x = src->x * mag;
    unit->y = src->y * mag;
    unit->z = src->z * mag;
}

f32 PSVECDotProduct(const Vec* a, const Vec* b) {
    return (a->x * b->x) + (a->y * b->y) + (a->z * b->z);
}

void PSVECCrossProduct(const Vec* a, const Vec* b, Vec* axb) {
    Vec tmp;

    tmp.x = (a->y * b->z) - (a->z * b->y);
    tmp.y = (a->z * b->x) - (a->x * b->z);
    tmp.z = (a->x * b->y) - (a->y * b->x);

    *axb = tmp;
}

f32 PSVECSquareDistance(const Vec* a, const Vec* b) {
    Vec diff;
    PSVECSubtract(a, b, &diff);

    return PSVECSquareMag(&diff);
}

f32 PSVECDistance(const Vec* a, const Vec* b) {
    return sqrtf(PSVECSquareDistance(a, b));
}

void C_VECHalfAngle(const Vec* a, const Vec* b, Vec* half) {
    Vec aTmp;
    Vec bTmp;
    Vec hTmp;

    aTmp.x = -a->x;
    aTmp.y = -a->y;
    aTmp.z = -a->z;
    bTmp.x = -b->x;
    bTmp.y = -b->y;
    bTmp.z = -b->z;

    PSVECNormalize(&aTmp, &aTmp);
    PSVECNormalize(&bTmp, &bTmp);
    PSVECAdd(&aTmp, &bTmp, &hTmp);

    if (PSVECDotProduct(&hTmp, &hTmp) > 0.0f) {
        PSVECNormalize(&hTmp, half);
        return;
    }

    *half = hTmp;
}

/******************************************************************************
 *
 * Quaternion
 *
 ******************************************************************************/
void PSQUATMultiply(const Quaternion* p, const Quaternion* q, Quaternion* pq) {
    Quaternion tmp;

    tmp.w = p->w * q->w - p->x * q->x - p->y * q->y - p->z * q->z;
    tmp.x = p->w * q->x + p->x * q->w + p->y * q->z - p->z * q->y;
    tmp.y = p->w * q->y + p->y * q->w + p->z * q->x - p->x * q->z;
    tmp.z = p->w * q->z + p->z * q->w + p->x * q->y - p->y * q->x;

    *pq = tmp;
}

f32 PSQUATDotProduct(const Quaternion* p, const Quaternion* q) {
    return p->x * q->x + p->y * q->y + p->z * q->z + p->w * q->w;
}

void C_QUATMtx(Quaternion* r, const Mtx m) {
    f32 tr, s;
    s32 i, j, k;
    s32 nxt[3] = {1, 2, 0};
    f32 q[3];

    tr = m[0][0] + m[1][1] + m[2][2];
    if (tr > 0.0f) {
        tr = static_cast<f32>(sqrt(static_cast<f64>(1.0f + tr)));
        r->w = tr * 0.5f;
        s = 0.5f / tr;
        r->x = (m[2][1] - m[1][2]) * s;
        r->y = (m[0][2] - m[2][0]) * s;
        r->z = (m[1][0] - m[0][1]) * s;
    } else {
        i = 0;
        if (m[1][1] > m[0][0]) {
            i = 1;
        }
        if (m[2][2] > m[i][i]) {
            i = 2;
        }
        j = nxt[i];
        k = nxt[j];
        s = static_cast<f32>(sqrt(static_cast<f64>((m[i][i] - (m[j][j] + m[k][k])) + 1.0f)));
        q[i] = s * 0.5f;

        if (s != 0.0f) {
            s = 0.5f / s;
        }

        r->w = (m[k][j] - m[j][k]) * s;
        q[j] = (m[i][j] + m[j][i]) * s;
        q[k] = (m[i][k] + m[k][i]) * s;

        r->x = q[0];
        r->y = q[1];
        r->z = q[2];
    }
}

void C_QUATSlerp(const Quaternion* p, const Quaternion* q, Quaternion* r, f32 t) {
    f32 theta, sin_th, cos_th, tp, tq, sin_1mtth, sin_tth;

    cos_th = p->x * q->x + p->y * q->y + p->z * q->z + p->w * q->w;
    tq = 1.0f;

    if (cos_th < 0.0f) {
        cos_th = -cos_th;
        tq = -tq;
    }

    if (cos_th <= 1.0f - 0.00001f) {
        theta = static_cast<f32>(acos(static_cast<f64>(cos_th)));
        sin_th = static_cast<f32>(sin(static_cast<f64>(theta)));
        tp = 1.0f - t;
        sin_1mtth = static_cast<f32>(sin(static_cast<f64>(tp * theta)));
        tp = sin_1mtth / sin_th;
        sin_tth = static_cast<f32>(sin(static_cast<f64>(t * theta)));
        tq *= sin_tth / sin_th;
    } else {
        tp = 1.0f - t;
        tq = tq * t;
    }

    r->x = tp * p->x + tq * q->x;
    r->y = tp * p->y + tq * q->y;
    r->z = tp * p->z + tq * q->z;
    r->w = tp * p->w + tq * q->w;
}

} // extern "C"
