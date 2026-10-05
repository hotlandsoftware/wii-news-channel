// The nw4r::math inline-asm helpers (VEC3Dot, VEC3Sub) allocate their work
// registers in this file in the opposite order from the NW4R libraries.
#define NW4R_MATH_WORK_REGS_REVERSED
#include <news/GlobePin.h>
#include <news/Camera.h>
#include <news/Common.h>
#include <news/Draw2D.h>
#include <news/Message.h>
#include <news/NewsArticle.h>
#include <news/NewsData.h>
#include <news/System.h>
#include <nw4r/math/math_arithmetic.h>
#include <nw4r/math/math_triangular.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_Font.h>
#include <nw4r/ut/ut_TextWriterBase.h>
#include <revolution/gx.h>
#include <revolution/mtx.h>
#include <wchar.h>

using namespace nw4r;

// Latest pointer position of a channel (the same helpers as in Connect.cpp).
static inline f32 GetCursorX(s32 chan) { return gCursorX[chan][0]; }
static inline f32 GetCursorY(s32 chan) { return gCursorY[chan][0]; }

// Not yet decompiled: globals of the globe screen.
struct GlobeView {
    u8 unk0[4];
    Camera* mCamera;  // at 0x4
};

extern s32 lbl_80357598;           // view mode (1: pins only)
extern s32 lbl_803575A8;
extern u8 lbl_803575BD;            // input locked
extern f32 lbl_803575DC;           // maximum location name width
extern ut::Font* gCityFont;     // label font
extern GlobeView* gGlobe;
extern wchar_t gTextBuf[392];  // label text buffer

extern const wchar_t* gMsgOtherAreas[7];
extern const wchar_t* gMsgOtherAreasShort[7];

// Picture shown on the cards of articles without one, per language.
static const u32 sNoPictureTex[7] = {74, 76, 72, 71, 75, 73, 70};

static inline f32 SinIdx(u16 idx) {
    return math::SinFIdx((1.0f / 256.0f) * math::U16ToF32(idx));
}

static inline f32 CosIdx(u16 idx) {
    return math::CosFIdx((1.0f / 256.0f) * math::U16ToF32(idx));
}

static inline NewsTexture* GetPictureTexture(NewsArticle* article) {
    return article->mPicture != NULL ? article->mPicture->texture : NULL;
}

GlobePin::GlobePin(s32 arg1, s32 arg2, NewsArticle* article, f32 radius)
    : GlobePoint(article->mLocation, radius), mNext(NULL), m2C(0), mArticle(article),
      mState(NULL) {
    mHeadlineLen = 0;
    mPicX = 0.0f;
    mPicY = 0.0f;
    mC0 = 0.0f;
    mPicScale = 0.0f;
    mCC = 0;
    mD0 = 0;
    mD4 = 0;
    mLocationLen = 0;
    mDC = arg1;
    mE0 = arg2;
    mE4 = 0;
    mPressedChan = -1;
    mPhase = 0;
    mCount = 0;
    mActive = false;
    if (IsErrorState()) {
        return;
    }

    mHeadlineLen = mArticle->mText->size >> 1;
    mCC = mArticle->unk58;
    mLocationLen = mD0 = mArticle->unk5C;
    if (gUpdateMsgType == 1) {
        mLocationLen += wcslen(gMsgOtherAreas[gLanguage]);
    } else {
        mLocationLen += wcslen(gMsgOtherAreasShort[gLanguage]);
    }
    mHoverTime = 0;

    math::VEC3 right;
    math::VEC3 up;
    math::VEC3 dir;
    Vec pos = GlobePoint::GetPos();
    Mtx mtx;
    PSVECNormalize(&pos, &dir);
    if (0.0f == dir.x && 0.0f == dir.z) {
        up.x = 0.0f;
        up.y = 0.0f;
        up.z = 1.0f;
    } else {
        up.x = 0.0f;
        up.y = 1.0f;
        up.z = 0.0f;
    }
    PSVECCrossProduct(&up, &dir, &right);
    PSVECCrossProduct(&dir, &right, &up);
    PSVECNormalize(&right, &right);
    PSVECNormalize(&up, &up);
    mtx[0][0] = right.x;
    mtx[1][0] = right.y;
    mtx[2][0] = right.z;
    mtx[0][1] = up.x;
    mtx[1][1] = up.y;
    mtx[2][1] = up.z;
    mtx[0][2] = dir.x;
    mtx[1][2] = dir.y;
    mtx[2][2] = dir.z;
    mtx[0][3] = 0.0f;
    mtx[1][3] = 0.0f;
    mtx[2][3] = 0.0f;
    C_QUATMtx(&mQuat, mtx);

    mCurQuat = mQuat;
    f32 r = mRadius;
    mCardDist = r;
    mLabelPos.x = 0.0f;
    mLabelPos.y = 0.0f;
    mLabelW = 0.0f;
    mLabelH = 0.0f;
    mLabelHidden = false;
    mLabelAlpha = 255;
    ResetRipples();
    ChangeState(&GlobePin::StateHidden);
}

