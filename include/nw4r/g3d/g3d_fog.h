#ifndef NW4R_G3D_FOG_H
#define NW4R_G3D_FOG_H
#include <types.h>
#include <macros.h>

#include <nw4r/g3d/res/g3d_rescommon.h>

#include <nw4r/math/math_types.h>
#include <nw4r/math/math_arithmetic.h>
#include <nw4r/math/math_triangular.h>
#include <nw4r/math/math_geometry.h>
#include <nw4r/ut/ut_Color.h>
#include <nw4r/ut/ut_algorithm.h>
#include <nw4r/ut/ut_LockedCache.h>
#include <nw4r/ut/ut_binaryFileFormat.h>

#include <revolution/gx.h>
#include <nw4r/g3d/platform/g3d_gxhw.h>

namespace nw4r {
namespace g3d {

struct FogData {
    GXFogType type;         // at 0x0
    f32 startz;             // at 0x4
    f32 endz;               // at 0x8
    f32 nearz;              // at 0xC
    f32 farz;               // at 0x10
    GXColor color;          // at 0x14
    GXBool adjEnable;       // at 0x18
    u8 PADDING_0x19;        // at 0x19
    u16 adjCenter;          // at 0x1A
    GXFogAdjTable adjTable; // at 0x1C
};

class Fog : public ResCommon<FogData> {
public:
    explicit Fog(FogData* pData);

    void Init();
    Fog CopyTo(void* pDst) const;

    void SetFogRangeAdjParam(u16 width, u16 center,
                             const math::MTX44& rProjMtx);
    void SetGP() const;

    void SetFogType(GXFogType type) {
        if (!IsValid()) {
            return;
        }

        ref().type = type;
    }

    void SetZ(f32 startZ, f32 endZ) {
        if (!IsValid()) {
            return;
        }

        FogData& r = ref();

        r.startz = startZ;
        r.endz = endZ;
    }

    void SetNearFar(f32 nearZ, f32 farZ) {
        if (!IsValid()) {
            return;
        }

        FogData& r = ref();

        r.nearz = nearZ;
        r.farz = farZ;
    }

    void SetFogColor(GXColor color) {
        if (!IsValid()) {
            return;
        }

        ref().color = color;
    }

    bool IsFogRangeAdjEnable() const {
        return IsValid() && ref().adjEnable == TRUE;
    }
};

} // namespace g3d
} // namespace nw4r

#endif
