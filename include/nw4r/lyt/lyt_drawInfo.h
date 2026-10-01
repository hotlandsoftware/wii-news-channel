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
    virtual ~DrawInfo(); // at 0x08

    const math::MTX34& GetViewMtx() const { return mViewMtx; }
    void SetViewMtx(const math::MTX34& value) { mViewMtx = value; }

    void SetViewRect(const ut::Rect& value) { mViewRect = value; }

    const math::VEC2& GetLocationAdjustScale() const { return mLocationAdjustScale; }
    void SetLocationAdjustScale(const math::VEC2& scale) { mLocationAdjustScale = scale; }

    bool IsMultipleViewMtxOnDraw() const { return mFlag.mulViewDraw; }
    void SetMultipleViewMtxOnDraw(bool bEnable) { mFlag.mulViewDraw = bEnable; }

    bool IsInfluencedAlpha() const { return mFlag.influencedAlpha; }
    void SetInfluencedAlpha(bool bEnable) { mFlag.influencedAlpha = bEnable; }

    bool IsLocationAdjust() const { return mFlag.locationAdjust; }
    void SetLocationAdjust(bool bEnable) { mFlag.locationAdjust = bEnable; }

    bool IsInvisiblePaneCalculateMtx() const { return mFlag.invisiblePaneCalculateMtx; }
    void SetInvisiblePaneCalculateMtx(bool bEnable) { mFlag.invisiblePaneCalculateMtx = bEnable; }

    bool IsDebugDrawMode() const { return mFlag.debugDrawMode; }
    void SetDebugDrawMode(bool bEnable) { mFlag.debugDrawMode = bEnable; }

    bool IsYAxisUp() const { return mViewRect.bottom - mViewRect.top < 0.0f; }

    f32 GetGlobalAlpha() const { return mGlobalAlpha; }
    void SetGlobalAlpha(f32 alpha) { mGlobalAlpha = alpha; }

protected:
    math::MTX34 mViewMtx;            // at 0x04
    ut::Rect mViewRect;              // at 0x34
    math::VEC2 mLocationAdjustScale; // at 0x44
    f32 mGlobalAlpha;                // at 0x4C
    struct {
        u8 mulViewDraw : 1;
        u8 influencedAlpha : 1;
        u8 locationAdjust : 1;
        u8 invisiblePaneCalculateMtx : 1;
        u8 debugDrawMode : 1;
    } mFlag; // at 0x50
};

} // namespace lyt
} // namespace nw4r

#endif