// The original's copy is not scheduled (two words, then one), like
// Camera::GetRot() in Globe.cpp.
#pragma scheduling off
inline math::VEC3 GlobePoint::GetPos() const {
    return mPos;
}
#pragma scheduling reset

GlobePin::~GlobePin() {}

void GlobePin::Draw(u8 alpha) {
    if (lbl_80357598 == 1 || lbl_803575A8 > 0) {
        static ut::Color sRippleColor(255, 255, 255, 0);
        ut::Color color(0, 0, 0, 0);
        Ripple* ripple = mRipples;
        math::VEC3 pos(0.0f, 0.0f, 0.0f);
        f32 halfW = 0.5f * TPL_GetWidth(gCommonTpl, 0x52);
        f32 halfH = 0.5f * TPL_GetHeight(gCommonTpl, 0x52);
        f32 scale;
        f32 w;
        f32 h;
        f32 k = 1.5f;
        Draw2D_SetupGX();
        Draw2D_SetOrtho();
        GXSetZMode(GX_TRUE, GX_LEQUAL, GX_FALSE);
        for (s32 i = 0; i < 2; i++, ripple++) {
            if (ripple->mScale > 0.0f && ripple->mAlpha != 0) {
                scale = ripple->mScale;
                scale *= k;
                w = halfW * scale;
                h = halfH * scale;
                color.a = ripple->mAlpha;
                pos.x = GetPos().x - w;
                pos.y = GetPos().y - h;
                GXSetTevColor(GX_TEVREG0, color);
                GXSetTevColor(GX_TEVREG1, sRippleColor);
                Draw2D_Tex(gCommonTpl, 0x52, &pos, scale, scale);
            }
        }
    } else {
        DrawCards(alpha);
    }
}

math::VEC2 GlobePin::GetPos() {
    return mScreenPos;
}

static inline void SetTevColorAlpha(GXTevRegID reg, u8 a) {
    GXColor color = ut::Color(0);
    color.a = a;
    GXSetTevColor(reg, color);
}

static inline void SetTevWhite(GXTevRegID reg, u8 a) {
    GXColor color = {255, 255, 255, a};
    GXSetTevColor(reg, color);
}

static inline void SetTevBlack(GXTevRegID reg) {
    GXSetTevColor(reg, (GXColor)ut::Color(0));
}

static inline void SetTevWhiteClear(GXTevRegID reg) {
    const GXColor white = {255, 255, 255, 0};
    GXSetTevColor(reg, white);
}

