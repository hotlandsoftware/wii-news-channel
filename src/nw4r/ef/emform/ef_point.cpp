#include <nw4r/ef.h>

#include <cmath>

namespace nw4r {
namespace ef {

void EmitterFormPoint::Emission(Emitter* pEmitter, ParticleManager* pManager,
                                int count, u32 flags, f32* pParams, u16 life,
                                f32 lifeRnd, const math::MTX34* pSpace) {

#pragma unused(flags)
#pragma unused(pParams)

    if (count < 1) {
        return;
    }

    for (int i = 0; i < count; i++) {
        math::VEC3 pos(0.0f, 0.0f, 0.0f);
        math::VEC3 normal;

        f32 rx = pEmitter->mRandom.RandFloat() * 2.0f - 1.0f;
        if (rx >= 0.0f) {
            normal.x = (0.66f + 0.34f * rx) * rx;
        } else {
            normal.x = (0.66f - 0.34f * rx) * rx;
        }

        f32 radius = std::sqrtf(1.0f - normal.x * normal.x);
        rx = pEmitter->mRandom.RandFloat() * NW4R_MATH_PI * 2;

        normal.y = std::cosf(rx) * radius;
        normal.z = std::sinf(rx) * radius;

        math::VEC3 fromYAxis(normal.x, 0.0f, normal.z);

        if (normal.x != 0.0f || normal.z != 0.0f) {
            math::VEC3Normalize(&fromYAxis, &fromYAxis);
        }

        math::VEC3 vel;
        CalcVelocity(&vel, pEmitter, pos, normal, normal, fromYAxis);

        // clang-format off
        pManager->CreateParticle(
            CalcLife(life, lifeRnd, pEmitter),
            pos,
            vel,
            pSpace,
            1.0f + pEmitter->mParameter.mVelMomentumRandom *
                (1.0f / 100.0f) * pEmitter->mRandom.RandFloat(),
            &pEmitter->mInheritSetting,
            pEmitter->mpReferenceParticle,
            pEmitter->mCalcRemain);
        // clang-format on
    }
}

void EmitterFormPoint::Draw(Emitter* pEmitter, const DrawInfo& rInfo) {
    SetupDrawGX();

    math::MTX34 mtx = *rInfo.GetViewMtx();
    math::MTX34 work;
    pEmitter->CalcGlobalMtx(&work);
    math::MTX34Mult(&mtx, &mtx, &work);

    // Edge length of GXDrawCube (unused here)
    f32 cubeSize = 2.0f / std::sqrtf(3.0f);
#pragma unused(cubeSize)

    math::VEC3 scale(0.4f, 0.4f, 0.4f);
    math::MTX34Scale(&mtx, &mtx, &scale);

    GXLoadPosMtxImm(mtx, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    GXDrawCube();
}

} // namespace ef
} // namespace nw4r
