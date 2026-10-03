#include <nw4r/ef.h>
#include <nw4r/math.h>

#include <revolution/GX.h>

// Older revision (News Channel). Written from the DOL: no reference
// decompilation has ef_drawstripestrategy.cpp. The function names come from
// the Wii Sports symbol map (ogws), which has a newer version of this file
// (tubes, connect types, smooth stripes).

namespace nw4r {
namespace ef {

DrawStripeStrategy::DrawStripeStrategy() {}

static void GXEnd();

// Builds the particle's local axes from the ahead direction and the previous
// axis (shared by DrawParticle and Draw)
inline math::MTX34
DrawStripeStrategy::CalcStripeMtx(Particle* pParticle,
              AheadContextStripe* pContext,
              const math::VEC3& rAhead, const math::VEC3& rPos,
              math::VEC3* pPrevAxis) {
    math::VEC3 axisX;
    math::VEC3Cross(&axisX, &rAhead, &pParticle->mPrevAxis);

    if (!Normalize(&axisX)) {
        axisX = pContext->mEmitterAxisX;
    }

    math::VEC3 axisZ;
    math::VEC3Cross(&axisZ, &axisX, &rAhead);
    Normalize(&axisZ);

    pParticle->mPrevAxis = axisZ;
    *pPrevAxis = axisZ;

    return math::MTX34(axisX.x, rAhead.x, axisZ.x, rPos.x, axisX.y, rAhead.y,
                       axisZ.y, rPos.y, axisX.z, rAhead.z, axisZ.z, rPos.z);
}

// Rotation about the stripe's Y axis around the pivot, scaled by the size
inline math::MTX34 DrawStripeStrategy::CalcRotateMtx(Particle* pParticle,
                                                     f32 pivot) {
    math::VEC3 rot;
    pParticle->Draw_GetRotate(&rot);

    f32 size = pParticle->Draw_GetSizeX();
    f32 c = size * math::CosRad(rot.y);
    f32 s = size * math::SinRad(rot.y);

    return math::MTX34(c, 0.0f, -s, pivot - c * pivot, 0.0f, 1.0f, 0.0f, 0.0f,
                       s, 0.0f, c, -s * pivot);
}

#pragma push
#pragma dont_inline on
void DrawStripeStrategy::DrawStripe(AheadContextStripe* pContext,
                                    const math::VEC3& rVtx0,
                                    const math::VEC3& rVtx1) {
    ParticleManager* pManager = pContext->mCommon.mParticleManager;
    EmitterDesc* pDesc = pManager->mResource->GetEmitterDesc();

    CalcAheadFunc pCalcAheadFunc = GetCalcAheadFunc(pManager);

    int drawOrder =
        pDesc->drawSetting.mFlags & EmitterDrawSetting::FLAG_DRAW_ORDER;

    GetFirstDrawParticleFunc pGetFirstFunc =
        GetGetFirstDrawParticleFunc(drawOrder);
    GetNextDrawParticleFunc pGetNextFunc =
        GetGetNextDrawParticleFunc(drawOrder);

    int numVtx = pManager->mActivityList.GetNumActiveCount();

    f32 pivot = 0.01f * pDesc->pivotX;

    math::VEC3 prevAxis(pContext->mCommon.mEmitterAxisY);

    bool connectRing = (pDesc->typeOption2 & 7) == 1;
    bool connectEmitter = (pDesc->typeOption2 & 7) == 2;

    if (connectRing || connectEmitter) {
        numVtx++;
    }

    bool useTex = mNumTexmap != 0;

    GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, static_cast<u16>(numVtx * 2));

    f32 texStep = 1.0f / (numVtx - 1);

    int idx;
    int step;

    switch (pDesc->drawSetting.mFlags & EmitterDrawSetting::FLAG_DRAW_ORDER) {
    case DRAWORDER_YOUNGERS_FIRST: {
        idx = 0;
        step = 1;
        break;
    }
    case DRAWORDER_ELDERS_FIRST:
    default: {
        idx = numVtx - 1;
        step = -1;
        break;
    }
    }