BOOL GlobePin::DrawCards(u8 alpha) {
    BOOL drawn = FALSE;
    Camera* camera = gGlobe->mCamera;
    camera->GetG3dCamera().GXSetViewport();
    camera->GetG3dCamera().GXSetProjection();
    math::MTX34 viewMtx;
    camera->GetG3dCamera().GetCameraMtx(&viewMtx);
    Draw2D_SetupGX();
    s32 i = 0;
    GXSetZScaleOffset(1.0f, mCardAlpha < 160 ? -0.1f : -0.01f);
    for (GlobePin* pin = this; pin != NULL; pin = pin->mNext) {
        drawn = TRUE;
        NewsTexture* tex = GetPictureTexture(pin->mArticle);
        f32 w = pin->mCardW;
        f32 h = pin->mCardH;
        f32 halfW = 0.5f * w;
        f32 halfH = 0.5f * h;
        f32 border = 0.17f * (w <= h ? w : h);
        Mtx mtx;
        PSMTXConcat(viewMtx.mtx, pin->mCardMtx.mtx, mtx);
        GXLoadPosMtxImm(mtx, GX_PNMTX0);
        GXSetCurrentMtx(GX_PNMTX0);
        if (i == mCount - 1) {
            SetTevColorAlpha(GX_TEVREG0, mCardAlpha);
            SetTevBlack(GX_TEVREG1);
            GXSetZMode(GX_TRUE, GX_LEQUAL, GX_FALSE);
            GXTexObj texObj;
            TPL_GetTexObj(gCommonTpl, 0x53, &texObj);
            GXLoadTexObj(&texObj, GX_TEXMAP0);
            GXBegin(GX_QUADS, GX_VTXFMT0, 4);
            GXPosition3f32(-halfW + border, halfH - border, -0.001f);
            GXTexCoord2f32(0.0f, 0.0f);
            GXPosition3f32(halfW + border, halfH - border, -0.001f);
            GXTexCoord2f32(1.0f, 0.0f);
            GXPosition3f32(halfW + border, -halfH - border, -0.001f);
            GXTexCoord2f32(1.0f, 1.0f);
            GXPosition3f32(-halfW + border, -halfH - border, -0.001f);
            GXTexCoord2f32(0.0f, 1.0f);
            GXEnd();
            GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
        }
        if (tex != NULL) {
            GXTexObj texObj;
            GXInitTexObj(&texObj, tex->data, tex->width, tex->height, (GXTexFmt)tex->format,
                         GX_CLAMP, GX_CLAMP, GX_FALSE);
            GXLoadTexObj(&texObj, GX_TEXMAP0);
            SetTevWhite(GX_TEVREG0, alpha);
            SetTevBlack(GX_TEVREG1);
        } else {
            GXTexObj texObj;
            TPL_GetTexObj(gCommonTpl, sNoPictureTex[gLanguage], &texObj);
            GXLoadTexObj(&texObj, GX_TEXMAP0);
            SetTevWhiteClear(GX_TEVREG0);
            SetTevColorAlpha(GX_TEVREG1, alpha);
        }
        GXBegin(GX_QUADS, GX_VTXFMT0, 4);
        GXPosition3f32(-halfW, halfH, 0.0f);
        GXTexCoord2f32(0.0f, 0.0f);
        GXPosition3f32(halfW, halfH, 0.0f);
        GXTexCoord2f32(1.0f, 0.0f);
        GXPosition3f32(halfW, -halfH, 0.0f);
        GXTexCoord2f32(1.0f, 1.0f);
        GXPosition3f32(-halfW, -halfH, 0.0f);
        GXTexCoord2f32(0.0f, 1.0f);
        GXEnd();
        i++;
    }
    GXSetZScaleOffset(1.0f, 0.0f);
    return drawn;
}

