// Self-test of the MTX backend (src/pc/sdk/mtx.cpp): results against values
// worked out by hand. Called from RunSelfTest() in main.cpp.

#include <cmath>

#include <revolution/mtx.h>

#include <nw4r/math.h>

#include "pc_selftest.h"

namespace {

bool Near(f32 a, f32 b, f32 eps = 1e-5f) {
    return std::fabs(a - b) <= eps;
}

bool NearVec(const Vec& v, f32 x, f32 y, f32 z) {
    return Near(v.x, x) && Near(v.y, y) && Near(v.z, z);
}

bool NearQuat(const Quaternion& q, f32 x, f32 y, f32 z, f32 w) {
    return Near(q.x, x) && Near(q.y, y) && Near(q.z, z) && Near(q.w, w);
}

bool NearMtx(const Mtx m, const f32 expected[3][4]) {
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            if (!Near(m[r][c], expected[r][c])) {
                return false;
            }
        }
    }
    return true;
}

const f32 kPi = 3.14159265358979f;
const f32 kSqrtHalf = 0.70710678f;

// A quarter turn about z: x goes to y, y goes to -x
const f32 kRotZ90[3][4] = {
    {0.0f, -1.0f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f, 0.0f},
};

} // namespace

void PCSelfTestMtx() {
    Mtx trans, scale, m;

    // Identity, translation, scale
    PSMTXIdentity(m);
    const f32 identity[3][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}};
    PC_CHECK(NearMtx(m, identity));

    PSMTXTrans(trans, 1.0f, 2.0f, 3.0f);
    PSMTXScale(scale, 2.0f, 3.0f, 4.0f);

    // Concat: first scale, then translate; and the other way round
    const f32 transScale[3][4] = {{2, 0, 0, 1}, {0, 3, 0, 2}, {0, 0, 4, 3}};
    const f32 scaleTrans[3][4] = {{2, 0, 0, 2}, {0, 3, 0, 6}, {0, 0, 4, 12}};
    PSMTXConcat(trans, scale, m);
    PC_CHECK(NearMtx(m, transScale));
    PSMTXConcat(scale, trans, m);
    PC_CHECK(NearMtx(m, scaleTrans));

    // The result may be one of the operands
    PSMTXCopy(trans, m);
    PSMTXConcat(m, scale, m);
    PC_CHECK(NearMtx(m, transScale));
    PSMTXCopy(scale, m);
    PSMTXConcat(trans, m, m);
    PC_CHECK(NearMtx(m, transScale));

    Mtx array[2], arrayOut[2];
    PSMTXCopy(scale, array[0]);
    PSMTXCopy(trans, array[1]);
    PSMTXConcatArray(scale, array, arrayOut, 2);
    const f32 scaleScale[3][4] = {{4, 0, 0, 0}, {0, 9, 0, 0}, {0, 0, 16, 0}};
    PC_CHECK(NearMtx(arrayOut[0], scaleScale) && NearMtx(arrayOut[1], scaleTrans));

    PSMTXScaleApply(trans, m, 2.0f, 3.0f, 4.0f);
    PC_CHECK(NearMtx(m, scaleTrans));
    PSMTXTransApply(scale, m, 1.0f, 2.0f, 3.0f);
    PC_CHECK(NearMtx(m, transScale));

    // Inverse of "scale, then translate": undo the translation, then the scale
    const f32 inverse[3][4] = {
        {0.5f, 0, 0, -0.5f},
        {0, 1.0f / 3.0f, 0, -2.0f / 3.0f},
        {0, 0, 0.25f, -0.75f},
    };
    Mtx inv;
    PC_CHECK(PSMTXInverse(m, inv) == 1);
    PC_CHECK(NearMtx(inv, inverse));
    PC_CHECK(PSMTXInverse(m, m) == 1); // in place
    PC_CHECK(NearMtx(m, inverse));

    // A singular matrix returns 0 and leaves the output alone
    Mtx singular;
    PSMTXScale(singular, 1.0f, 0.0f, 1.0f);
    PC_CHECK(PSMTXInverse(singular, inv) == 0);
    PC_CHECK(NearMtx(inv, inverse));
    PC_CHECK(PSMTXInvXpose(singular, inv) == 0);
    PC_CHECK(NearMtx(inv, inverse));

    // Inverse transpose: the translation is dropped
    const f32 general[3][4] = {{1, 2, 0, 5}, {0, 1, 0, 6}, {0, 0, 2, 7}};
    const f32 generalInvXpose[3][4] = {{1, 0, 0, 0}, {-2, 1, 0, 0}, {0, 0, 0.5f, 0}};
    const f32 generalXpose[3][4] = {{1, 0, 0, 0}, {2, 1, 0, 0}, {0, 0, 2, 0}};
    PSMTXCopy(general, m);
    PC_CHECK(PSMTXInvXpose(m, inv) == 1);
    PC_CHECK(NearMtx(inv, generalInvXpose));
    PSMTXTranspose(m, m);
    PC_CHECK(NearMtx(m, generalXpose));

    f32 reordered[3][4];
    PSMTXReorder(general, reordered);
    const f32* flat = &reordered[0][0];
    PC_CHECK(flat[0] == 1 && flat[1] == 0 && flat[2] == 0 && flat[3] == 2 && flat[4] == 1 &&
             flat[8] == 2 && flat[9] == 5 && flat[10] == 6 && flat[11] == 7);

    // Rotations: the three ways to make a quarter turn about z agree
    PSMTXRotRad(m, 'z', kPi / 2.0f);
    PC_CHECK(NearMtx(m, kRotZ90));
    PSMTXRotTrig(m, 'Z', 1.0f, 0.0f);
    PC_CHECK(NearMtx(m, kRotZ90));

    const Vec axisZ = {0.0f, 0.0f, 2.0f}; // normalised by the function
    PSMTXRotAxisRad(m, &axisZ, kPi / 2.0f);
    PC_CHECK(NearMtx(m, kRotZ90));

    const Quaternion quatZ90 = {0.0f, 0.0f, kSqrtHalf, kSqrtHalf};
    PSMTXQuat(m, &quatZ90);
    PC_CHECK(NearMtx(m, kRotZ90));

    const f32 rotX90[3][4] = {{1, 0, 0, 0}, {0, 0, -1, 0}, {0, 1, 0, 0}};
    const f32 rotY90[3][4] = {{0, 0, 1, 0}, {0, 1, 0, 0}, {-1, 0, 0, 0}};
    PSMTXRotTrig(m, 'x', 1.0f, 0.0f);
    PC_CHECK(NearMtx(m, rotX90));
    PSMTXRotTrig(m, 'y', 1.0f, 0.0f);
    PC_CHECK(NearMtx(m, rotY90));
    PSMTXRotTrig(m, 'w', 1.0f, 0.0f); // unknown axis: nothing is written
    PC_CHECK(NearMtx(m, rotY90));

    // Matrix times vector
    Vec v = {1.0f, 2.0f, 3.0f};
    Vec out;
    PSMTXConcat(trans, scale, m);
    PSMTXMultVec(m, &v, &out);
    PC_CHECK(NearVec(out, 3.0f, 8.0f, 15.0f));
    PSMTXMultVecSR(m, &v, &out);
    PC_CHECK(NearVec(out, 2.0f, 6.0f, 12.0f));
    PSMTXMultVec(m, &v, &v); // in place
    PC_CHECK(NearVec(v, 3.0f, 8.0f, 15.0f));

    const Vec points[2] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    Vec pointsOut[2];
    PSMTXMultVecArray(m, points, pointsOut, 2);
    PC_CHECK(NearVec(pointsOut[0], 3.0f, 2.0f, 3.0f) && NearVec(pointsOut[1], 1.0f, 5.0f, 3.0f));
    PSMTXMultVecArraySR(m, points, pointsOut, 2);
    PC_CHECK(NearVec(pointsOut[0], 2.0f, 0.0f, 0.0f) && NearVec(pointsOut[1], 0.0f, 3.0f, 0.0f));

    // Vectors
    const Vec a = {1.0f, 2.0f, 3.0f};
    const Vec b = {4.0f, 5.0f, 6.0f};
    PSVECAdd(&a, &b, &out);
    PC_CHECK(NearVec(out, 5.0f, 7.0f, 9.0f));
    PSVECSubtract(&a, &b, &out);
    PC_CHECK(NearVec(out, -3.0f, -3.0f, -3.0f));
    PSVECScale(&a, &out, 2.0f);
    PC_CHECK(NearVec(out, 2.0f, 4.0f, 6.0f));
    PC_CHECK(PSVECDotProduct(&a, &b) == 32.0f);
    PSVECCrossProduct(&a, &b, &out);
    PC_CHECK(NearVec(out, -3.0f, 6.0f, -3.0f));

    const Vec v3412 = {3.0f, 4.0f, 12.0f};
    const Vec zero = {0.0f, 0.0f, 0.0f};
    PC_CHECK(PSVECMag(&v3412) == 13.0f && C_VECMag(&v3412) == 13.0f);
    PC_CHECK(PSVECSquareMag(&v3412) == 169.0f);
    PC_CHECK(PSVECMag(&zero) == 0.0f);
    PC_CHECK(PSVECDistance(&v3412, &zero) == 13.0f && PSVECSquareDistance(&zero, &v3412) == 169.0f);

    const Vec v034 = {0.0f, 3.0f, 4.0f};
    PSVECNormalize(&v034, &out);
    PC_CHECK(NearVec(out, 0.0f, 0.6f, 0.8f));

    // Half angle: between the reversed directions; opposite vectors give zero
    const Vec fromX = {-1.0f, 0.0f, 0.0f};
    const Vec fromY = {0.0f, -2.0f, 0.0f};
    const Vec toX = {1.0f, 0.0f, 0.0f};
    C_VECHalfAngle(&fromX, &fromY, &out);
    PC_CHECK(NearVec(out, kSqrtHalf, kSqrtHalf, 0.0f));
    C_VECHalfAngle(&fromX, &toX, &out);
    PC_CHECK(NearVec(out, 0.0f, 0.0f, 0.0f));

    // Quaternions: i * j = k, and a slerp half way to a quarter turn
    const Quaternion qi = {1.0f, 0.0f, 0.0f, 0.0f};
    const Quaternion qj = {0.0f, 1.0f, 0.0f, 0.0f};
    const Quaternion qIdentity = {0.0f, 0.0f, 0.0f, 1.0f};
    Quaternion q;
    PSQUATMultiply(&qi, &qj, &q);
    PC_CHECK(NearQuat(q, 0.0f, 0.0f, 1.0f, 0.0f));
    PSQUATMultiply(&qj, &qi, &q);
    PC_CHECK(NearQuat(q, 0.0f, 0.0f, -1.0f, 0.0f));
    PC_CHECK(PSQUATDotProduct(&quatZ90, &qIdentity) == kSqrtHalf);

    C_QUATSlerp(&qIdentity, &quatZ90, &q, 0.5f);
    PC_CHECK(NearQuat(q, 0.0f, 0.0f, 0.38268343f, 0.92387953f)); // sin, cos of 22.5 degrees
    C_QUATSlerp(&qIdentity, &qIdentity, &q, 0.25f);
    PC_CHECK(NearQuat(q, 0.0f, 0.0f, 0.0f, 1.0f));

    C_QUATMtx(&q, kRotZ90); // trace > 0
    PC_CHECK(NearQuat(q, 0.0f, 0.0f, kSqrtHalf, kSqrtHalf));
    const f32 rotZ180[3][4] = {{-1, 0, 0, 0}, {0, -1, 0, 0}, {0, 0, 1, 0}};
    C_QUATMtx(&q, rotZ180); // trace < 0
    PC_CHECK(NearQuat(q, 0.0f, 0.0f, 1.0f, 0.0f));

    // Camera and projection matrices
    const Point3d camPos = {0.0f, 0.0f, 5.0f};
    const Vec camUp = {0.0f, 1.0f, 0.0f};
    const Point3d target = {0.0f, 0.0f, 0.0f};
    const f32 lookAt[3][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, -5}};
    C_MTXLookAt(m, &camPos, &camUp, &target);
    PC_CHECK(NearMtx(m, lookAt));

    Mtx44 proj;
    C_MTXOrtho(proj, 0.0f, 480.0f, 0.0f, 640.0f, 0.0f, 1.0f);
    PC_CHECK(Near(proj[0][0], 2.0f / 640.0f) && Near(proj[0][3], -1.0f));
    PC_CHECK(Near(proj[1][1], -2.0f / 480.0f) && Near(proj[1][3], 1.0f));
    PC_CHECK(proj[2][2] == -1.0f && proj[2][3] == -1.0f && proj[3][3] == 1.0f && proj[3][2] == 0.0f);

    C_MTXPerspective(proj, 90.0f, 2.0f, 1.0f, 3.0f);
    PC_CHECK(Near(proj[0][0], 0.5f) && Near(proj[1][1], 1.0f));
    PC_CHECK(proj[2][2] == -0.5f && proj[2][3] == -1.5f && proj[3][2] == -1.0f && proj[3][3] == 0.0f);

    C_MTXFrustum(proj, 1.0f, -1.0f, -2.0f, 2.0f, 1.0f, 3.0f);
    PC_CHECK(proj[0][0] == 0.5f && proj[1][1] == 1.0f && proj[0][2] == 0.0f && proj[1][2] == 0.0f);
    PC_CHECK(proj[2][2] == -0.5f && proj[2][3] == -1.5f && proj[3][2] == -1.0f);

    Mtx44 projCopy;
    PSMTX44Copy(proj, projCopy);
    PC_CHECK(projCopy[2][3] == -1.5f && projCopy[3][2] == -1.0f);
    PSMTX44Identity(proj);
    PC_CHECK(proj[0][0] == 1.0f && proj[3][3] == 1.0f && proj[2][3] == 0.0f && proj[3][2] == 0.0f);

    // Texture projection: scale 0.5 and offset 0.5 map -1..1 to 0..1
    C_MTXLightOrtho(m, 1.0f, -1.0f, -2.0f, 2.0f, 0.5f, 0.5f, 0.5f, 0.5f);
    const f32 lightOrtho[3][4] = {{0.25f, 0, 0, 0.5f}, {0, 0.5f, 0, 0.5f}, {0, 0, 0, 1}};
    PC_CHECK(NearMtx(m, lightOrtho));
    C_MTXLightPerspective(m, 90.0f, 2.0f, 0.5f, 0.5f, 0.5f, 0.5f);
    const f32 lightPersp[3][4] = {{0.25f, 0, -0.5f, 0}, {0, 0.5f, -0.5f, 0}, {0, 0, -1, 0}};
    PC_CHECK(NearMtx(m, lightPersp));
    C_MTXLightFrustum(m, 1.0f, -1.0f, -2.0f, 2.0f, 1.0f, 0.5f, 0.5f, 0.5f, 0.5f);
    PC_CHECK(NearMtx(m, lightPersp));

    // nw4r::math reaches the same backend
    nw4r::math::MTX34 n1, n2, n3;
    nw4r::math::MTX34Identity(&n1);
    PSMTXCopy(trans, n2.mtx);
    nw4r::math::MTX34Mult(&n3, &n2, &n1);
    PC_CHECK(n3._00 == 1.0f && n3._03 == 1.0f && n3._13 == 2.0f && n3._23 == 3.0f);
    PC_CHECK(nw4r::math::MTX34Inv(&n1, &n3) == 1);
    PC_CHECK(n1._03 == -1.0f && n1._13 == -2.0f && n1._23 == -3.0f);
}
