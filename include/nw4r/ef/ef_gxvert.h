#ifndef NW4R_EF_GX_VERT_H
#define NW4R_EF_GX_VERT_H

// ogws's GXVert.h vertex functions, private to nw4r::ef (Task 9). They write
// through the WGPIPE object at 0xCC008000 (declared in g3d's copy of ogws's
// GXHardware.h), which, unlike the pointer cast of our SDK's GXVert.h, does
// not alias local variables. The ef functions that send vertices in a loop
// (DrawStrategyImpl::SetupGP) depend on this. Being in nw4r::ef, these hide
// the SDK functions of the same name in ef code.
#include <types.h>

#include <nw4r/g3d/platform/gx/GXHardwareBase.h>

namespace nw4r {
namespace ef {

inline void GXCmd1u8(u8 uc) {
    WGPIPE.uc = uc;
}

inline void GXCmd1u16(u16 us) {
    WGPIPE.us = us;
}

inline void GXCmd1u32(u32 ul) {
    WGPIPE.ui = ul;
}

inline void GXPosition3f32(f32 x, f32 y, f32 z) {
    WGPIPE.f = x;
    WGPIPE.f = y;
    WGPIPE.f = z;
}

inline void GXPosition3u8(u8 x, u8 y, u8 z) {
    WGPIPE.uc = x;
    WGPIPE.uc = y;
    WGPIPE.uc = z;
}

inline void GXPosition3s8(s8 x, s8 y, s8 z) {
    WGPIPE.c = x;
    WGPIPE.c = y;
    WGPIPE.c = z;
}

inline void GXPosition3u16(u16 x, u16 y, u16 z) {
    WGPIPE.us = x;
    WGPIPE.us = y;
    WGPIPE.us = z;
}

inline void GXPosition3s16(s16 x, s16 y, s16 z) {
    WGPIPE.s = x;
    WGPIPE.s = y;
    WGPIPE.s = z;
}

inline void GXPosition2f32(f32 x, f32 y) {
    WGPIPE.f = x;
    WGPIPE.f = y;
}

inline void GXPosition2u8(u8 x, u8 y) {
    WGPIPE.uc = x;
    WGPIPE.uc = y;
}

inline void GXPosition2s8(s8 x, s8 y) {
    WGPIPE.c = x;
    WGPIPE.c = y;
}

inline void GXPosition2u16(u16 x, u16 y) {
    WGPIPE.us = x;
    WGPIPE.us = y;
}

inline void GXPosition2s16(s16 x, s16 y) {
    WGPIPE.s = x;
    WGPIPE.s = y;
}

inline void GXPosition1x16(u16 us) {
    WGPIPE.us = us;
}

inline void GXPosition1x8(u8 uc) {
    WGPIPE.uc = uc;
}

inline void GXNormal3f32(f32 x, f32 y, f32 z) {
    WGPIPE.f = x;
    WGPIPE.f = y;
    WGPIPE.f = z;
}

inline void GXNormal3u16(s16 x, s16 y, s16 z) {
    WGPIPE.us = x;
    WGPIPE.us = y;
    WGPIPE.us = z;
}

inline void GXNormal3s16(s16 x, s16 y, s16 z) {
    WGPIPE.s = x;
    WGPIPE.s = y;
    WGPIPE.s = z;
}

inline void GXNormal3u8(u8 x, u8 y, u8 z) {
    WGPIPE.uc = x;
    WGPIPE.uc = y;
    WGPIPE.uc = z;
}

inline void GXNormal3s8(s8 x, s8 y, s8 z) {
    WGPIPE.c = x;
    WGPIPE.c = y;
    WGPIPE.c = z;
}

inline void GXNormal1x16(u16 us) {
    WGPIPE.us = us;
}

inline void GXNormal1x8(u8 uc) {
    WGPIPE.uc = uc;
}

inline void GXColor4u8(u8 r, u8 g, u8 b, u8 a) {
    WGPIPE.uc = r;
    WGPIPE.uc = g;
    WGPIPE.uc = b;
    WGPIPE.uc = a;
}

inline void GXColor1u32(u32 color) {
    WGPIPE.ui = color;
}

inline void GXColor3u8(u8 r, u8 g, u8 b) {
    WGPIPE.uc = r;
    WGPIPE.uc = g;
    WGPIPE.uc = b;
}

inline void GXColor1u16(u16 us) {
    WGPIPE.us = us;
}

inline void GXColor1x16(u16 us) {
    WGPIPE.us = us;
}

inline void GXColor1x8(u8 uc) {
    WGPIPE.uc = uc;
}

inline void GXTexCoord2f32(f32 x, f32 y) {
    WGPIPE.f = x;
    WGPIPE.f = y;
}

inline void GXTexCoord2s16(s16 x, s16 y) {
    WGPIPE.s = x;
    WGPIPE.s = y;
}

inline void GXTexCoord2u16(u16 x, u16 y) {
    WGPIPE.us = x;
    WGPIPE.us = y;
}

inline void GXTexCoord2s8(s8 x, s8 y) {
    WGPIPE.c = x;
    WGPIPE.c = y;
}

inline void GXTexCoord2u8(u8 x, u8 y) {
    WGPIPE.uc = x;
    WGPIPE.uc = y;
}

inline void GXTexCoord1f32(f32 f) {
    WGPIPE.f = f;
}

inline void GXTexCoord1s16(s16 s) {
    WGPIPE.s = s;
}

inline void GXTexCoord1u16(u16 us) {
    WGPIPE.us = us;
}

inline void GXTexCoord1s8(s8 c) {
    WGPIPE.c = c;
}

inline void GXTexCoord1u8(u8 uc) {
    WGPIPE.uc = uc;
}

inline void GXTexCoord1x16(u16 us) {
    WGPIPE.us = us;
}

inline void GXTexCoord1x8(u8 uc) {
    WGPIPE.uc = uc;
}

} // namespace ef
} // namespace nw4r

#endif