void GlobePin::DrawLabel() {
    ut::TextWriterBase<wchar_t> writer;
    f32 scale = 1.0f;
    f32 t = 0.5f - 0.5f * math::CosDeg(15.0f * mHoverTime);
    if (lbl_80357598 == 1) {
        t = 1.0f;
    }
    if (gNewsData->mHeader->language == 0) {
        scale *= 0.9f;
    } else {
        scale *= 0.75f;
    }
    scale *= 1.0f + 0.2f * t;

    if (mNext != NULL) {
        BOOL same = TRUE;
        const wchar_t* name = mArticle->mLocationName;
        GlobePin* pin = mNext;
        do {
            if (wcscmp(name, pin->mArticle->mLocationName) != 0) {
                same = FALSE;
                break;
            }
            pin = pin->mNext;
        } while (pin != NULL);
        if (same) {
            swprintf(gTextBuf, 256, L"%ls [%d]", name, mCount);
        } else {
            switch (gLanguage) {
            case 1:
            case 3:
            case 4:
                if (gUpdateMsgType == 1) {
                    wcscpy(gTextBuf, name);
                    wcscat(gTextBuf, gMsgOtherAreas[gLanguage]);
                } else {
                    wcscpy(gTextBuf, name);
                    wcscat(gTextBuf, gMsgOtherAreasShort[gLanguage]);
                }
                break;
            case 0:
            case 2:
            case 5:
            case 6:
            default:
                wcscpy(gTextBuf, name);
                wcscat(gTextBuf, gMsgOtherAreasShort[gLanguage]);
                break;
            }
            swprintf(gTextBuf, 256, L"%ls [%d]", gTextBuf, mCount);
        }
    } else {
        wcscpy(gTextBuf, mArticle->mLocationName);
    }

    writer.SetFont(*gCityFont);
    writer.SetDrawFlag(0x11);
    writer.SetScale(scale);
    writer.SetCharSpace(0.0f);
    writer.SetTextColor(ut::Color(255, 255, 255, mLabelAlpha));
    mLabelW = writer.CalcStringWidth(gTextBuf);
    mLabelH = writer.CalcStringHeight(gTextBuf);

    if (mHoverTime != 0) {
        f32 x = mLabelPos.x;
        f32 y = mLabelPos.y;
        u8 a = mLabelAlpha * t;
        f32 h = mLabelH;
        f32 halfW = 64.0f + 0.5f * mLabelW;
        ut::Color colors[4];
        colors[0].r = 255;
        colors[0].g = 136;
        colors[0].b = 75;
        colors[1] = ut::Color(255, 136, 75, 0);
        colors[2] = ut::Color(255, 136, 75, 0);
        colors[3].r = 255;
        colors[3].g = 136;
        colors[3].b = 75;
        colors[0].a = a;
        colors[3].a = a;
        Draw2D_SetupGX();
        Draw2D_SetOrtho();
        GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
        math::VEC3 quad[4];
        quad[0].x = x;
        quad[0].y = y;
        quad[1].x = x + halfW;
        quad[1].y = y;
        quad[2].x = x + halfW;
        quad[2].y = y + h;
        quad[3].x = x;
        quad[3].y = y + h;
        Draw2D_FillQuadGradient(quad, colors);
        quad[2].x = quad[1].x = x - halfW;
        Draw2D_FillQuadGradient(quad, colors);
    }

    if (lbl_80357598 == 1 || lbl_803575A8 > 0) {
        f32 offset = 0.5f * TPL_GetHeight(gCommonTpl, 0x52);
        writer.SetCursor(GetScreenPos().x, offset + GetScreenPos().y);
    } else {
        writer.SetCursor(mLabelPos.x, mLabelPos.y);
    }
    Draw2D_SetupGX();
    Draw2D_SetOrtho();
    writer.SetupGX();
    GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
    writer.Print(gTextBuf);
}

void GlobePin::DrawName() {
    ut::TextWriterBase<wchar_t> writer;
    f32 scale = 1.1f;
    if (gNewsData->mHeader->language == 0) {
        scale *= 0.9f;
    } else {
        scale *= 0.75f;
    }
    writer.SetFont(*gCityFont);
    writer.SetDrawFlag(0x11);
    writer.SetScale(scale);
    writer.SetCharSpace(0.0f);
    writer.SetTextColor(ut::Color(255, 255, 255, 255));
    if (lbl_80357598 == 1 || lbl_803575A8 > 0) {
        f32 offset = 0.5f * TPL_GetHeight(gCommonTpl, 0x52);
        writer.SetCursor(GetScreenPos().x, offset + GetScreenPos().y);
    } else {
        writer.SetCursor(mLabelPos.x, mLabelPos.y);
    }
    Draw2D_SetupGX();
    Draw2D_SetOrtho();
    writer.SetupGX();
    wcscpy(gTextBuf, mArticle->mLocationName);
    f32 width = writer.CalcStringWidth(gTextBuf);
    if (width > lbl_803575DC) {
        writer.SetScale(scale * (lbl_803575DC / width), scale);
    }
    writer.Print(gTextBuf);
}

