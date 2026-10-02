#include <nw4r/ef.h>
#include <nw4r/math.h>

// No reference source; written from the DOL (News Channel, older NW4R).

namespace nw4r {
namespace ef {

static u8 directional_tex0_u8[] ATTRIBUTE_ALIGN(32) = {
    0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01};

DrawDirectionalStrategy::DrawDirectionalStrategy() {}

static void DrawQuad(const math::MTX34& rMtx, const math::_VEC3* pPosArray,
                     bool texCoord) {

    math::VEC3 p0, p1, p2, p3;

    math::VEC3Transform(&p0, &rMtx,
                        static_cast<const math::VEC3*>(&pPosArray[0]));
    math::VEC3Transform(&p1, &rMtx,
                        static_cast<const math::VEC3*>(&pPosArray[1]));
    math::VEC3Transform(&p2, &rMtx,
                        static_cast<const math::VEC3*>(&pPosArray[2]));
    math::VEC3Transform(&p3, &rMtx,
                        static_cast<const math::VEC3*>(&pPosArray[3]));

    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    {
        GXPosition(p0);
        if (texCoord) {
            GXTexCoord1x8(0);
        }

        GXPosition(p1);
        if (texCoord) {
            GXTexCoord1x8(1);
        }

        GXPosition(p2);
        if (texCoord) {
            GXTexCoord1x8(2);
        }

        GXPosition(p3);
        if (texCoord) {
            GXTexCoord1x8(3);
        }
    }
    GXEnd();
}

static inline math::VEC3 GetColumn(const math::MTX34& rMtx, int col) {
    return math::VEC3(rMtx.m[0][col], rMtx.m[1][col], rMtx.m[2][col]);
}

static inline math::MTX34 CalcRotate(Particle* pParticle, u8 axis) {
    math::VEC3 rRot;
    pParticle->Draw_GetRotate(&rRot);

    f32 sx = 0.0f;
    f32 cx = 1.0f;
    f32 sy = 0.0f;
    f32 cy = 1.0f;
    f32 sz = 0.0f;
    f32 cz = 1.0f;

    switch (axis) {
    case 0: {
        if (rRot.x != 0.0f) {
            math::SinCosRad(&sx, &cx, -rRot.x);
        }

        // clang-format off
        return math::MTX34(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f,   cx,  -sx, 0.0f,
            0.0f,   sx,   cx, 0.0f);
        // clang-format on
    }

    case 1: {
        if (rRot.y != 0.0f) {
            math::SinCosRad(&sy, &cy, -rRot.y);
        }

        // clang-format off
        return math::MTX34(
              cy, 0.0f,   sy, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
             -sy, 0.0f,   cy, 0.0f);
        // clang-format on
    }

    case 2: {
        if (rRot.z != 0.0f) {
            math::SinCosRad(&sz, &cz, -rRot.z);
        }

        // clang-format off
        return math::MTX34(
              cz,  -sz, 0.0f, 0.0f,
              sz,   cz, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f);
        // clang-format on
    }

    case 3:
    default: {
        if (rRot.x) {
            math::SinCosRad(&sx, &cx, -rRot.x);
        }

        if (rRot.y) {
            math::SinCosRad(&sy, &cy, -rRot.y);
        }

        if (rRot.z) {
            math::SinCosRad(&sz, &cz, -rRot.z);
        }

        f32 sx_sy = sx * sy;
        f32 cx_sy = cx * sy;

        // clang-format off
        return math::MTX34(
            cy * cz,  sx_sy * cz - cx * sz,  cx_sy * cz + sx * sz,  0.0f,
            cy * sz,  sx_sy * sz + cx * cz,  cx_sy * sz - sx * cz,  0.0f,
                -sy,               cy * sx,               cx * cy,  0.0f);
        // clang-format on
    }
    }
}

void DrawDirectionalStrategy::Draw(const DrawInfo& rInfo,
                                   ParticleManager* pManager) {

    InitGraphics(rInfo, pManager);

    EmitterDesc* pDesc = pManager->mResource->GetEmitterDesc();
    const EmitterDrawSetting& rSetting = pDesc->drawSetting;

    AheadContext context(*rInfo.GetViewMtx(), pManager);
    CalcAheadFunc pCalcAheadFunc = GetCalcAheadFunc(pManager);

    math::MTX34 glbMtx;
    math::MTX34 posMtx;

    pManager->CalcGlobalMtx(&glbMtx);
    math::MTX34Mult(&posMtx, rInfo.GetViewMtx(), &glbMtx);
    GXLoadPosMtxImm(posMtx, GX_PNMTX0);

    math::MTX34 emitterMtx;
    pManager->mManagerEM->CalcGlobalMtx(&emitterMtx);

    math::MTX34 glbMtxInv;
    math::MTX34Inv(&glbMtxInv, &glbMtx);
    math::MTX34Mult(&emitterMtx, &glbMtxInv, &emitterMtx);

    math::VEC3 emitterAxisY;
    math::VEC3 emitterAxisZ;
    emitterAxisY = math::VEC3(emitterMtx._01, emitterMtx._11, emitterMtx._21);
    emitterAxisZ = math::VEC3(emitterMtx._02, emitterMtx._12, emitterMtx._22);
    Normalize(&emitterAxisZ);

    for (Particle* pIt = GetYoungestParticle(pManager);
         pIt != NULL && pIt->mPrevAxis.x > 1.0f;
         pIt = GetElderParticle(pManager, pIt)) {

        pIt->mPrevAxis = emitterAxisY;
    }

    f32 px = pDesc->pivotX / 100.0f;
    f32 py = pDesc->pivotY / 100.0f;

    GetFirstDrawParticleFunc pGetFirstFunc = GetGetFirstDrawParticleFunc(
        pDesc->drawSetting.mFlags & EmitterDrawSetting::FLAG_DRAW_ORDER);

    GetNextDrawParticleFunc pGetNextFunc = GetGetNextDrawParticleFunc(
        pDesc->drawSetting.mFlags & EmitterDrawSetting::FLAG_DRAW_ORDER);

    bool first = true;

    for (Particle* pIt = pGetFirstFunc(pManager); pIt != NULL;
         pIt = pGetNextFunc(pManager, pIt)) {

        f32 sx = pIt->Draw_GetSizeX();
        if (sx < NW4R_MATH_FLT_EPSILON) {
            continue;
        }

        f32 sy = pIt->Draw_GetSizeY();
        if (sy < NW4R_MATH_FLT_EPSILON) {
            continue;
        }

        f32 stretch = 1.0f;

        if (pDesc->typeOption0 != 0) {
            math::VEC3 dir;
            pIt->GetMoveDir(&dir);
            stretch += 0.5f * math::FSqrt(math::VEC3Dot(&dir, &dir)) / sy;
        }

        SetupGP(pIt, rSetting, rInfo, first, false);
        first = false;

        math::MTX34 rotMtx = CalcRotate(pIt, pDesc->typeAxis);
        math::MTX34 locMtx;

        switch (pDesc->typeOption1) {
        case 0: {
            f32 sx_px = sx * px;
            f32 sy_py = sy * (py + stretch - 1.0f);
            f32 sy2 = sy * stretch;

            locMtx._00 = rotMtx._00 * sx;
            locMtx._01 = rotMtx._01 * sy2;
            locMtx._02 = rotMtx._02 * sx;
            locMtx._03 = -rotMtx._00 * sx_px - rotMtx._01 * sy_py + px;
            locMtx._10 = rotMtx._10 * sx;
            locMtx._11 = rotMtx._11 * sy2;
            locMtx._12 = rotMtx._12 * sx;
            locMtx._13 = -rotMtx._10 * sx_px - rotMtx._11 * sy_py + py;
            locMtx._20 = rotMtx._20 * sx;
            locMtx._21 = rotMtx._21 * sy2;
            locMtx._22 = rotMtx._22 * sx;
            locMtx._23 = -rotMtx._20 * sx_px - rotMtx._21 * sy_py;
            break;
        }

        case 1:
        default: {
            f32 sx_px = sx * px;
            f32 sy_py = sy * py;

            locMtx._00 = rotMtx._00 * sx;
            locMtx._01 = rotMtx._02 * sy;
            locMtx._02 = rotMtx._01 * sx;
            locMtx._03 = -rotMtx._00 * sx_px - rotMtx._02 * sy_py + px;
            locMtx._10 = rotMtx._10 * sx;
            locMtx._11 = rotMtx._12 * sy;
            locMtx._12 = rotMtx._11 * sx;
            locMtx._13 = -rotMtx._10 * sx_px - rotMtx._12 * sy_py;
            locMtx._20 = rotMtx._20 * sx;
            locMtx._21 = rotMtx._22 * sy;
            locMtx._22 = rotMtx._21 * sx;
            locMtx._23 = -rotMtx._20 * sx_px - rotMtx._22 * sy_py - py;
            break;
        }
        }

        math::VEC3 axisX;
        math::VEC3 axisY;
        math::VEC3 axisZ;

        pCalcAheadFunc(&axisY, &context, pIt);
        math::VEC3Cross(&axisZ, &pIt->mPrevAxis, &axisY);

        if (!Normalize(&axisZ)) {
            axisZ = emitterAxisZ;
        }

        math::VEC3Cross(&axisX, &axisY, &axisZ);

        pIt->mPrevAxis = axisX;

        math::VEC3 pos = pIt->mParameter.mPosition;

        // clang-format off
        math::MTX34 dirMtx(
            axisX.x, axisY.x, axisZ.x, pos.x,
            axisX.y, axisY.y, axisZ.y, pos.y,
            axisX.z, axisY.z, axisZ.z, pos.z);
        // clang-format on

        math::MTX34 drawMtx;
        math::MTX34Mult(&drawMtx, &dirMtx, &locMtx);

        // clang-format off
        static const math::_VEC3 p[4] = {
            -1.0f, -1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f, 0.0f,
             1.0f, -1.0f, 0.0f
        };
        // clang-format on

        DrawQuad(drawMtx, p, mNumTexmap > 0);

        if (pDesc->typeOption == EmitterDrawSetting::TYPE_CMN_CROSS) {
            // clang-format off
            static const math::_VEC3 px[4] = {
                0.0f, -1.0f,  1.0f,
                0.0f,  1.0f,  1.0f,
                0.0f,  1.0f, -1.0f,
                0.0f, -1.0f, -1.0f
            };
            // clang-format on

            DrawQuad(drawMtx, px, mNumTexmap > 0);
        }
    }
}

DrawStrategyImpl::CalcAheadFunc
DrawDirectionalStrategy::GetCalcAheadFunc(ParticleManager* pManager) {

    switch (pManager->mResource->GetEmitterDesc()->typeDir) {
    case EmitterDrawSetting::AHEAD_BB_SPEED: {
        return CalcAhead_Speed;
    }

    case EmitterDrawSetting::AHEAD_BB_EMITTER_CENTER: {
        return CalcAhead_EmitterCenter;
    }

    case EmitterDrawSetting::AHEAD_BB_EMITTER_DESIGN: {
        return CalcAhead_EmitterDesign;
    }

    case EmitterDrawSetting::AHEAD_BB_PARTICLE: {
        return CalcAhead_Particle;
    }

    case EmitterDrawSetting::AHEAD_CMN_NODESIGN: {
        return CalcAhead_NoDesign;
    }

    default: {
        return CalcAhead_Speed;
    }
    }
}

void DrawDirectionalStrategy::InitGraphics(const DrawInfo& rInfo,
                                           ParticleManager* pManager) {

    EmitterDesc* pDesc = pManager->mResource->GetEmitterDesc();
    const EmitterDrawSetting& rSetting = pDesc->drawSetting;

    InitTexture(rSetting);
    InitTev(rSetting, rInfo);
    InitColor(pManager, rSetting, rInfo);

    GXEnableTexOffsets(GX_TEXCOORD0, TRUE, TRUE);

    GXSetArray(GX_VA_TEX0, directional_tex0_u8, 2);

    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);

    if (mNumTexmap > 0) {
        GXSetVtxDesc(GX_VA_TEX0, GX_INDEX8);
    }

    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_U8, 0);

    GXSetCurrentMtx(GX_PNMTX0);
}

} // namespace ef
} // namespace nw4r
