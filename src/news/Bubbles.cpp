#include <news/Bubbles.h>
#include <news/Draw2D.h>
#include <news/Random.h>
#include <news/System.h>
#include <nw4r/math/math_arithmetic.h>
#include <nw4r/math/math_triangular.h>
#include <revolution/gx.h>

static inline f32 SinIdx(u16 idx) {
    return nw4r::math::SinFIdx((1.0f / 256.0f) * nw4r::math::U16ToF32(idx));
}

static inline f32 CosIdx(u16 idx) {
    return nw4r::math::CosFIdx((1.0f / 256.0f) * nw4r::math::U16ToF32(idx));
}

Bubbles::Bubbles() {}

Bubbles::~Bubbles() {}

void Bubbles::Reset() {
    for (s32 i = 0; i < NUM; i++) {
        mTimer[i] = 0;
    }
    mSpawnTimer = 0;
}

void Bubbles::Update(BOOL spawn) {
    if (spawn && --mSpawnTimer < 0) {
        f32 x = (RandomF(1.6f) - 0.3f) * GetScreenWidth();
        f32 y = (RandomF(1.6f) - 0.3f) * GetScreenHeight();
        f32 vx = RandomF(0.8f);
        f32 vy = RandomF(0.8f);
        f32 dx = x - 0.5f * GetScreenWidth();
        f32 dy = y - 228.0f;
        f32 ady = __fabsf(dy);
        if (__fabsf(dx) > ady) {
            if (dx > 0.0f) {
                vx = -vx;
            }
        } else if (dy > 0.0f) {
            vy = -vy;
        }
        s32 time = Random() % 60 + 240;
        f32 size = GetScreenHeight() * (0.05f + RandomF(0.45f));
        s32 type = (Random() & 1) + 1;
        for (s32 i = 0; i < NUM; i++) {
            if (mTimer[i] == 0) {
                mType[i] = type;
                mX[i] = x;
                mY[i] = y;
                mVX[i] = vx;
                mVY[i] = vy;
                mSize[i] = size;
                mTimer[i] = time;
                mDuration[i] = time;
                break;
            }
        }
        mSpawnTimer = Random() % 15 + 25;
    }

    for (s32 i = 0; i < NUM; i++) {
        if (mTimer[i] > 0) {
            mX[i] += mVX[i];
            mY[i] += mVY[i];
            mTimer[i]--;
        }
    }
}

void Bubbles::Draw(u32 alpha, u32 height) {
    Draw2D_SetupGX();
    Draw2D_SetOrtho();
    GXSetScissor(0, 0, gRenderMode.fbWidth, height);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE,
                  GX_AF_NONE);
    GXSetNumChans(1);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE,
                      GX_PTIDENTITY);
    GXSetNumTexGens(1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_C0);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_A0);
    GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);

    for (s32 i = 0; i < NUM; i++) {
        if (mTimer[i] <= 0) {
            continue;
        }

        GXColor color;
        if (mType[i] == TYPE_GRAY) {
            color.r = 160;
            color.g = 160;
            color.b = 160;
        } else {
            color.r = 140;
            color.g = 203;
            color.b = 154;
        }

        f32 outer;
        if (mType[i] == TYPE_RING) {
            outer = mSize[i] * nw4r::math::SinFIdx(
                                   NW4R_MATH_DEG_TO_FIDX(90.0f * (1.0f - (f32)mTimer[i] / (f32)mDuration[i])));
            if (mTimer[i] < 30) {
                color.a = (f32)alpha * (63.0f * mTimer[i]) / 7650.0f;
            } else {
                color.a = 63.0f * alpha / 255.0f;
            }
        } else {
            f32 fade;
            if (mTimer[i] < 30) {
                fade = mTimer[i] / 30.0f;
            } else if (mTimer[i] > mDuration[i] - 30) {
                fade = (mDuration[i] - mTimer[i]) / 30.0f;
            } else {
                fade = 1.0f;
            }
            outer = mSize[i];
            color.a = 31.0f * fade * alpha / 255.0f;
        }
        f32 inner = 0.85f * outer;

        GXSetTevColor(GX_TEVREG0, color);
        GXBegin(GX_QUADS, GX_VTXFMT0, 128 * 4);
        for (s32 j = 0; j < 128; j++) {
            u16 a0 = (j << 16) / 128;
            u16 a1 = ((j + 1) << 16) / 128;
            f32 sin0 = SinIdx(a0);
            f32 cos0 = CosIdx(a0);
            f32 sin1 = SinIdx(a1);
            f32 cos1 = CosIdx(a1);
            GXPosition2f32(mX[i] + outer * cos0, mY[i] + outer * sin0);
            GXPosition2f32(mX[i] + inner * cos0, mY[i] + inner * sin0);
            GXPosition2f32(mX[i] + inner * cos1, mY[i] + inner * sin1);
            GXPosition2f32(mX[i] + outer * cos1, mY[i] + outer * sin1);
        }
        GXEnd();
    }
}

void Bubbles::AddRing(f32 x, f32 y) {
    for (s32 i = 0; i < NUM; i++) {
        if (mTimer[i] == 0) {
            mType[i] = TYPE_RING;
            mX[i] = x;
            mY[i] = y;
            mVX[i] = 0.0f;
            mVY[i] = 0.0f;
            mSize[i] = 501.6f;
            mTimer[i] = 60;
            mDuration[i] = 60;
            return;
        }
    }
}