void GlobePin::DrawHeadline(ut::CharWriter* writer) {
    const ut::Font* font = writer->GetFont();
    f32 right = GetScreenWidth();
    f32 right2;
    f32 scaleH = writer->GetScaleH();
    f32 ascent = scaleH * font->GetAscent();
    f32 space = ((ut::TextWriterBase<wchar_t>*)writer)->GetCharSpace();
    f32 y = writer->GetCursorY();
    u32 n = 0;
    const wchar_t* str = mArticle->unk3C;
    f32 x = writer->GetCursorX();
    y = y - 0.5f * (scaleH * font->GetHeight());
    writer->SetCursorY(y);
    f32 offset = ascent - ascent;
    while (*str != 0) {
        if (x > right) {
            break;
        }
        if (n >= mHeadlineLen) {
            ((ut::TextWriterBase<wchar_t>*)writer)->SetCharSpace(space);
            writer->SetScale(scaleH);
            writer->SetCursorY(y + offset);
            right2 = GetScreenWidth();
            // The original reuses y for the cursor x here.
            y = writer->GetCursorX();
            while (*str != 0) {
                if (y > right2) {
                    break;
                }
                if (*str == 0xA0) {
                    writer->Print(L' ');
                } else {
                    writer->Print(*str);
                }
                y = space + writer->GetCursorX();
                writer->SetCursorX(y);
                str++;
            }
            return;
        }
        if (*str == 0xA0) {
            writer->Print(L' ');
        } else {
            writer->Print(*str);
        }
        x = space + writer->GetCursorX();
        writer->SetCursorX(x);
        str++;
        n++;
    }
}

void GlobePin::Update(Camera* camera) {
    mOpened = false;
    mC0 = 1.0f;
    mPressedChan = -1;
    mHover[0] = false;
    mHover[1] = false;
    mHover[2] = false;
    mHover[3] = false;
    CalcScreenPos(camera);
    math::VEC3 pos = GlobePoint::GetPos();
    f32 minY = 63.0f;
    f32 maxY = 393.0f;
    math::VEC3 dir = camera->mDir;
    PSVECNormalize(&pos, &pos);
    PSVECNormalize(&dir, &dir);
    if (math::VEC3Dot(&pos, &dir) < 0.0f) {
        if (mActive && !lbl_803575BD) {
            mE4 = 0xFF;
            for (s32 i = 0; i < 4; i++) {
                if (IsPointerValid(i)) {
                    math::VEC2 cursor(GetCursorX(i), GetCursorY(i));
                    if (cursor.y > minY && cursor.y < maxY) {
                        math::VEC2 screen = GetPos();
    f32 maxDist = 35.0f;
                        math::VEC2 d;
                        d.x = cursor.x - screen.x;
                        d.y = cursor.y - screen.y;
                        if (math::FSqrt(d.x * d.x + d.y * d.y) < maxDist) {
                            mHover[i] = true;
                            if (gTrig[i] & 0x800) {
                                mPressedChan = i;
                                break;
                            }
                        }
                    }
                }
            }
            if (mHover[0] || mHover[1] || mHover[2] || mHover[3]) {
                mHoverTime++;
                if (mHoverTime > 12) {
                    mHoverTime = 12;
                } else if (mHoverTime == 12) {
                    mOpened = true;
                }
            } else if (mHoverTime != 0) {
                mHoverTime--;
            }
        }
    } else {
        mHoverTime = 0;
        mActive = false;
        mE4 = 0xFF;
        ChangeState(&GlobePin::StateHidden);
    }
    if (mState) {
        (this->*mState)();
    }
}