    if (connectEmitter && drawOrder == 0) {
        DrawParticle(NULL, pContext, pCalcAheadFunc,
                     pContext->mCommon.mEmitterCenter, rVtx0, rVtx1,
                     &prevAxis, pivot, texStep * idx, useTex);
        idx += step;
    }

    for (Particle* pIt = pGetFirstFunc(pManager); pIt != NULL;
         pIt = pGetNextFunc(pManager, pIt), idx += step) {

        DrawParticle(pIt, pContext, pCalcAheadFunc,
                     pIt->mParameter.mPosition, rVtx0, rVtx1, &prevAxis,
                     pivot, texStep * idx, useTex);
    }

    if (connectRing) {
        Particle* pFirst = pGetFirstFunc(pManager);
        DrawParticle(pFirst, pContext, pCalcAheadFunc,
                     pFirst->mParameter.mPosition, rVtx0, rVtx1, &prevAxis,
                     pivot, texStep * idx, useTex);
    }

    if (connectEmitter && drawOrder != 0) {
        DrawParticle(NULL, pContext, pCalcAheadFunc,
                     pContext->mCommon.mEmitterCenter, rVtx0, rVtx1,
                     &prevAxis, pivot, texStep * idx, useTex);
    }

    GXEnd();
}
#pragma pop

void DrawStripeStrategy::DrawParticle(Particle* pParticle,
                                      AheadContextStripe* pContext,
                                      CalcAheadFunc pCalcAheadFunc,
                                      const math::VEC3& rPos,
                                      const math::VEC3& rVtx0,
                                      const math::VEC3& rVtx1,
                                      math::VEC3* pPrevAxis, f32 pivot,
                                      f32 texCoord, bool useTex) {
    if (pParticle == NULL) {
        pParticle =
            GetYoungestDrawParticle_Stripe(pContext->mCommon.mParticleManager);
    }

    math::VEC3 ahead;
    math::MTX34 mtx;
    math::VEC3 p0;
    math::VEC3 p1;

    pCalcAheadFunc(&ahead, pContext, pParticle);

    math::MTX34 baseMtx =
        CalcStripeMtx(pParticle, pContext, ahead, rPos, pPrevAxis);

    math::MTX34 rotMtx = CalcRotateMtx(pParticle, pivot);

    math::MTX34Mult(&mtx, &baseMtx, &rotMtx);

    math::VEC3Transform(&p0, &mtx, &rVtx0);
    math::VEC3Transform(&p1, &mtx, &rVtx1);

    GXPosition3f32(p0.x, p0.y, p0.z);
    if (useTex) {
        GXTexCoord2f32(1.0f, texCoord);
    }

    GXPosition3f32(p1.x, p1.y, p1.z);
    if (useTex) {
        GXTexCoord2f32(0.0f, texCoord);
    }
}

static void GXEnd() {}

void DrawStripeStrategy::Draw(const DrawInfo& rInfo,
                              ParticleManager* pManager) {
    int numParticle = pManager->mActivityList.mActiveList.numObjects;
    EmitterResource* pResource = pManager->mResource;
    const EmitterDrawSetting& rSetting =
        pResource->GetEmitterDesc()->drawSetting;

    if (numParticle == 0) {
        return;
    }

    AheadContextStripe context(*rInfo.GetViewMtx(), pManager);
    math::MTX34 posMtx;

    // Particles that have not been drawn yet start from the emitter axis
    for (Particle* pIt = GetYoungestParticle(pManager); pIt != NULL;
         pIt = GetElderParticle(pManager, pIt)) {

        if (pIt->mPrevAxis.x <= 1.0f) {
            break;
        }

        pIt->mPrevAxis = context.mCommon.mEmitterAxisY;
    }

    int connect = pResource->GetEmitterDesc()->typeOption2 & 7;

    if ((connect == 1 && numParticle < 3) ||
        (connect == 0 && numParticle < 2)) {
        // Too few particles to draw: only update the axes
        math::VEC3 prevAxis(context.mCommon.mEmitterAxisY);

        int drawOrder = pResource->GetEmitterDesc()->drawSetting.mFlags &
                        EmitterDrawSetting::FLAG_DRAW_ORDER;

        CalcAheadFunc pCalcAheadFunc = GetCalcAheadFunc(pManager);

        GetFirstDrawParticleFunc pGetFirstFunc =
            GetGetFirstDrawParticleFunc(drawOrder);
        GetNextDrawParticleFunc pGetNextFunc =
            GetGetNextDrawParticleFunc(drawOrder);

        for (Particle* pIt = pGetFirstFunc(pManager); pIt != NULL;
             pIt = pGetNextFunc(pManager, pIt)) {

            math::VEC3 ahead;
            pCalcAheadFunc(&ahead, &context, pIt);

            math::MTX34 mtx = CalcStripeMtx(pIt, &context, ahead,
                                            pIt->mParameter.mPosition,
                                            &prevAxis);
        }

        return;
    }

    const EmitterDrawSetting& rDrawSetting =
        pManager->mResource->GetEmitterDesc()->drawSetting;

    InitTexture(rDrawSetting);
    InitTev(rDrawSetting, rInfo);
    InitColor(pManager, rDrawSetting, rInfo);

    GXEnableTexOffsets(GX_TEXCOORD0, GX_TRUE, GX_TRUE);

    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);

