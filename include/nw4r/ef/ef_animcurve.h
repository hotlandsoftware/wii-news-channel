#ifndef NW4R_EF_ANIM_CURVE_H
#define NW4R_EF_ANIM_CURVE_H
#include <nw4r/types_nw4r.h>

namespace nw4r {
namespace ef {

// Forward declarations
class Particle;

// Seed of the per-key random values: big-endian bytes, XOR-folded
union AnimCurveRandomSeed {
    u32 value;
    u8 bytes[4];
};

// Older revision (News Channel): one function per component count.
// The names are guesses.
void AnimCurveExecuteColor(u8* pCmdList, u8* pTarget, u32 tick, u16 seed,
                           u32 life);
void AnimCurveExecuteAlpha(u8* pCmdList, u8* pTarget, u32 tick, u16 seed,
                           u32 life);

void AnimCurveExecuteF32x1(u8* pCmdList, Particle* pParticle, f32* pTarget,
                           u32 tick, u16 seed, u32 life);
void AnimCurveExecuteF32x2(u8* pCmdList, Particle* pParticle, f32* pTarget,
                           u32 tick, u16 seed, u32 life);
void AnimCurveExecuteF32x3(u8* pCmdList, f32* pTarget, u32 tick, u16 seed,
                           u32 life);

void AnimCurveExecuteF32(u8* pCmdList, f32* pTarget, u32 tick, u16 seed,
                         u32 life, u8 ctrl);

void AnimCurveExecuteRotate(u8* pCmdList, f32* pTarget, u32 tick, u16 seed,
                            u32 life);

void AnimCurveExecuteTexture(u8* pCmdList, Particle* pParticle, u32 tick,
                             u16 seed, u32 life);

void AnimCurveExecuteChild(u8* pCmdList, Particle* pParticle, u32 tick,
                           u16 seed, u32 life);

} // namespace ef
} // namespace nw4r

#endif