void GlobePin::UpdateCards(f32 alpha) {
    Quaternion* quat;
    math::MTX34* mtx;
    s32 i;
    Camera* camera = gGlobe->mCamera;
    s32 picIndex = -1;
    f32 distance = camera->mDistance;
    f32 zoom = 0.00019f * distance;
    if (mCount == 0) {
        return;
    }
    f32 t = 0.5f * (1.0f - math::CosDeg(15.0f * mHoverTime));
    f32 size = 50.0f * (1.0f + 1.3f * t) * zoom;
    math::MTX34 camMtx;
    camera->GetG3dCamera().GetCameraMtx(&camMtx);
    PSMTXTranspose(camMtx.mtx, camMtx.mtx);
    Quaternion camQuat;
    C_QUATMtx(&camQuat, camMtx.mtx);
    mCardAlpha = (160.0f - 160.0f * t) * alpha;

    // The first card with a picture goes to the front. (This loop has a
    // counter of its own.)
    s32 n = 0;
    for (GlobePin* pin = this; pin != NULL; pin = pin->mNext, n++) {
        if (GetPictureTexture(pin->mArticle) != NULL) {
            picIndex = n;
            break;
        }
    }

    i = 0;
    for (GlobePin* pin = this; pin != NULL; pin = pin->mNext, i++) {
        quat = &pin->mCurQuat;
        mtx = &pin->mCardMtx;
        NewsTexture* tex = GetPictureTexture(pin->mArticle);
        f32 w;
        f32 h;
        if (tex != NULL) {
            w = tex->width;
            h = tex->height;
        } else {
            w = TPL_GetWidth(gCommonTpl, sNoPictureTex[gLanguage]);
            h = TPL_GetWidth(gCommonTpl, sNoPictureTex[gLanguage]);
        }
        f32 s;
        if (w < h) {
            s = size / h;
        } else {
            s = size / w;
        }
        if (tex == NULL) {
            s *= 0.95f;
        }
        pin->mCardW = w * s;
        pin->mCardH = h * s;
        f32 order;
        if (i == picIndex) {
            order = 0.0f;
        } else if (i == 0 && picIndex >= 0) {
            order = picIndex;
        } else {
            order = i;
        }
        f32 target = 1.0f + 0.5f * (zoom * ((mCount - order) - 1.0f));
        target *= mRadius;
        f32 dist = 0.9f * pin->mCardDist + 0.1f * target;
        pin->mCardDist = dist;
        C_QUATSlerp(quat, &mQuat, quat, 0.1f);
        PSMTXQuat(mtx->mtx, quat);
        f32 x = mtx->m[0][2] * dist;
        f32 y = mtx->m[1][2] * dist;
        f32 z = mtx->m[2][2] * dist;
        Quaternion q;
        C_QUATSlerp(quat, &camQuat, &q, t);
        PSMTXQuat(mtx->mtx, &q);
        mtx->m[0][3] = x;
        mtx->m[1][3] = y;
        mtx->m[2][3] = z;
        math::VEC3 labelPos(x - 0.5f * (mtx->m[0][1] * size), y - 0.5f * (mtx->m[1][1] * size),
                            z - 0.5f * (mtx->m[2][1] * size));
        gGlobe->mCamera->Project(&mLabelPos, &labelPos);
    }

    s32 a = mLabelAlpha;
    if (mLabelHidden) {
        a -= 32;
        if (a < 0) {
            a = 0;
        }
    } else {
        a += 32;
        if (a > 255) {
            a = 255;
        }
    }
    mLabelAlpha = a * alpha;
    mLabelHidden = false;
}

void GlobePin::StateHidden() {
    switch (mPhase) {
    case 0:
        mPhase++;
        ResetRipples();
        break;
    case -1:
        break;
    default:
        if (mActive) {
            ChangeState(&GlobePin::StateRipple);
        }
        break;
    }
}