    if (mNumTexmap != 0) {
        GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    }

    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

    GXSetCurrentMtx(GX_PNMTX0);

    math::MTX34Mult(&posMtx, rInfo.GetViewMtx(),
                    &context.mCommon.mParticleManagerMtx);
    GXLoadPosMtxImm(posMtx, GX_PNMTX0);

    GetFirstDrawParticleFunc pGetFirstFunc = GetGetFirstDrawParticleFunc(
        pResource->GetEmitterDesc()->drawSetting.mFlags &
        EmitterDrawSetting::FLAG_DRAW_ORDER);

    SetupGP(pGetFirstFunc(pManager), rSetting, rInfo, true, false);

    DrawStripe(&context, math::VEC3(1.0f, 0.0f, 0.0f),
               math::VEC3(-1.0f, 0.0f, 0.0f));

    if (pResource->GetEmitterDesc()->typeOption ==
        EmitterDrawSetting::TYPE_CMN_CROSS) {
        DrawStripe(&context, math::VEC3(0.0f, 0.0f, -1.0f),
                   math::VEC3(0.0f, 0.0f, 1.0f));
    }
}

DrawStrategyImpl::CalcAheadFunc
DrawStripeStrategy::GetCalcAheadFunc(ParticleManager* pManager) {
    EmitterDesc* pDesc = pManager->mResource->GetEmitterDesc();

    switch (pDesc->typeDir) {
    case 0: {
        return CalcAhead_Speed;
    }
    case 1: {
        return CalcAhead_EmitterCenter;
    }
    case 2: {
        return CalcAhead_EmitterDesign;
    }
    case 3: {
        return CalcAhead_Particle_Stripe;
    }
    case 5: {
        return CalcAhead_NoDesign;
    }
    case 6: {
        switch (pDesc->typeOption2 & 7) {
        case 1: {
            return CalcAhead_ParticleBoth_Ring;
        }
        case 2: {
            return CalcAhead_ParticleBoth_Origin;
        }
        default: {
            return CalcAhead_ParticleBoth_Stripe;
        }
        }
    }
    default: {
        return CalcAhead_Speed;
    }
    }
}

void DrawStripeStrategy::CalcAhead_Particle_Stripe(math::VEC3* pAxisY,
                                                   AheadContext* pContext,
                                                   Particle* pParticle) {
    Particle* pYounger =
        GetYoungerDrawParticle(pContext->mCommon.mParticleManager, pParticle);

    if (pYounger != NULL) {
        math::VEC3Sub(pAxisY, &pParticle->mParameter.mPosition,
                      &pYounger->mParameter.mPosition);
    } else {
        math::VEC3Sub(pAxisY, &pParticle->mParameter.mPosition,
                      &pContext->mCommon.mEmitterCenter);
    }

    if (!Normalize(pAxisY)) {
        *pAxisY = pContext->mCommon.mEmitterAxisY;
    }
}

