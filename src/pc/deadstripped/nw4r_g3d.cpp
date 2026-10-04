// nw4r::g3d definitions that are not in the News Channel DOL.
// See src/pc/deadstripped/nw4r_ut.cpp and docs/pc_port.md, "Dead-stripped
// definitions".

#include <nw4r/g3d.h>

namespace nw4r {
namespace g3d {

// First out-of-line virtual of AnmObj (vtable anchor); the base does nothing.
void AnmObj::Release() {}

// Static member read by the inline FrameCtrl::UpdateFrm(); 1.0 in the library.
f32 FrameCtrl::smBaseUpdateRate = 1.0f;

// g3d_anmchr.cpp is not in the DOL (the channel has no skeletal animation),
// but inline code still asks for AnmObjChr's type name (DynamicCast).
NW4R_G3D_RTTI_DEF(AnmObjChr);

} // namespace g3d
} // namespace nw4r