void GlobePin::StateRipple() {
    switch (mPhase) {
    case 0:
        mPhase++;
        ResetRipples();
        break;
    case -1:
        break;
    default: {
        s32 i;
        Ripple* ripple = mRipples;
        switch (mPhase) {
        case 1:
            for (i = 0; i < 2; i++, ripple++) {
                ripple->mAngle += 0x100;
                if (ripple->mAngle >= 0x4000) {
                    ripple->mAngle = 0;
                }
                u16 angle = ripple->mAngle < 0 ? (u16)0 : (u16)ripple->mAngle;
                ripple->mScale = SinIdx(angle);
                ripple->mAlpha = 255.0f * CosIdx(angle);
            }
            if (!mActive) {
                mPhase++;
            }
            break;
        case 2:
        default: {
            u16 angle;
            BOOL done = TRUE;
            for (i = 0; i < 2; i++, ripple++) {
                ripple->mAngle += 0x100;
                if (ripple->mAngle < 0 || ripple->mAngle >= 0x4000) {
                    angle = 0;
                } else {
                    done = FALSE;
                    angle = ripple->mAngle;
                }
                ripple->mScale = SinIdx(angle);
                ripple->mAlpha = 255.0f * CosIdx(angle);
            }
            if (done) {
                ChangeState(&GlobePin::StateHidden);
            } else if (mActive) {
                ResetRipples();
                mPhase = 1;
            }
            break;
        }
        }
        break;
    }
    }
}

void GlobePin::TruncateHeadline(ut::CharWriter* writer) {
    if (mHeadlineScroller.mMode == Scroller::MODE_WAIT) {
        const ut::Font* font = writer->GetFont();
        const wchar_t* src = mArticle->mHeadline;
        wchar_t* dst = mArticle->unk3C;
        f32 scale = writer->GetScaleH();
        f32 space = ((ut::TextWriterBase<wchar_t>*)writer)->GetCharSpace();
        u32 n = 0;
        f32 width = 0.0f;
        f32 maxWidth = mHeadlineScroller.mViewWidth - 30.0f * scale;
        f32 limit;
        if (gLanguage == 0) {
            while (*src != 0) {
                *dst = *src++;
                f32 cw;
                if (n < mHeadlineLen) {
                    cw = scale * font->GetCharWidth(*dst);
                } else {
                    f32 s = scale;
                    cw = s * font->GetCharWidth(*dst);
                }
                width += cw;
                *++dst = 0;
                if (width > maxWidth) {
                    // Both cases use the same scale; the original keeps the compare.
                    f32 s = scale;
                    if (n < mHeadlineLen) {
                        s = scale;
                    }
                    limit = s * font->GetCharWidth(0x2026);
                    dst[-1] = 0;
                    dst -= 2;
                    f32 cut = scale * font->GetCharWidth(*dst);
                    while (cut < limit) {
                        cut += space + scale * font->GetCharWidth(*--dst);
                    }
                    dst[0] = 0x2026;
                    dst[1] = 0;
                    return;
                }
                width += space;
                n++;
            }
        } else {
            while (*src != 0) {
                *dst = *src++;
                f32 cw;
                if (n < mHeadlineLen) {
                    cw = scale * font->GetCharWidth(*dst);
                } else {
                    f32 s = scale;
                    cw = s * font->GetCharWidth(*dst);
                }
                width += cw;
                *++dst = 0;
                if (width > maxWidth) {
                    // Both cases use the same scale; the original keeps the compare.
                    f32 s = scale;
                    if (n < mHeadlineLen) {
                        s = scale;
                    }
                    limit = 2.0f * space + s * (3.0f * font->GetCharWidth(L'.'));
                    dst[-1] = 0;
                    dst -= 2;
                    f32 cut = scale * font->GetCharWidth(*dst);
                    while (cut < limit) {
                        cut += space + scale * font->GetCharWidth(*--dst);
                    }
                    dst[0] = 0;
                    wcscat(mArticle->unk3C, L"...");
                    return;
                }
                width += space;
                n++;
            }
        }
        return;
    }
    wcscpy(mArticle->unk3C, mArticle->mHeadline);
}

