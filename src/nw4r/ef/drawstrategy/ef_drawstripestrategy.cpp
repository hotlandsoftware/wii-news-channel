#include <nw4r/ef.h>

// No reference source. Not decompiled yet; only the constructor is written.
// What is in the DOL (0x800B7F3C-0x800B9690, News Channel, older NW4R):
//
// 0x800B7F3C  DrawStripeStrategy::DrawStripeStrategy()
// 0x800B7F78  (0x350) draws one stripe: walks the particles with the draw
//             order functions and calls 0x800B82F4 for each vertex pair; its
//             arguments are (this, AheadContext*, CalcAheadFunc, ...).
// 0x800B82C8  EmitterResource::GetEmitterDesc() (out of line here)
// 0x800B82D0  returns ActivityList::mNumActive (out of line here)
// 0x800B82D8  math::VEC3 copy (out of line here)
// 0x800B82F4  (0x3E0) emits one vertex pair: axis from the CalcAheadFunc,
//             cross product with Particle::mPrevAxis, rotation, two
//             GXPosition/GXTexCoord pairs.
// 0x800B86D4  empty function (GXEnd)
// 0x800B86D8  DrawStripeStrategy::Draw (0x51C)
// 0x800B8BF4  DrawStripeStrategy::GetCalcAheadFunc (typeDir 0-6; 6 selects by
//             typeOption2 & 7 between 0x800B8DF4, 0x800B90D0, 0x800B93FC)
// 0x800B8CA8  CalcAhead function for typeDir 3 (stripe-specific)
// 0x800B8DF4  DrawStrategyImpl::CalcAhead_ParticleBoth
// 0x800B90D0, 0x800B93FC  two more CalcAhead variants

namespace nw4r {
namespace ef {

DrawStripeStrategy::DrawStripeStrategy() {}

} // namespace ef
} // namespace nw4r