void DrawStripeStrategy::CalcAhead_ParticleBoth_Stripe(math::VEC3* pAxisY,
                                                       AheadContext* pContext,
                                                       Particle* pParticle) {
    Particle* pElder =
        GetElderDrawParticle(pContext->mCommon.mParticleManager, pParticle);

    Particle* pYounger =
        GetYoungerDrawParticle(pContext->mCommon.mParticleManager, pParticle);

    math::VEC3 elderPos(0.0f, 0.0f, 0.0f);

    if (pElder != NULL) {
        math::VEC3Sub(&elderPos, &pElder->mParameter.mPosition,
                      &pParticle->mParameter.mPosition);

        if (!Normalize(&elderPos)) {
            elderPos = math::VEC3(0.0f, 0.0f, 0.0f);
        }
    }

    math::VEC3 youngerPos(0.0f, 0.0f, 0.0f);

    if (pYounger != NULL) {
        math::VEC3Sub(&youngerPos, &pYounger->mParameter.mPosition,
                      &pParticle->mParameter.mPosition);

        if (!Normalize(&youngerPos)) {
            youngerPos = math::VEC3(0.0f, 0.0f, 0.0f);
        }
    }

    math::VEC3Sub(pAxisY, &elderPos, &youngerPos);

    if (!Normalize(pAxisY)) {
        *pAxisY = pContext->mCommon.mEmitterAxisY;
    }
}

void DrawStripeStrategy::CalcAhead_ParticleBoth_Ring(math::VEC3* pAxisY,
                                                     AheadContext* pContext,
                                                     Particle* pParticle) {
    Particle* pElder =
        GetElderDrawParticle(pContext->mCommon.mParticleManager, pParticle);
    if (pElder == NULL) {
        pElder = GetYoungestDrawParticle_Stripe(
            pContext->mCommon.mParticleManager);
    }

    Particle* pYounger =
        GetYoungerDrawParticle(pContext->mCommon.mParticleManager, pParticle);
    if (pYounger == NULL) {
        pYounger = GetOldestDrawParticle(pContext->mCommon.mParticleManager);
    }

    math::VEC3 elderPos;
    math::VEC3Sub(&elderPos, &pElder->mParameter.mPosition,
                  &pParticle->mParameter.mPosition);

    if (!Normalize(&elderPos)) {
        elderPos = math::VEC3(0.0f, 0.0f, 0.0f);
    }

    math::VEC3 youngerPos;
    math::VEC3Sub(&youngerPos, &pYounger->mParameter.mPosition,
                  &pParticle->mParameter.mPosition);

    if (!Normalize(&youngerPos)) {
        youngerPos = math::VEC3(0.0f, 0.0f, 0.0f);
    }

    math::VEC3Sub(pAxisY, &elderPos, &youngerPos);

    if (!Normalize(pAxisY)) {
        *pAxisY = pContext->mCommon.mEmitterAxisY;
    }
}

void DrawStripeStrategy::CalcAhead_ParticleBoth_Origin(math::VEC3* pAxisY,
                                                       AheadContext* pContext,
                                                       Particle* pParticle) {
    Particle* pElder =
        GetElderDrawParticle(pContext->mCommon.mParticleManager, pParticle);

    Particle* pYounger =
        GetYoungerDrawParticle(pContext->mCommon.mParticleManager, pParticle);

    math::VEC3 elderPos(0.0f, 0.0f, 0.0f);

    if (pElder != NULL) {
        math::VEC3Sub(&elderPos, &pElder->mParameter.mPosition,
                      &pParticle->mParameter.mPosition);
        Normalize(&elderPos);
    }

    math::VEC3 youngerPos;

    if (pYounger != NULL) {
        math::VEC3Sub(&youngerPos, &pYounger->mParameter.mPosition,
                      &pParticle->mParameter.mPosition);
    } else {
        math::VEC3Sub(&youngerPos, &pContext->mCommon.mEmitterCenter,
                      &pParticle->mParameter.mPosition);
    }

    Normalize(&youngerPos);

    math::VEC3Sub(pAxisY, &elderPos, &youngerPos);

    if (!Normalize(pAxisY)) {
        *pAxisY = pContext->mCommon.mEmitterAxisY;
    }
}

} // namespace ef
} // namespace nw4r
