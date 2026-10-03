#include <news/GlobeDots.h>
#include <news/Camera.h>
#include <news/Draw2D.h>
#include <news/System.h>
#include <nw4r/math/math_triangular.h>
#include <nw4r/math/math_types.h>
#include <revolution/gx.h>
#include <string.h>

using namespace nw4r;

struct GlobeView {
    u8 unk0[0x4];
    Camera* mCamera; // at 0x04
};
extern "C" GlobeView* lbl_8035775C;

// One triangle per dot, before it is scaled and rotated into place.
static const Vec sGlobeDotTemplate[3] = {
    {0.0f, 0.0f, -100.0f},
    {0.0f, 0.0f, -100.0f},
    {0.0f, 0.0f, -100.0f},
};

static const f32 sGlobeDotTexCoords[3][2] = {
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {0.0f, 1.0f},
};

// Rotation of each dot about x and y (u16 angle units). The extracted table is
// bytes, hence the union.
static const union {
    u8 bytes[GLOBE_DOT_COUNT * 4];
    u16 angles[GLOBE_DOT_COUNT][2];
} sGlobeDotAngles = {
#include "news/GlobeDotAngles.inc"
};

static const u8 sGlobeDotSizes[GLOBE_DOT_COUNT] = {
#include "news/GlobeDotSizes.inc"
};

static const u8 sGlobeDotColorIdx[GLOBE_DOT_COUNT] = {
#include "news/GlobeDotColorIdx.inc"
};

static const u8 sGlobeDotColors[GLOBE_DOT_COUNT * 3] = {
#include "news/GlobeDotColors.inc"
};

// The SDK fast cast (OSu16tof32).
static inline f32 U16ToF32(register u16* in) {
    register f32 ret;
    asm {
        psq_l ret, 0(in), 1, 3
    }
    return ret;
}

static inline f32 SinIdx(u16 idx) {
    return math::SinFIdx(0.00390625f * U16ToF32(&idx));
}

static inline f32 CosIdx(u16 idx) {
    return math::CosFIdx(0.00390625f * U16ToF32(&idx));
}

GlobeDots::GlobeDots() {
    for (s32 i = 0; i < GLOBE_DOT_COUNT; i++) {
        s32 idx = i * 3;
        f32 size = 0.0045f * sGlobeDotSizes[i];
        f32 size3 = 3.0f * size;
        u16 rotX = sGlobeDotAngles.angles[i][0];
        u16 rotY = sGlobeDotAngles.angles[i][1];

        Mtx mx, my, m;
        PSMTXRotTrig(mx, 'x', SinIdx(rotX), CosIdx(rotX));
        PSMTXRotTrig(my, 'y', SinIdx(rotY), CosIdx(rotY));
        PSMTXConcat(my, mx, m);

        Vec v0 = *(Vec*)&sGlobeDotTemplate[0];
        v0.x = -size;
        v0.y = -size;
        PSMTXMultVec(m, &v0, &mVerts[idx]);

        Vec v1 = *(Vec*)&sGlobeDotTemplate[1];
        v1.x = size3;
        v1.y = -size;
        PSMTXMultVec(m, &v1, &mVerts[idx + 1]);

        Vec v2 = *(Vec*)&sGlobeDotTemplate[2];
        v2.x = -size;
        v2.y = size3;
        PSMTXMultVec(m, &v2, &mVerts[idx + 2]);
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
    u8 target = Clamp(255.0f - 200.0f * (__fabsf(dx) + __fabsf(dy)), 0.0f, 255.0f);
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
    Camera* camera = lbl_8035775C->mCamera;
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
    GXSetArray(GX_VA_CLR0, (void*)sGlobeDotColors, 3);
    GXSetArray(GX_VA_TEX0, (void*)sGlobeDotTexCoords, sizeof(sGlobeDotTexCoords[0]));
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
    const u8* colors = sGlobeDotColorIdx;
    for (s32 i = 0; i < GLOBE_DOT_COUNT; i++) {
        GXPosition1x16(i * 3);
        GXColor1x8(colors[i]);
        GXTexCoord1x8(0);
        GXPosition1x16(i * 3 + 1);
        GXColor1x8(colors[i]);
        GXTexCoord1x8(1);
        GXPosition1x16(i * 3 + 2);
        GXColor1x8(colors[i]);
        GXTexCoord1x8(2);
    }
    GXEnd();
}