void GlobePin::TruncateLocation(ut::CharWriter* writer) {
    if (mLocationScroller.mMode == Scroller::MODE_WAIT) {
        f32 width;
        f32 maxWidth;
        f32 limit;
        f32 scale;
        f32 space;
        const ut::Font* font = writer->GetFont();
        maxWidth = mLocationScroller.mViewWidth;
        const wchar_t* src = mArticle->mLocationName;
        wchar_t* dst = mArticle->unk40;
        scale = writer->GetScaleH();
        space = ((ut::TextWriterBase<wchar_t>*)writer)->GetCharSpace();
        width = 0.0f;
        if (gLanguage == 0) {
            while (*src != 0) {
                *dst = *src++;
                width += scale * font->GetCharWidth(*dst);
                *++dst = 0;
                if (width > maxWidth) {
                    limit = scale * font->GetCharWidth(0x2026);
                    dst[-1] = 0;
                    dst -= 2;
                    f32 cut = scale * font->GetCharWidth(*dst);
                    while (cut < limit) {
                        cut += space + scale * font->GetCharWidth(*--dst);
                    }
                    dst[0] = 0x2026;
                    dst[1] = 0;
                    return;
                }
                width += space;
            }
        } else {
            while (*src != 0) {
                *dst = *src++;
                width += scale * font->GetCharWidth(*dst);
                *++dst = 0;
                if (width > maxWidth) {
                    limit = 2.0f * space + scale * (3.0f * font->GetCharWidth(L'.'));
                    dst[-1] = 0;
                    dst -= 2;
                    f32 cut = scale * font->GetCharWidth(*dst);
                    while (cut < limit) {
                        cut += space + scale * font->GetCharWidth(*--dst);
                    }
                    dst[0] = 0;
                    wcscat(mArticle->unk40, L"...");
                    return;
                }
                width += space;
            }
        }
        return;
    }
    wcscpy(mArticle->unk40, mArticle->mLocationName);
}

f32 GlobePin::CalcHeadlineWidth(const wchar_t* str, const ut::Font* font, f32 scale, f32 space) {
    const wchar_t* p = str;
    u32 len = wcslen(p);
    f32 width = 0.0f;
    for (u32 n = 0; *p != 0; p++, n++) {
        if (n >= mHeadlineLen) {
            width += scale * font->GetCharWidth(*p);
        } else {
            width = width + scale * font->GetCharWidth(*p);
        }
    }
    if (len >= mHeadlineLen) {
        u32 count = mHeadlineLen - 1;
        len -= count;
        width += space * count;
    }
    width += space * len;
    return width;
}

void GlobePin::LayoutPicture(f32 size) {
    NewsTexture* tex = GetPictureTexture(mArticle);
    if (tex != NULL) {
        f32 w = tex->width;
        f32 h = tex->height;
        f32 one = 1.0f;
        mPicScale = h / w > one ? size / h : size / w;
        mPicY = -(0.5f * (h * mPicScale));
        mPicX = 0.5f * (size - w * mPicScale);
    }
}

s32 GlobePin::CompareLabel(GlobePin* other) {
    s32 result = 0;
    if (!mLabelHidden) {
        f32 dx = other->mLabelPos.x - mLabelPos.x;
        f32 w1 = other->mLabelW;
        if (__fabsf(dx) < 0.5f * (mLabelW + w1)) {
            f32 h1 = other->mLabelH;
            f32 h0 = mLabelH;
            if (__fabsf(((other->mLabelPos.y + 0.5f * h1) - mLabelPos.y) - 0.5f * h0) <
                0.5f * (h0 + h1)) {
                if (other->mHoverTime > mHoverTime) {
                    result = -1;
                } else if (other->mHoverTime < mHoverTime) {
                    result = 1;
                } else {
                    result = 1;
                    if (other->mCount > mCount) {
                        result = -1;
                    }
                }
            }
        }
    }
    return result;
}
