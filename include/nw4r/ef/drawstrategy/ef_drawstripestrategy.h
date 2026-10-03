#ifndef NW4R_EF_DRAW_STRIPE_STRATEGY_H
#define NW4R_EF_DRAW_STRIPE_STRATEGY_H
#include <nw4r/types_nw4r.h>

#include <nw4r/ef/drawstrategy/ef_drawstrategyimpl.h>

namespace nw4r {
namespace ef {

class DrawStripeStrategy : public DrawStrategyImpl {
public:
    // Older revision (News Channel): only the emitter X axis is added
    struct AheadContextStripe : public AheadContext {
        math::VEC3 mEmitterAxisX; // at 0xBC

        AheadContextStripe(const math::MTX34& rViewMtx,
                           ParticleManager* pManager)
            : AheadContext(rViewMtx, pManager) {}
    };

public:
    DrawStripeStrategy();

    virtual void Draw(const DrawInfo& rInfo,
                      ParticleManager* pManager); // at 0xC

    virtual CalcAheadFunc
    GetCalcAheadFunc(ParticleManager* pManager); // at 0x18

    // Older revision (News Channel): no tubes or connect-type variants
    void DrawStripe(AheadContextStripe* pContext, const math::VEC3& rVtx0,
                    const math::VEC3& rVtx1);

    void DrawParticle(Particle* pParticle, AheadContextStripe* pContext,
                      CalcAheadFunc pCalcAheadFunc, const math::VEC3& rPos,
                      const math::VEC3& rVtx0, const math::VEC3& rVtx1,
                      math::VEC3* pPrevAxis, f32 pivot, f32 texCoord,
                      bool useTex);

    static void CalcAhead_Particle_Stripe(math::VEC3* pAxisY,
                                          AheadContext* pContext,
                                          Particle* pParticle);
    static void CalcAhead_ParticleBoth_Stripe(math::VEC3* pAxisY,
                                              AheadContext* pContext,
                                              Particle* pParticle);
    static void CalcAhead_ParticleBoth_Ring(math::VEC3* pAxisY,
                                            AheadContext* pContext,
                                            Particle* pParticle);
    static void CalcAhead_ParticleBoth_Origin(math::VEC3* pAxisY,
                                              AheadContext* pContext,
                                              Particle* pParticle);

    // Older revision (News Channel): steps with GetElderDrawParticle
    static Particle* GetYoungestDrawParticle_Stripe(ParticleManager* pManager) {
        Particle* pIt = GetYoungestParticle(pManager);

        while (pIt != NULL &&
               pIt->GetLifeStatus() != ReferencedObject::NW4R_EF_LS_ACTIVE) {
            pIt = GetElderDrawParticle(pManager, pIt);
        }

        return pIt;
    }

};

} // namespace ef
} // namespace nw4r

#endif
