#include <news/GlobeDots.h>
#include <news/Camera.h>
#include <news/Draw2D.h>
#include <news/System.h>
#include <nw4r/math/math_arithmetic.h>
#include <nw4r/math/math_triangular.h>
#include <nw4r/math/math_types.h>
#include <revolution/gx.h>
#include <string.h>

using namespace nw4r;

struct GlobeView {
    // Draw() reads the camera through this getter (it changes the saved registers).
    Camera* GetCamera() { return mCamera; }

    u8 unk0[0x4];
    Camera* mCamera; // at 0x04
};
extern "C" GlobeView* gGlobe;

extern const f32 gGlobeDotTexCoords[3][2];
extern const union GlobeDotAngles {
    u8 bytes[GLOBE_DOT_COUNT * 4];
    u16 angles[GLOBE_DOT_COUNT * 2]; // x, y rotation of each dot
} gGlobeDotAngles;
extern const u8 gGlobeDotSizes[GLOBE_DOT_COUNT];
extern const u8 gGlobeDotColorIdx[GLOBE_DOT_COUNT];
extern const u8 gGlobeDotColors[GLOBE_DOT_COUNT * 3];

// The SDK fast cast (OSu16tof32).
static inline f32 U16ToF32(register u16* in) {
#ifdef TARGET_PC
    return PCFastCastU16ToF32(*in);
#else
    register f32 ret;
    asm {
        psq_l ret, 0(in), 1, 3
    }
    return ret;
#endif
}

static inline f32 SinIdx(u16 idx) {
    return math::SinFIdx(0.00390625f * U16ToF32(&idx));
}

static inline f32 CosIdx(u16 idx) {
    return math::CosFIdx(0.00390625f * U16ToF32(&idx));
}

GlobeDots::GlobeDots() {
    for (s32 i = 0; i < GLOBE_DOT_COUNT; i++) {
        s32 a = i * 2;
        s32 idx = i * 9;
#ifdef TARGET_PC
        // The table is the DOL's bytes (GlobeDotAngles.inc): big-endian u16.
        // Reading them through the union is right only on a big-endian host.
        u16 rotX = gGlobeDotAngles.bytes[a * 2] << 8 | gGlobeDotAngles.bytes[a * 2 + 1];
        u16 rotY = gGlobeDotAngles.bytes[a * 2 + 2] << 8 | gGlobeDotAngles.bytes[a * 2 + 3];
#else
        u16 rotX = gGlobeDotAngles.angles[a];
        u16 rotY = gGlobeDotAngles.angles[a + 1];
#endif
        f32 size = 0.0045f * gGlobeDotSizes[i];
        f32 size3 = 3.0f * size;

        Mtx mx, my, m;
        PSMTXRotTrig(mx, 'x', SinIdx(rotX), CosIdx(rotX));
        PSMTXRotTrig(my, 'y', SinIdx(rotY), CosIdx(rotY));
        PSMTXConcat(my, mx, m);

        Vec v0 = {-size, -size, -100.0f};
        PSMTXMultVec(m, &v0, (Vec*)&mVerts[idx]);

        Vec v1 = {size3, -size, -100.0f};
        PSMTXMultVec(m, &v1, (Vec*)&mVerts[idx + 3]);

        Vec v2 = {-size, size3, -100.0f};
        PSMTXMultVec(m, &v2, (Vec*)&mVerts[idx + 6]);
    }
}

GlobeDots::~GlobeDots() {}

void GlobeDots::ResetAlpha() {
    mAlpha = 255;
}

static inline f32 Clamp(f32 x, f32 min, f32 max) {
    return x > max ? max : (x < min ? min : x);
}

void GlobeDots::UpdateAlpha(f32 dx, f32 dy) {
    u8 target = Clamp(255.0f - 200.0f * (math::FAbs(dx) + math::FAbs(dy)), 0.0f, 255.0f);
    if (mAlpha > target) {
        if (mAlpha - target < 32) {
            mAlpha = target;
        } else {
            mAlpha -= 32;
        }
    } else if (mAlpha >= 254) {
        mAlpha = 255;
    } else {
        mAlpha += 2;
    }
}

void GlobeDots::Draw() {
    Camera* camera = gGlobe->GetCamera();
    Draw2D_SetupGX();
    GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);

    math::MTX34 view;
    camera->mCamera.GetCameraMtx(&view);
    math::MTX44 proj;
    camera->mCamera.GetProjectionMtx(&proj);
    f32 aspect = (f32)GetScreenWidth() / 456.0f;
    C_MTXPerspective(proj, 40.0f, aspect, 1.0f, 1000.0f);
    view._23 = view._13 = view._03 = 0.0f;
    GXLoadPosMtxImm(view, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    GXSetProjection(proj, GX_PERSPECTIVE);

    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_INDEX16);
    GXSetVtxDesc(GX_VA_CLR0, GX_INDEX8);
    GXSetVtxDesc(GX_VA_TEX0, GX_INDEX8);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGB, GX_RGB8, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GXSetArray(GX_VA_POS, mVerts, sizeof(Vec));
    GXSetArray(GX_VA_CLR0, (void*)gGlobeDotColors, 3);
    GXSetArray(GX_VA_TEX0, (void*)gGlobeDotTexCoords, sizeof(gGlobeDotTexCoords[0]));
    GXSetChanCtrl(GX_COLOR0, GX_FALSE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE,
                  GX_AF_NONE);

    GXTexObj texObj;
    TPL_GetTexObj(gCursorTpl, 0x60, &texObj);
    GXLoadTexObj(&texObj, GX_TEXMAP0);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_RASC, GX_CC_ONE, GX_CC_TEXC, GX_CC_ZERO);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_A0, GX_CA_ZERO);
    GXSetTevColor(GX_TEVREG0, (GXColor){0, 0, 0, mAlpha});

    GXBegin(GX_TRIANGLES, GX_VTXFMT0, GLOBE_DOT_COUNT * 3);
    // The loop counts vertices; the dot index runs alongside.
    for (s32 pos = 0, i = 0; pos < GLOBE_DOT_COUNT * 3; pos += 3, i++) {
        u8 c = gGlobeDotColorIdx[i];
        GXPosition1x16(pos);
        GXColor1x8(c);
        GXTexCoord1x8(0);
        GXPosition1x16(pos + 1);
        GXColor1x8(c);
        GXTexCoord1x8(1);
        GXPosition1x16(pos + 2);
        GXColor1x8(c);
        GXTexCoord1x8(2);
    }
    GXEnd();
}

const f32 gGlobeDotTexCoords[3][2] = {
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {0.0f, 1.0f},
};

// Rotation of each dot about x and y (u16 angle units). The extracted table is
// bytes, hence the union.
const union GlobeDotAngles gGlobeDotAngles = {
#include "news/GlobeDotAngles.inc"
};

const u8 gGlobeDotSizes[GLOBE_DOT_COUNT] = {
#include "news/GlobeDotSizes.inc"
};

const u8 gGlobeDotColorIdx[GLOBE_DOT_COUNT] = {
#include "news/GlobeDotColorIdx.inc"
};

const u8 gGlobeDotColors[GLOBE_DOT_COUNT * 3] = {
#include "news/GlobeDotColors.inc"
};
