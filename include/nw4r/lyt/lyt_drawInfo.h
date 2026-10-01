#ifndef NW4R_LYT_DRAW_INFO_H
#define NW4R_LYT_DRAW_INFO_H

#include <types.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_Rect.h>

namespace nw4r {
namespace lyt {

class DrawInfo {
public:
    DrawInfo();
    virtual ~DrawInfo();

    void SetViewMtx(const math::MTX34& value) { mViewMtx = value; }
    void SetViewRect(const ut::Rect& value) { mViewRect = value; }

protected:
    math::MTX34 mViewMtx;             // at 0x04
    ut::Rect mViewRect;               // at 0x34
    math::VEC2 mLocationAdjustScale;  // at 0x44
    f32 mGlobalAlpha;                 // at 0x4C
    u8 mFlag;                         // at 0x50
};

} // namespace lyt
} // namespace nw4r

#endif
