// The default ut::Color constructor of this file stores white (see the
// out-of-line copy used by __construct_array).
#define NW4R_UT_COLOR_DEFAULT_WHITE
#define NW4R_UT_COLOR_WORD_COPY

#include <news/PointerHistory.h>
#include <news/PaneLayout.h>
#include <news/SlideShow.h>
#include <news/d_s_news.h>
#include <news/ArticleText.h>
#include <news/Common.h>
#include <news/Draw2D.h>
#include <news/MathUtil.h>
#include <news/NewsArticle.h>
#include <news/NewsData.h>
#include <news/PaneButton.h>
#include <news/System.h>
#include <nw4r/math/math_triangular.h>
#include <nw4r/ut/ut_CharWriter.h>
#include <nw4r/ut/ut_Font.h>

using namespace nw4r;

// Not yet decompiled: layouts (0x80047B50), the article view (0x8002E7DC),
// the globe (0x8004BD60) and input globals.
extern u8 lbl_801EE270[];         // layout resource accessor
extern "C" s32 gHoverButtons[4];   // per-channel "pointer used" flags
extern "C" f32 gPointerScroll[];    // pointer movement
extern "C" s32 lbl_801EDFD0[4];
extern "C" f32 lbl_801EDFA0[6];
extern "C" f32 lbl_801EDFB8[6];
extern "C" f32 lbl_803575D0;
extern "C" GXColor lbl_80357600;
extern "C" u8 lbl_803575BA;
extern "C" u8 lbl_803575BB;
extern "C" s32 lbl_80357598;
extern "C" s32 lbl_803575E0;      // number of categories
extern "C" u8 lbl_8035697C;
extern "C" s32 lbl_80356970;      // text size setting
extern "C" bool gHideClock;
extern "C" u8 gPointerOverClock;
extern "C" const wchar_t* lbl_80357564; // title text
extern "C" ArticleText* lbl_80357568;
extern "C" const f32 lbl_801922D0[]; // text scale per setting
extern "C" const s32 lbl_80192370[];
extern "C" const s32 lbl_80192398[];

struct MenuState {
    u8 unk0[0x60];
    s32 mCategory; // at 0x60
};
extern "C" MenuState* lbl_8035755C;

struct GlobeCamera {
    u8 unk0[0x90];
    f32 mLon; // at 0x90
    f32 mLat; // at 0x94
};
// Globe view (Globe.cpp, include/news/Globe.h; fields as used here).
struct Globe {
    void Calc();
    void ApplyCamera();
    void CalcPoles();
    void UpdateLights();
    void CalcScene();
    void UpdateZoom(const s32* se);
    void SetTiltNow(s32 level);
    void UpdateTilt(s32 unused, const s32* se);
    void UpdateSpin(u32 stop);
    void SetZoom(s32 level);

    u8 unk0[0x4];
    GlobeCamera* mCamera; // at 0x04
    f32 mLon;             // at 0x08
    f32 mLat;             // at 0x0C
    u8 unk10[0x74 - 0x10];
    f32 mZoom;            // at 0x74
};
extern "C" Globe* gGlobe;

void DrawScreenFade(s32 alpha);


extern "C" {


void fn_8001F730(nw4r::lyt::Pane* pane, const nw4r::ut::Color& color);
// DrawPointerEffect(u8, u16); the call passes a u32 height without truncating it.
void DrawPointerEffect__FUcUs(u8 alpha, u32 y);
// DrawTabRect(const ut::Rect&, u8, f32); the call passes an s32 alpha without
// truncating it.
void DrawTabRect__FRCQ34nw4r2ut4RectUcf(const nw4r::ut::Rect& rect, s32 alpha, f32 z);
}

// OSu16tof32: u16 to f32 through the paired-single unit (GQR3 = u16).
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

static inline f32 FIdxRad(f32 rad) {
    return 40.743664f * rad;
}

static inline s32 GetFrameRate() {
    return gRenderMode.viTVmode == 4 ? 50 : 60;
}

#pragma explicit_zero_data on
static f32 sUnused[3] = {5.0f, 25.0f, 50.0f};
#pragma explicit_zero_data reset

static ut::Color sWhite(0xFFFFFFFF);

static math::VEC2 sArrowSize(20.0f, 20.0f);

// Tweakable values (.sdata). They are emitted where they are defined, so
// each one sits next to the function that uses it; the footer colour and the
// picture scale are defined after their first use.
extern u8 sFooterRed;
extern u8 sFooterGreen;
extern u8 sFooterBlue;
extern f32 sPictureScale;

static f32 sTitleY = 45.0f;

static inline Category* GetCategory(s32 idx) {
    return &gNewsData->mCategories[idx];
}

static inline NewsTexture* GetPictureTexture(NewsArticle* article) {
    return article->mPicture != NULL ? article->mPicture->texture : NULL;
}

static inline const wchar_t* GetPictureCaption(NewsArticle* article) {
    return article->mPicture != NULL ? article->mPicture->caption : NULL;
}

static inline s32 ClampZero(s32 x) {
    return x < 0 ? 0 : x;
}

static inline ut::Color operator-(const ut::Color& a, const ut::Color& b) {
    s32 al = ClampZero(a.a - b.a);
    s32 bl = ClampZero(a.b - b.b);
    s32 g = ClampZero(a.g - b.g);
    s32 r = ClampZero(a.r - b.r);
    return ut::Color(r, g, bl, al);
}

SlideShow::SlideShow(u32 arc)
    : mCurLayout(NULL), mMainLayout(NULL), mSlideLayout(NULL), mBeltLayout(NULL), mUpButton(NULL),
      mDownButton(NULL), mBackButton(NULL), mZoomInButton(NULL), mZoomOutButton(NULL),
      mEndButton(NULL), mArticle(NULL), mPrevPicture(NULL), mPrevCaption(NULL), mState(NULL),
      mSubState(NULL), mDrawFooter(NULL), mView(0.0f, 0.0f, 0.0f, 0.0f),
      mText(0.0f, 0.0f, 0.0f, 0.0f), mPicArea(0.0f, 0.0f, 0.0f, 0.0f), 
      mTitleRect(0.0f, 30.0f, 0.0f, 66.0f), mUnk204(0.0f, 0.0f, 0.0f, 0.0f),
      mUnk214(0.0f, 0.0f), mViewWidth(0.0f), mViewHeight(0.0f), mGlobeFrom(0.0f, 0.0f),
      mGlobeTo(0.0f, 0.0f), mUnk288(30.0f, 30.0f), mSelect(0.0f, 0.0f, 0.0f, 0.0f) {
    mZoomOutPressed = false;
    mPrevPressed = false;
    mNextPressed = false;
    mZoomInPressed = false;
    mBackPressed = false;
    mMainPressed = false;
    mUnk30E = false;
    mUpPressed = false;
    mDownPressed = false;
    mShowPicture = false;
    mZoomed = false;
    mTextMoving = false;
    mTextVisible = false;
    mHasTitle = false;
    mShowMain = true;
    mPlaySound = false;
    mCategory = 0;
    mArticleIdx = 0;
    mUnk328 = 99;
    mUnk32C = 0;
    mUnk330 = 0;
    mUnk334 = 0;
    mUnk338 = 0;
    mUnk33C = 10;
    mTimer = 0;
    mPointerOutTimer = 0;
    mIdleTimer = 0;
    mFooterFade = 0;
    mPicAlpha = 255;
    mPrevPicAlpha = 255;
    mSlideAngle = 0;
    mGlobeAngle = 0x8000;
    mAlpha = 0;
    mFooterAlpha = 0;
    mZoomAngle = 0;
    mZoomSpeed = 0;
    mDirection = 0;
    mUnk374 = 0;
    mUnk378 = 50.0f;
    mUnk37C = 50.0f;
    mUnk380 = 0.0f;
    mUnk384 = 0.0f;
    mScroll = 0.0f;
    mScrollTarget = 0.0f;
    mScrollSpeed = 0.0f;
    mPicScale = 1.0f;
    mGlobeZoomFrom = 0.0f;
    mGlobeZoomTo = 0.0f;
    mGlobeZoom = 0.0f;
    mTextOfs = 0.0f;
    mTitleScale = 0.75f;
    mZoomFrom[0] = 0.0f;
    mZoomFrom[1] = 0.0f;
    mZoomFrom[2] = 0.0f;
    mZoomFrom[3] = 0.0f;
    mZoomFrom[4] = 0.0f;
    mZoomFrom[5] = 0.0f;
    mZoomFrom[6] = 0.0f;
    mZoomFrom[7] = 0.0f;
    mUnk3DC = 0.0f;
    mUnk3E0 = 0.0f;
    mUnk3E4 = 0;
    mStateFrame = 0;
    mSubStateFrame = 0;
    mMessageFade = 0;
    mHoldTimer = 0;
    mMessageFlag = false;
    mLoop = false;
    mBeltFade = 15;
    mBeltVisible = true;
    mSpeed = 4;
    mBounceTimer = 0;
    mQuickMove = false;
    mZoomDone = false;
    mTimerFade = 0;

    if (IsErrorState()) {
        return;
    }

    if (gLanguage == 0) {
        mSpeed = 4;
    } else {
        mSpeed = 5;
    }

    mDragging[0] = false;
    mDragging[1] = false;
    mDragging[2] = false;
    mDragging[3] = false;

    Layout* layout = new Layout((void*)arc, "slide_main.brlyt", (PaneButtonColors*)lbl_801EE270, false);
    mMainLayout = layout;
    if (mMainLayout == NULL) {
        gAllocFailed = true;
        return;
    }

    layout = new Layout((void*)arc, "slide.brlyt", (PaneButtonColors*)lbl_801EE270, false);
    mSlideLayout = layout;
    if (mMainLayout == NULL) {
        gAllocFailed = true;
        return;
    }

    layout = new Layout((void*)arc, "slide_belt.brlyt", (PaneButtonColors*)lbl_801EE270, false);
    mBeltLayout = layout;
    if (mMainLayout == NULL) {
        gAllocFailed = true;
        return;
    }

    mUpButton = mMainLayout->FindButton("up");
    if (mUpButton == NULL) {
        gFatalError = true;
        return;
    }
    mUpButton->mUnk91 = true;

    mDownButton = mMainLayout->FindButton("down");
    if (mDownButton == NULL) {
        gFatalError = true;
        return;
    }
    mDownButton->mUnk91 = true;

    mBackButton = mMainLayout->FindButton("back");
    if (mBackButton == NULL) {
        gFatalError = true;
        return;
    }
    mBackButton->mUnk91 = true;

    mZoomInButton = mMainLayout->FindButton("zoom_in");
    if (mZoomInButton == NULL) {
        gFatalError = true;
        return;
    }
    mZoomInButton->mUnk91 = true;

    mZoomOutButton = mMainLayout->FindButton("zoom_out");
    if (mZoomOutButton == NULL) {
        gFatalError = true;
        return;
    }
    mZoomOutButton->mUnk91 = true;
    mZoomOutButton->mTextColorCallback = (PaneButton::TextColorCallback)fn_8001F730;

    mEndButton = mMainLayout->FindButton("end");
    if (mEndButton == NULL) {
        gFatalError = true;
        return;
    }
    mEndButton->mUnk91 = true;

    mCurLayout = mMainLayout;

    mWriter.SetFont(*gArticleFont);
    mWriter.SetDrawFlag(0);
    mWriter.SetScale(0.8f);
    mWriter.SetCharSpace(3.0f);

    mView.top = 0.0f;
    mView.left = 0.3f * GetScreenWidth();
    mView.bottom = GetScreenHeight();
    mViewHeight = GetScreenHeight() - mView.top;
    mView.right = GetScreenWidth();
    mViewWidth = mView.right - mView.left;
    mText.bottom = mView.bottom - 63.0f;
    mText.right = GetContentRight();
    Article_SetHeight(mText.bottom - mText.top);

    s32 numCategories = lbl_803575E0;
    while (GetCategory(mCategory)->mArticles == NULL) {
        if (++mCategory >= numCategories) {
            mCategory = 0;
            break;
        }
    }

    mPointerIn[0] = false;
    mPointerIn[1] = false;
    mPointerIn[2] = false;
    mPointerIn[3] = false;

    mDrawFooter = gLanguage == 0 ? &SlideShow::DrawFooterA : &SlideShow::DrawFooterB;

    mTitleRect.right = GetScreenWidth();
    mTitleRight = GetContentRight() - 2;
    mTitleY = sTitleY;
    mTitleMaxWidth = mTitleRight - 160.0f;
}

SlideShow::~SlideShow() {
    delete mBeltLayout;
    delete mSlideLayout;
    delete mMainLayout;
}

static inline void ApplyView(SlideShow* s) {
    s->mViewWidth = s->mView.right - s->mView.left;
    s->mViewHeight = s->mView.bottom - s->mView.top;
    s->mText.right = GetContentRight();
    s->mText.bottom = s->mView.bottom - 63.0f;
    Article_SetHeight(s->mText.bottom - s->mText.top);
}

static inline void SetViewToTarget(SlideShow* s) {
    s->mText.left = s->mTextTarget[0];
    s->mText.top = s->mTextTarget[1];
    s->mView.left = s->mViewTarget[0];
    s->mView.top = s->mViewTarget[1];
    ApplyView(s);
}

static inline void StartZoom(SlideShow* s) {
    s->mZoomSpeed = 0x800;
    s->mZoomFrom[0] = s->mView.left;
    s->mZoomFrom[1] = s->mViewTarget[0] - s->mView.left;
    s->mZoomFrom[2] = s->mView.top;
    s->mZoomFrom[3] = s->mViewTarget[1] - s->mView.top;
    s->mZoomFrom[4] = s->mText.left;
    s->mZoomFrom[5] = s->mTextTarget[0] - s->mText.left;
    s->mZoomFrom[6] = s->mText.top;
    s->mZoomFrom[7] = s->mTextTarget[1] - s->mText.top;
}

static inline void SetArticleText(SlideShow* s) {
    math::VEC2 start(0.0f, 0.0f);
    math::VEC2 size(s->mText.right - s->mText.left, s->mText.bottom - s->mText.top);
    NewsArticle* article = s->mArticle;
    Article_Set(article, GetCategory(s->mCategory)->mName, (BOOL)GetPictureTexture(article), &start,
                &start, &sPictureScale, size, true, gTextScale, false);
    Article_Arrange(gTextScale);
    Article_Reset();
}

static inline BOOL IsFirstArticle(SlideShow* s) {
    if (s->mArticleIdx == 0) {
        if (s->mLoop) {
            for (s32 cat = s->mCategory; --cat >= 0;) {
                if (GetCategory(cat)->mArticles != NULL) {
                    return FALSE;
                }
            }
            return TRUE;
        }
        return TRUE;
    }
    return FALSE;
}

static inline BOOL IsLastArticle(SlideShow* s) {
    s32 cat = s->mCategory;
    if ((u32)s->mArticleIdx >= (u32)(GetCategory(cat)->mNumArticles - 1)) {
        if (s->mLoop) {
            while (++cat < (u32)lbl_803575E0) {
                if (GetCategory(cat)->mArticles != NULL) {
                    return FALSE;
                }
            }
            return TRUE;
        }
        return TRUE;
    }
    return FALSE;
}

void SlideShow::LoadArticle() {
    MenuState* list = lbl_8035755C;
    mUnk328 = 0;
    if (list == NULL) {
        return;
    }

    s32 start = list->mCategory;
    s32 count = lbl_803575E0;
    mCategory = start;
    NewsArticle** articles = GetCategory(start)->mArticles;
    while (articles == NULL) {
        if (++mCategory >= count) {
            mCategory = 0;
        }
        if (mCategory == start) {
            return;
        }
        articles = GetCategory(mCategory)->mArticles;
    }

    NewsTexture* tex = NULL;
    mArticle = articles[mArticleIdx];
    mPrevPicture = NULL;
    mPrevCaption = NULL;
    lbl_8035697C = 1;
    LayoutArticle();
    NewsArticle* article = mArticle;
    if (article->mPicture != NULL) {
        tex = article->mPicture->texture;
    }
    if (tex != NULL || article->mLocation == NULL) {
        mShowPicture = true;
        mPicAlpha = 255;
    }

    SetViewToTarget(this);

    SetArticleText(this);
    Article_ResetHeadline();
    mPicAlpha = mShowPicture ? 255 : 0;
    LayoutTitle();
}

void SlideShow::Start() {
    mMainLayout->Reset();
    mSlideLayout->Reset();
    mBeltLayout->Reset();
    mArticleIdx = 0;
    mTextVisible = true;
    mTextMoving = false;
    mScrollTarget = 0.0f;
    mScroll = 0.0f;
    mMessageFade = 0;
    mHoldTimer = 0;
    mMessageFlag = false;
    mPointerIn[0] = false;
    mPointerIn[1] = false;
    mPointerIn[2] = false;
    mPointerIn[3] = false;
    mLoop = lbl_8035755C->mCategory == 0;
    mBeltFade = 15;
    mBeltVisible = true;
    mBounceTimer = 0;
    mQuickMove = false;
    mZoomDone = false;
    mTimerFade = 0;
    LoadArticle();
    ChangeState(&SlideShow::StateShow);
    if (gGlobe != NULL) {
        gGlobe->SetTiltNow(7);
    }
    Calc();
    SetDPDAll(1);
}

void SlideShow::Stop() {
    ChangeState(&SlideShow::StateStop);
    Article_Reset();
}

void SlideShow::Calc() {
    SetDPDAll(1);
    Globe_ResetFocus();
    mDragging[0] = false;
    mDragging[1] = false;
    mDragging[2] = false;
    mDragging[3] = false;
    mZoomOutPressed = false;
    mPrevPressed = false;
    mNextPressed = false;
    mZoomInPressed = false;
    mBackPressed = false;
    mMainPressed = false;
    mUnk30E = false;
    mUpPressed = false;
    mDownPressed = false;

    if (lbl_80356970 <= 0) {
        mZoomOutButton->mDisabled = true;
        mZoomOutButton->Press();
    } else {
        mZoomOutButton->mDisabled = false;
    }
    if (lbl_80356970 >= 9) {
        mZoomInButton->mDisabled = true;
        mZoomInButton->Press();
    } else {
        mZoomInButton->mDisabled = false;
    }

    if (CheckInput()) {
        return;
    }

    if (IsState(&SlideShow::StateShow) || IsState(&SlideShow::StateMove)) {
        if (gRepeatSlowAll & 0x200) {
            if (mSpeed < 10) {
                mSpeed++;
                mTimer = mSpeed * GetFrameRate();
                PlaySE(0x56);
            }
        }
        if (gRepeatSlowAll & 0x100) {
            if (mSpeed > 1) {
                mSpeed--;
                mTimer = mSpeed * GetFrameRate();
                PlaySE(0x57);
            }
        }
    }

    if (!mZoomed && mShowPicture) {
        mPicAlpha += 20;
        if (mPicAlpha > 255) {
            mPicAlpha = 255;
        }
    } else {
        mPicAlpha -= 20;
        if (mPicAlpha < 0) {
            mPicAlpha = 0;
        }
    }

    mPrevPicAlpha -= 20;
    if (mPrevPicAlpha < 0) {
        mPrevPicAlpha = 0;
    }

    mSlideAngle += 0x400;
    if (mSlideAngle > 0x4000) {
        mSlideAngle = 0x4000;
    }

    UpdateTextSize(mZoomInPressed, mZoomOutPressed);

    if (mState) {
        (this->*mState)(NULL);
    }
    if (mSubState) {
        (this->*mSubState)();
    }

    CalcArrows();

    Globe* globe = gGlobe;
    if (globe != NULL) {
        mGlobeAngle += 0x200;
        f32 range = mGlobeZoomTo - mGlobeZoomFrom;
        if (mGlobeAngle > 0x8000) {
            mGlobeAngle = 0x8000;
        }
        f32 t = range * CosineEase(mGlobeAngle);
        mGlobeZoom = mGlobeZoomFrom + t;
        globe->mZoom = mGlobeZoom;
        globe->UpdateZoom(lbl_80192370);

        Globe* g = gGlobe;
        if (g != NULL) {
            GlobeCamera* camera = g->mCamera;
            if (camera != NULL) {
                f32 s = CosineEase(mGlobeAngle);
                f32 lon = mGlobeFrom.x + (mGlobeTo.x - mGlobeFrom.x) * s;
                camera->mLon = lon;
                camera->mLat = mGlobeFrom.y + (mGlobeTo.y - mGlobeFrom.y) * s;
                g->mLon = lon;
                g->mLat = camera->mLat;
            }
        }

        globe->UpdateTilt(0, lbl_80192398);
        globe->UpdateSpin(0);
        globe->Calc();
        globe->CalcPoles();
        GlobeCamera* camera = globe->mCamera;
        globe->mLon = camera->mLon;
        globe->mLat = camera->mLat;
        globe->ApplyCamera();
        Pins_UpdateFade();
        globe->UpdateLights();
        globe->CalcScene();
    }

    if (mArticle != NULL && mArticle->mLocation == NULL) {
        mAlpha += 32;
        if (mAlpha > 255) {
            mAlpha = 255;
        }
    } else {
        mAlpha -= 32;
        if (mAlpha < 0) {
            mAlpha = 0;
        }
    }

    if (mArticle != NULL && mArticle->mLocation == NULL && GetPictureTexture(mArticle) == NULL) {
        mFooterAlpha += 32;
        if (mFooterAlpha > 255) {
            mFooterAlpha = 255;
        }
    } else {
        mFooterAlpha -= 32;
        if (mFooterAlpha < 0) {
            mFooterAlpha = 0;
        }
    }

    bool active = true;
    bool moving = IsState(&SlideShow::StateShow) || IsState(&SlideShow::StateMove);
    if (!moving) {
        if (!IsState(&SlideShow::StateEnd)) {
            active = false;
        }
    }
    if (active) {
        SetDPDAll(0);
        gPointerOverClock = 0;
    }

    if (mBeltVisible) {
        if (mBeltFade < 15) {
            mBeltFade++;
        }
    } else {
        if (mBeltFade > 0) {
            mBeltFade--;
        }
    }

    if (mTimerFade > 0 && mTimerFade < 15) {
        mTimerFade++;
    }
}

static const GXColor sFooterColor1 = {0, 192, 0, 0};

// Not referenced, so the linker strips it. The original has such a function
// here: the widescreen factor sits in the constant pool between the footer
// colour and the constants of Draw.
f32 SlideShow_GetAspect() {
    return gWidescreen ? 1.3684211f : 1.0f;
}

static inline void DrawFooter(SlideShow* s) {
    if (s->mFooterAlpha != 0 && s->mDrawFooter) {
        Draw2D_SetupGX();
        Draw2D_SetOrtho();
        GXSetTevColor(GX_TEVREG0, (GXColor){sFooterRed, sFooterGreen, sFooterBlue, s->mFooterAlpha});
        GXSetTevColor(GX_TEVREG1, sFooterColor1);
        (s->*s->mDrawFooter)();
    }
}

static inline void DrawTexAt(TPLPalette* tpl, u32 id, f32 x, f32 y, f32 scale) {
    Vec pos;
    pos.x = x;
    pos.y = y;
    pos.z = 0.0f;
    Draw2D_Tex(tpl, id, &pos, scale, scale);
}

#pragma explicit_zero_data on
static f32 sLineFromZ = 0.0f;
static f32 sLineToZ = 0.0f;
#pragma explicit_zero_data reset

void SlideShow::Draw() {
    if (mAlpha != 0) {
        mSlideLayout->SetAlpha(mAlpha);
        mSlideLayout->Draw();
        DrawPointerEffect__FUcUs(mAlpha, mView.top);
        DrawFooter(this);
    }

    static ut::Color sLineColor(0, 0, 0, 255);
    static ut::Color sLightColor(255, 255, 255, 255);
    static ut::Color sBackColor(222, 222, 222, 255);

    math::VEC3 line[2];
    line[0].x = mView.left;
    line[0].y = mView.top;
    line[0].z = sLineFromZ;
    line[1].x = mView.right;
    line[1].y = mView.top;
    line[1].z = sLineToZ;
    Draw2D_SetupGX();
    Draw2D_SetOrtho();
    Draw2D_FillRect(&mView, &sBackColor);
    Draw2D_Line(line[0], line[1], 12, sLineColor, ut::Color(0xFF));
    line[0].y += 1.0f;
    line[1].y += 1.0f;
    Draw2D_Line(line[0], line[1], 12, sLightColor, ut::Color(0xFF));

    DrawPictures();

    if (IsState(&SlideShow::StateZoom) || IsState(&SlideShow::StateMessage)) {
        mMainLayout->Draw();
    }

    if (mBeltFade > 0) {
        f32 t = 1.0f - math::CosFIdx(FIdxRad((1.5707964f * mBeltFade) / 15.0f));
        s32 alpha = 255.0f * t;
        mBeltLayout->SetAlpha(alpha);
        mBeltLayout->Draw();

        if (mHasTitle) {
            Draw2D_SetupGX();
            Draw2D_SetOrtho();
            DrawTabRect__FRCQ34nw4r2ut4RectUcf(mTitleRect, alpha, 0.0f);
            ut::TextWriterBase<wchar_t> writer;
            writer.SetFont(*gSysFont);
            writer.SetDrawFlag(0x122);
            writer.SetupGX();
            writer.SetScale(mTitleScale);
            writer.SetCharSpace(0.0f);
            writer.SetTextColor(ut::Color(0, 0, 0, alpha));
            writer.SetCursor(2.0f + mTitleRight, 1.0f + mTitleY);
            writer.Print(lbl_80357564);
            writer.SetTextColor(ut::Color(255, 255, 255, alpha));
            writer.SetCursor(mTitleRight, mTitleY);
            writer.Print(lbl_80357564);
        }

        s32 rate = GetFrameRate();
        TPLPalette* tpl;
        s32 speed;
        s32 seconds = mTimer / rate;
        speed = mSpeed;
        tpl = gCursorTpl;
        s32 frac = mTimer - seconds * rate;
        f32 x = (f32)GetContentRight() - 8.0f;
        f32 y = 421.0f;
        f32 width = TPL_GetWidth(tpl, 0x60);
        f32 height = TPL_GetHeight(tpl, 0x60);
        f32 fade = (f32)(15 - mTimerFade) / 15.0f;
        Draw2D_SetupGX();
        Draw2D_SetOrtho();
        GXSetTevColor(GX_TEVREG0, ut::Color(0, 0, 0, alpha / 2));

        for (s32 i = 0; i < speed; i++) {
            static ut::Color sDotOn(64, 180, 32, 0);
            static ut::Color sDotOff(48, 128, 16, 0);
            static ut::Color sDotDiff = sDotOn - sDotOff;

            ut::Color color = ut::Color::WHITE;
            f32 scale;
            if (i < seconds) {
                color = 0;
                scale = 0.0f;
            } else if (i == seconds) {
                f32 t2 = (f32)(rate - frac + 1) / rate;
                if (t2 >= 0.75f) {
                    color = sDotDiff;
                    scale = 0.39999998f;
                } else if (t2 >= 0.6f) {
                    color = sDotDiff;
                    f32 u = (0.14999998f - (t2 - 0.6f)) / 0.14999998f;
                    scale = 0.39999998f + 0.5f * u;
                } else {
                    color = 0;
                    scale = 0.0f;
                }
            } else if (i > seconds) {
                color = sDotDiff;
                scale = 0.39999998f;
            }
            color.r = sDotOff.r + (s32)(color.r * fade);
            color.g = sDotOff.g + (s32)(color.g * fade);
            color.b = sDotOff.b + (s32)(color.b * fade);
            scale = 0.3f + scale * fade;
            GXSetTevColor(GX_TEVREG1, color);
            DrawTexAt(tpl, 0x60, x - 0.28125f * (width * scale), y - 0.28125f * (height * scale),
                      scale);
            x -= 18.0f;
        }
    }

    if (IsMessageState()) {
        DrawSelection();
    }
}

void SlideShow::CalcArrows() {
    f32 cx = mView.left + 0.5f * mViewWidth;
    f32 upY = 2.0f + mView.top;
    f32 downY = (mView.top + mViewHeight) - 2.0f;
    f32 right = 20.0f + cx;
    f32 left = cx - 20.0f;
    f32 downBase = downY - 15.0f;
    f32 upBase = 15.0f + upY;
    mUpArrow[0].x = cx;
    mUpArrow[0].y = downY;
    mUpArrow[1].x = right;
    mUpArrow[1].y = downBase;
    mUpArrow[2].x = left;
    mUpArrow[2].y = downBase;
    mDownArrow[0].x = cx;
    mDownArrow[0].y = upY;
    mDownArrow[1].x = left;
    mDownArrow[1].y = upBase;
    mDownArrow[2].x = right;
    mDownArrow[2].y = upBase;

    static ut::Color sArrowColor(50, 50, 50, 255);
    mArrowColors[0] = sArrowColor;
    mArrowColors[1] = sArrowColor;
    mArrowColors[2] = sArrowColor;

    f32 min = Article_GetMaxScrollOffset();
    if (mScroll > 0.0f) {
        mScroll = 0.0f;
        mScrollTarget = 0.0f;
    } else if (mScroll < min) {
        mScroll = min;
        mScrollTarget = min;
    }
    Article_ClampScroll();

    math::VEC2 pos(mText.left, mTextOfs + (mText.top + mScroll));
    if (mZoomed) {
        Article_Update(gTextScale);
        Article_Arrange(GetTextScale(lbl_80356970));
        Article_Layout(&pos, gTextScale - GetTextScale(lbl_80356970));
    } else {
        Article_UpdateHeadline(gTextScale);
        Article_ArrangeHeadline(GetTextScale(lbl_80356970));
        Article_LayoutHeadline(&pos, 1, gTextScale - GetTextScale(lbl_80356970));
    }
    CalcTextPos();
}

void SlideShow::CalcTextPos() {
    if (mTextVisible) {
        f32 lineHeight = lbl_80357568->mLineHeight;
        f32 area = 114.399994f;
        f32 height = Article_GetHeadlineY();
        s32 lines = area / lineHeight;
        f32 ofs;
        if (height < area) {
            ofs = 0.5f * (area - height);
        } else {
            ofs = 0.5f * (area - lineHeight * lines);
        }
        ofs += 10.0f;
        if (!mTextMoving) {
            mTextOfs = ofs;
        }
        if (IsNearlyZero(Ease(&mTextOfs, ofs, 0.2f, 20.0f, 2.0f))) {
            mTextMoving = false;
        }
    } else {
        mTextMoving = true;
        Ease(&mTextOfs, 40.0f, 0.2f, 20.0f, 2.0f);
    }
}

BOOL SlideShow::CheckInput() {
    bool dragging = false;
    if (!(IsState(&SlideShow::StateZoom) || IsState(&SlideShow::StateMessage))) {
        if (gRepeatFastAll & 0x1000) {
            mZoomOutPressed = true;
        }
        if (gRepeatFastAll & 0x10) {
            mZoomInPressed = true;
        }
        return FALSE;
    }

    mMainLayout->FadeIn(15);
    if (mShowMain) {
        mMainLayout->Calc();
    }
    mSlideLayout->Calc();
    mBeltLayout->Calc();

    if (lbl_80357598 != 1) {
        return TRUE;
    }

    if (!IsState(&SlideShow::StateMessage)) {
        UpdateLayoutButtons(mCurLayout, 0x23);
        if (gHold[0] & 0x400) {
            mDragging[0] = true;
            dragging = true;
        }
        if (gHold[1] & 0x400) {
            mDragging[1] = true;
            dragging = true;
        }
        if (gHold[2] & 0x400) {
            mDragging[2] = true;
            dragging = true;
        }
        if (gHold[3] & 0x400) {
            mDragging[3] = true;
            dragging = true;
        }

        CheckPointer();

        if (CheckButtonHold("zoom_out", 0x800) >= 0) {
            mZoomOutPressed = true;
        } else if (gRepeatFastAll & 0x1000) {
            mZoomOutPressed = true;
            mZoomOutButton->SetPressed(true);
        }

        if (CheckButtonHold("zoom_in", 0x800) >= 0) {
            mZoomInPressed = true;
        } else if (gRepeatFastAll & 0x10) {
            mZoomInPressed = true;
            mZoomInButton->SetPressed(true);
        }

        if (CheckButtonHold("prev", 0x800) >= 0 || (gTrigAll & 1)) {
            mPrevPressed = true;
        }
        if (CheckButtonHold("next", 0x800) >= 0 || (gTrigAll & 2)) {
            mNextPressed = true;
        }

        if (!dragging) {
            if (CheckButtonHold("up", 0x800) >= 0) {
                mUpPressed = true;
            } else if (gRepeatFastAll & 8) {
                mUpPressed = true;
                mUpButton->SetPressed(true);
            }
            if (CheckButtonHold("down", 0x800) >= 0) {
                mDownPressed = true;
            } else if (gRepeatFastAll & 4) {
                mDownPressed = true;
                mDownButton->SetPressed(true);
            }
        }

        if (CheckButtonTrig("main", 0x800) >= 0) {
            mMainPressed = true;
        }
        if (CheckButtonTrig("end", 0x800) >= 0) {
            mEndButton->mToggle = true;
            PlaySE(0x22);
            lbl_80357598 = 0;
            return TRUE;
        }
        if (CheckButtonTrig("back", 0x800) >= 0) {
            mBackPressed = true;
        }
    }
    return FALSE;
}

BOOL SlideShow::StateStop(const s32* arg) {
    switch (mStateFrame) {
    case 0:
        mStateFrame++;
        ChangeSubState(&SlideShow::SubStateIdle);
        break;
    case -1:
        break;
    }
    return TRUE;
}

BOOL SlideShow::StateShow(const s32* arg) {
    switch (mStateFrame) {
    case -1:
        Pins_SetStateAll(0);
        mTimerFade = 1;
        break;
    case 0:
        mStateFrame++;
        ChangeSubState(&SlideShow::SubStateIdle);
        Pins_SetState(mCategory, mArticleIdx, 1);
        mTimerFade = 0;
        mTimer = mSpeed * GetFrameRate();
        break;
    default:
        if (gRepeatSlowAll & 1) {
            mPrevPressed = true;
        }
        if (gRepeatSlowAll & 2) {
            mNextPressed = true;
        }
        Pins_SetState(mCategory, mArticleIdx, 1);

        if (mPrevPressed) {
            if (!IsFirstArticle(this)) {
                s32 dir = 0;
                PlaySE(0x39);
                mPlaySound = false;
                ChangeState(&SlideShow::StateMove, &dir);
                return TRUE;
            }
        } else if (mNextPressed) {
            if (IsLastArticle(this)) {
                PlaySE(0x55);
                ChangeState(&SlideShow::StateEnd);
                return TRUE;
            }
            s32 dir = 1;
            PlaySE(0x38);
            mPlaySound = false;
            ChangeState(&SlideShow::StateMove, &dir);
            return TRUE;
        }

        if (gTrigAll & 0x800) {
            PlaySE(0x37);
            ChangeState(&SlideShow::StateZoom);
            return TRUE;
        }

        if (mTimer > 0) {
            mTimer--;
            break;
        }

        if (IsLastArticle(this)) {
            PlaySE(0x55);
            ChangeState(&SlideShow::StateEnd);
            return TRUE;
        }
        s32 dir = 1;
        mPlaySound = true;
        ChangeState(&SlideShow::StateMove, &dir);
        return TRUE;
    }
    return TRUE;
}

void SlideShow::StartZoomOut() {
    f32 volume = 1.0f;
    Bgm_SetSlideshowVolume(volume);
    ChangeSubState(&SlideShow::SubStateIdle);
    NewsTexture* tex = NULL;
    LayoutArticle();
    NewsArticle* article = mArticle;
    if (article->mLocation == NULL) {
        mShowPicture = true;
    } else {
        if (article->mPicture != NULL) {
            tex = article->mPicture->texture;
        }
        if (tex != NULL) {
            mShowPicture = true;
        }
    }
    mTextVisible = true;
    mZoomAngle = 0;
    StartZoom(this);
    mScrollTarget = 0.0f;
}

BOOL SlideShow::StateZoom(const s32* arg) {
    switch (mStateFrame) {
    case -1:
        mZoomed = false;
        mTextVisible = true;
        gHideClock = gUpdateMsgType == 1;
        ChangeSubState(&SlideShow::SubStateIdle);
        mBounceTimer = 0;
        mQuickMove = false;
        break;
    case 0:
        mStateFrame++;
        mUpButton->mDisabled = true;
        mUpButton->Press();
        f32 volume = 0.0f;
        Bgm_SetSlideshowVolume(volume);
        mZoomed = true;
        mTextVisible = false;
        mViewTarget[0] = 0.0f;
        mViewTarget[1] = 0.0f;
        mTextTarget[1] = 83.0f;
        mTextTarget[0] = GetSideMargin();
        mArticle->mFlags |= 1;
        if (mViewTarget[0] != mView.left || mViewTarget[1] != mView.top) {
            mZoomAngle = 0;
        } else {
            mZoomAngle = 0x8000;
        }
        StartZoom(this);
        mBeltVisible = false;
        break;
    default: {
        mZoomAngle += mZoomSpeed;
        if (mZoomAngle > 0x8000) {
            mZoomAngle = 0x8000;
        }
        f32 t = CosineEase(mZoomAngle);
        mView.left = mZoomFrom[0] + mZoomFrom[1] * t;
        mView.top = mZoomFrom[2] + mZoomFrom[3] * t;
        mText.left = mZoomFrom[4] + mZoomFrom[5] * t;
        mText.top = mZoomFrom[6] + mZoomFrom[7] * t;
        ApplyView(this);

        switch (mStateFrame) {
        case 1:
            if (mZoomAngle == 0x8000) {
                mStateFrame++;
                mCurLayout = mMainLayout;
                ChangeSubState(&SlideShow::SubStateScroll);
                SetViewToTarget(this);
                return TRUE;
            }
            break;
        case 2: {
            bool close = false;
            f32 screenWidth = GetScreenWidth();
            ut::Rect rect(0.0f, 0.0f, 0.0f, 0.0f);
            if (Article_GetPictureRect(&rect, mText.left, mTextOfs + (mText.top + mScroll), 1.0f)) {
                bool hold = false;
                for (s32 i = 0; i < 4; i++) {
                    if (IsPointerValid(i)) {
                        f32 x = gCursorX[i][0];
                        f32 y = gCursorY[i][0];
                        bool in;
                        if (gHoverButtons[i] == 0 && x >= rect.left && x < rect.right &&
                            y >= rect.top && y < rect.bottom) {
                            if (gTrig[i] & 0x800) {
                                mMessageFlag = gHideClock;
                                ChangeState(&SlideShow::StateMessage);
                                return TRUE;
                            }
                            in = true;
                            hold = true;
                        } else {
                            in = false;
                        }
                        if (in && !mPointerIn[i]) {
                            StartRumble(i, 3, 20);
                        }
                        mPointerIn[i] = in;
                    }
                }
                if (hold) {
                    if (mHoldTimer < 8) {
                        if (++mHoldTimer == 8) {
                            PlaySE(0x42);
                        }
                    }
                } else {
                    if (mHoldTimer > 0) {
                        mHoldTimer--;
                    }
                }
            }

            for (s32 i = 0; i < 4; i++) {
                if (IsPointerValid(i)) {
                    f32 x = gPointerX[i];
                    f32 y = gPointerY[i];
                    if (x > 0.0f && x < screenWidth && y > 63.0f && y < 393.0f && (gTrig[i] & 0x800)) {
                        close = true;
                        break;
                    }
                } else if (gTrig[i] & 0x800) {
                    close = true;
                    break;
                }
            }

            if (mBackPressed || close) {
                mStateFrame++;
                PlaySE(0x21);
                ChangeSubState(&SlideShow::SubStateWait);
                mBeltVisible = true;
                return TRUE;
            }

            if (!IsLastArticle(this) && (gTrigAll & 2)) {
                mStateFrame = 4;
                mDirection = 1;
                ChangeSubState(&SlideShow::SubStateWait);
                PlaySE(0x38);
                mBeltVisible = true;
                mQuickMove = true;
                return TRUE;
            }

            if (!IsFirstArticle(this) && (gTrigAll & 1)) {
                mStateFrame = 4;
                mDirection = 0;
                PlaySE(0x39);
                ChangeSubState(&SlideShow::SubStateWait);
                mBeltVisible = true;
                mQuickMove = true;
                return TRUE;
            }

            ut::Rect area(0.0f, 0.0f, GetScreenWidth(), 456.0f);
            for (s32 i = 0; i < 4; i++) {
                lbl_801EDFD0[i] = 1;
                switch (UpdateGrab(i, &area)) {
                case 0:
                    if (StartGrab(i, &area)) {
                        if (mGrabStart[i].y < mGrabPos[i].y) {
                            mSelect.left = mGrabStart[i].x;
                            mSelect.top = mGrabStart[i].y;
                            mSelect.right = mGrabPos[i].x;
                            mSelect.bottom = mGrabPos[i].y;
                        } else {
                            mSelect.top = mGrabPos[i].y;
                            mSelect.left = mGrabPos[i].x;
                            mSelect.bottom = mGrabStart[i].y;
                            mSelect.right = mGrabStart[i].x;
                        }
                        if (Article_HitTest(&mSelect)) {
                            PlaySE(0x2B);
                        }
                    }
                    break;
                case 2:
                    Article_ClearHit();
                    Article_Arrange(GetTextScale(lbl_80356970));
                case 1:
                default:
                    if (mGrabbing[i]) {
                        if (mGrabStart[i].y < mGrabPos[i].y) {
                            mSelect.left = mGrabStart[i].x;
                            mSelect.top = mGrabStart[i].y;
                            mSelect.right = mGrabPos[i].x;
                            mSelect.bottom = mGrabPos[i].y;
                        } else {
                            mSelect.top = mGrabPos[i].y;
                            mSelect.left = mGrabPos[i].x;
                            mSelect.bottom = mGrabStart[i].y;
                            mSelect.right = mGrabStart[i].x;
                        }
                        if (Article_HitTest(&mSelect)) {
                            PlaySE(0x2B);
                        }
                        lbl_801EDFD0[i] = 6;
                    }
                    break;
                }
            }
            break;
        }
        case 3:
            if (mZoomDone && mZoomAngle == 0x8000) {
                SetViewToTarget(this);
                SetArticleText(this);
                ChangeState(&SlideShow::StateShow);
                return TRUE;
            }
            break;
        default:
            if (mZoomDone && mZoomAngle == 0x8000) {
                SetViewToTarget(this);
                mPlaySound = false;
                ChangeState(&SlideShow::StateMove, &mDirection);
                return TRUE;
            }
            break;
        }
        break;
    }
    }
    return TRUE;
}

BOOL SlideShow::StateMove(const s32* arg) {
    if (gGlobe == NULL) {
        return TRUE;
    }

    switch (mStateFrame) {
    case -1:
        break;
    case 0: {
        ChangeSubState(&SlideShow::SubStateIdle);
        u16 prevWidth;
        u16 prevHeight;
        NewsLocationRec* location = mArticle->mLocation;
        bool hadLocation = location != NULL;
        if (location != NULL) {
            prevWidth = ((u16*)location)[2];
            prevHeight = ((u16*)location)[3];
        }

        if (arg != NULL) {
            switch (*arg) {
            case 0:
                mSlideDist = 50.0f;
                PrevArticle();
                break;
            default:
                mSlideDist = -50.0f;
                NextArticle();
                break;
            }
        } else {
            mSlideDist = -50.0f;
            NextArticle();
        }

        NewsArticle* prev = mArticle;
        mSlideAngle = 0;
        mStateFrame++;
        if (prev != NULL) {
            mPrevPicture = GetPictureTexture(prev);
            mPrevCaption = GetPictureCaption(mArticle);
            mPrevPicCenter[0] = mPicCenter[0];
            mPrevPicCenter[1] = mPicCenter[1];
            mPrevPicAlpha = mPicAlpha;
            mPrevPicScale = mPicScale;
        } else {
            mPrevPicture = NULL;
            mPrevCaption = NULL;
        }

        mArticle = GetCategory(mCategory)->mArticles[mArticleIdx];
        LayoutArticle();
        SetArticleText(this);
        Article_ResetHeadline();
        LayoutTitle();

        NewsArticle* article = mArticle;
        if (article->mLocation == NULL) {
            mShowPicture = true;
        } else if (GetPictureTexture(article) != NULL) {
            mShowPicture = true;
            mPicAlpha = 0;
        } else {
            mPicAlpha = 0;
        }

        if (mPlaySound && hadLocation) {
            NewsLocationRec* loc = mArticle->mLocation;
            if (loc != NULL && (prevWidth != ((u16*)loc)[2] || prevHeight != ((u16*)loc)[3])) {
                PlaySE(0x24);
            }
        }

        mZoomAngle = 0;
        StartZoom(this);
        break;
    }
    default:
        if (mStateFrame < 3) {
            mZoomAngle += mZoomSpeed;
            if (mZoomAngle > 0x8000) {
                mStateFrame = 3;
                SetViewToTarget(this);
                return TRUE;
            }
            f32 t = CosineEase(mZoomAngle);
            mView.left = mZoomFrom[0] + mZoomFrom[1] * t;
            mView.top = mZoomFrom[2] + mZoomFrom[3] * t;
            mText.left = mZoomFrom[4] + mZoomFrom[5] * t;
            mText.top = mZoomFrom[6] + mZoomFrom[7] * t;
            ApplyView(this);
        }
        if (mStateFrame == 3) {
            if (mArticle->mLocation == NULL) {
                lbl_8035697C = 1;
            }
            ChangeState(&SlideShow::StateShow);
            return TRUE;
        }
        break;
    }
    return TRUE;
}

BOOL SlideShow::StateMessage(const s32* arg) {
    switch (mStateFrame) {
    case -1:
        break;
    case 0:
        PlaySE(0x40);
        gHideClock = 1;
        mMessageFade = 0;
        mZoomed = true;
        mTextVisible = false;
        ChangeSubState(&SlideShow::SubStateScroll);
        mStateFrame = 1;
        break;
    case 1:
        if (mMessageFade < 15) {
            mMessageFade++;
        }
        if (gTrigAll & 0x800) {
            PlaySE(0x41);
            bool flag = true;
            if (gUpdateMsgType != 1) {
                flag = mMessageFlag;
            }
            gHideClock = flag;
            mStateFrame = 2;
        }
        break;
    case 2:
        if (mMessageFade > 0) {
            mMessageFade--;
        }
        if (mMessageFade == 0) {
            ChangeState(&SlideShow::StateZoom);
        }
        break;
    }
    return TRUE;
}

BOOL SlideShow::StateEnd(const s32* arg) {
    switch (mStateFrame) {
    case 0:
        lbl_80357598 = 0;
        mStateFrame = 1;
        break;
    case -1:
        break;
    case 1:
        break;
    }
    return TRUE;
}

void SlideShow::SubStateIdle() {
    switch (mSubStateFrame) {
    case 0:
        mSubStateFrame++;
        mScrollTarget = 0.0f;
        break;
    case -1:
        break;
    default:
        if (IsNearlyZero(Ease(&mScroll, mScrollTarget, 0.2f, 100.0f, 1.0f))) {
            Article_ResetScroll();
        }
        break;
    }
}

void SlideShow::SubStateWait() {
    switch (mSubStateFrame) {
    case -1:
        break;
    case 0:
        mBounceTimer = 0;
        mScrollTarget = 0.0f;
        mZoomDone = false;
        mSubStateFrame = 1;
        break;
    case 1:
        if (mBounceTimer < 8) {
            mBounceTimer++;
            break;
        }
        if (mScroll < -100.0f) {
            mScroll = -100.0f;
        }
        StartZoomOut();
        mZoomDone = true;
        Article_ResetScroll();
        break;
    }
}

void SlideShow::SubStateScroll() {
    switch (mSubStateFrame) {
    case 0:
        mSubStateFrame++;
        break;
    case -1:
        break;
    default:
        if (!Article_IsShort()) {
            for (s32 i = 0; i < 4; i++) {
                if (mDragging[i]) {
                    ChangeSubState(&SlideShow::SubStateDrag);
                    return;
                }
            }
        }

        if (Article_IsAtTop()) {
            PaneButton* button = mMainLayout->FindButton("up");
            button->mDisabled = true;
            button->Press();
        } else {
            mMainLayout->FindButton("up")->mDisabled = false;
        }
        if (Article_IsAtBottom()) {
            PaneButton* button = mMainLayout->FindButton("down");
            button->mDisabled = true;
            button->Press();
        } else {
            mMainLayout->FindButton("down")->mDisabled = false;
        }

        if (mDownPressed && !Article_IsAtBottom()) {
            PlaySE(0x25);
            Article_PageDown(lbl_80356970, mScroll);
        } else if (mUpPressed && !Article_IsAtTop()) {
            PlaySE(0x25);
            Article_PageUp(lbl_80356970, mScroll);
        }

        mScrollTarget = Article_GetScrollOffset();
        if (IsNearlyZero(gTextScale - lbl_801922D0[lbl_80356970])) {
            Ease(&mScroll, mScrollTarget, 0.2f, 20.0f, 1.0f);
        } else {
            mScroll = mScrollTarget;
        }
        break;
    }
}

void SlideShow::SubStateDrag() {
    bool dragging = false;
    switch (mSubStateFrame) {
    case -1:
        lbl_803575BA = 0;
        lbl_801EDFD0[0] = 1;
        lbl_801EDFD0[1] = 1;
        lbl_801EDFD0[2] = 1;
        lbl_801EDFD0[3] = 1;
        lbl_803575BB = 0;
        mScrollTarget = Article_ScrollTo(mScroll, mScrollSpeed);
        break;
    case 0:
        mSubStateFrame++;
        lbl_801EDFA0[1] = gWidescreen ? 19 : 34;
        lbl_801EDFB8[1] = (456 - (gWidescreen ? 19 : 34)) - lbl_803575D0;
        lbl_80357600.a = 100;
        PlaySE(0x16);
        break;
    default: {
        if (Article_IsShort()) {
            ChangeSubState(&SlideShow::SubStateScroll);
            return;
        }
        if (mDragging[0]) {
            dragging = true;
        } else if (mDragging[1]) {
            dragging = true;
        } else if (mDragging[2]) {
            dragging = true;
        } else if (mDragging[3]) {
            dragging = true;
        }
        if (!dragging) {
            ChangeSubState(&SlideShow::SubStateScroll);
            return;
        }

        f32 move = 0.0f;
        s32 count = 0;
        for (s32 i = 0; i < 4; i++) {
            lbl_801EDFD0[i] = 1;
            if (gHold[i] & 0x400) {
                count++;
                lbl_801EDFD0[i] = 5;
                move += 0.1f * gPointerScroll[i];
            }
        }
        if (count != 0) {
            mScrollSpeed = move / count;
        }

        f32 min = Article_GetMaxScrollOffset();
        mScroll += mScrollSpeed;
        if (mScroll > 0.0f) {
            mScroll = 0.0f;
        } else if (mScroll < min) {
            mScroll = min;
        }

        if (mScroll >= 0.0f) {
            lbl_803575BA = 0;
            mUpButton->mDisabled = true;
            mUpButton->Press();
        } else {
            lbl_803575BA = 1;
            mUpButton->mDisabled = false;
        }
        if (mScroll <= min) {
            lbl_803575BB = 0;
            mDownButton->mDisabled = true;
            mDownButton->Press();
        } else {
            lbl_803575BB = 1;
            mDownButton->mDisabled = false;
        }
        break;
    }
    }
}

#pragma explicit_zero_data on
static f32 sPrevPicZ = 0.0f;
static f32 sPicZ = 0.0f;
#pragma explicit_zero_data reset

void SlideShow::DrawPictures() {
    f32 fade;
    if (mBounceTimer > 0) {
        fade = math::CosFIdx(FIdxRad((1.5707964f * mBounceTimer) / 8.0f));
    } else {
        fade = 1.0f;
    }

    Draw2D_SetScissor(mView.left, 0, mViewWidth, 456);
    math::VEC2 pos(mText.left, mTextOfs + (mText.top + mScroll));
    if (mQuickMove) {
        if (mZoomed) {
            Article_Draw(pos, 2, 1, fade,
                        1.0f + 0.05f * math::SinFIdx(FIdxRad(1.5707964f * (mHoldTimer / 8.0f))));
        }
    } else if (mZoomed) {
        Article_Draw(pos, 2, 0, fade,
                    1.0f + 0.05f * math::SinFIdx(FIdxRad(1.5707964f * (mHoldTimer / 8.0f))));
        Article_DrawHeadline(&pos, 0, 1.0f);
    } else {
        Article_DrawHeadline(&pos, 1, 1.0f);
    }
    Draw2D_SetScissor(0, 0, GetScreenWidth(), 456);

    ut::Color shadow(0, 0, 0, 0);
    f32 slide = SinIdx(mSlideAngle);
    Vec pos2;
    ut::Rect rect(0.0f, 0.0f, 0.0f, 0.0f);

    NewsTexture* prev = mPrevPicture;
    if (prev != NULL) {
        s32 alpha = mPrevPicAlpha * fade;
        if (alpha != 0) {
            f32 ofs = mSlideDist * slide;
            f32 width = mPrevPicScale * prev->width;
            f32 height = mPrevPicScale * prev->height;
            f32 hw = 0.5f * width;
            f32 hh = 0.5f * height;
            f32 x = (mPrevPicCenter[0] + ofs) - hw;
            f32 y = mPrevPicCenter[1] - hh;
            f32 border = 0.05f * height;
            pos2.x = x;
            pos2.y = y;
            pos2.z = sPrevPicZ;
            rect.left = 10.0f + x;
            rect.top = 10.0f + y;
            rect.right = border + (rect.left + width);
            rect.bottom = border + (rect.top + height);
            Draw2D_SetupGX();
            Draw2D_SetOrtho();
            shadow.a = 0.6f * alpha;
            GXSetTevColor(GX_TEVREG0, shadow);
            Draw2D_TexRect(gCommonTpl, 0x53, &rect, 0.0f, 0);
            Draw2D_SetupGX();
            GXSetTevColor(GX_TEVREG0, ut::Color(255, 255, 255, alpha));
            Draw2D_Texture(mPrevPicture, (math::VEC3*)&pos2, mPrevPicScale);
            if (mPrevCaption != NULL) {
                DrawCaption(mPrevCaption, alpha, pos2.x, pos2.y, width, height);
            }
        }
    }

    if (GetPictureTexture(mArticle) != NULL) {
        s32 alpha = mPicAlpha * fade;
        if (alpha != 0) {
            f32 ofs = mSlideDist * (1.0f - slide);
            f32 width = mPicScale * GetPictureTexture(mArticle)->width;
            f32 height = mPicScale * GetPictureTexture(mArticle)->height;
            f32 hw = 0.5f * width;
            f32 x = (mPicCenter[0] - ofs) - hw;
            f32 y = mPicCenter[1] - 0.5f * height;
            pos2.x = x;
            pos2.y = y;
            pos2.z = sPicZ;
            f32 border = 0.05f * height;
            rect.left = 10.0f + x;
            rect.top = 10.0f + y;
            rect.right = border + (rect.left + width);
            rect.bottom = border + (rect.top + height);
            Draw2D_SetupGX();
            Draw2D_SetOrtho();
            shadow.a = 0.6f * alpha;
            GXSetTevColor(GX_TEVREG0, shadow);
            Draw2D_TexRect(gCommonTpl, 0x53, &rect, 0.0f, 0);
            Draw2D_SetupGX();
            GXSetTevColor(GX_TEVREG0, ut::Color(255, 255, 255, alpha));
            Draw2D_Texture(GetPictureTexture(mArticle), (math::VEC3*)&pos2, mPicScale);
            if (GetPictureCaption(mArticle) != NULL) {
                DrawCaption(GetPictureCaption(mArticle), alpha, pos2.x, pos2.y, width, height);
            }
        }
    }
}

void SlideShow::DrawSelection() {
    f32 t = math::SinFIdx(FIdxRad(1.5707964f * (mMessageFade / 15.0f)));
    DrawScreenFade(255.0f * t);
    ut::Rect text(0.0f, 0.0f, 0.0f, 0.0f);
    ut::Rect select(0.0f, 0.0f, 0.0f, 0.0f);
    if (Article_GetPictureRect(&text, mText.left, mTextOfs + (mText.top + mScroll),
                    1.0f + 0.05f * math::SinFIdx(FIdxRad(1.5707964f * (mHoldTimer / 8.0f)))) &&
        Article_GetZoomedPictureRect(&select)) {
        Article_DrawZoomedPicture(text, select, t);
    }
}

u8 sFooterRed = 255;
u8 sFooterGreen = 255;
u8 sFooterBlue = 255;

void SlideShow::DrawFooterA() {
    f32 width = TPL_GetWidth(gCommonTpl, 0x41) + TPL_GetWidth(gCommonTpl, 0x42);
    f32 y = mPicCenter[1] - 0.5f * TPL_GetHeight(gCommonTpl, 0x41);
    math::VEC3 pos(GetScreenWidth() / 2 - 0.5f * width, y, 0.0f);
    Draw2D_Tex(gCommonTpl, 0x42, &pos, 1.0f, 1.0f);
    pos.x += TPL_GetWidth(gCommonTpl, 0x42);
    Draw2D_Tex(gCommonTpl, 0x41, &pos, 1.0f, 1.0f);
}

void SlideShow::DrawFooterB() {
    f32 width = TPL_GetWidth(gCommonTpl, 0x41) + TPL_GetWidth(gCommonTpl, 0x42);
    f32 y = mPicCenter[1] - 0.5f * TPL_GetHeight(gCommonTpl, 0x41);
    math::VEC3 pos(GetScreenWidth() / 2 - 0.5f * width, y, 0.0f);
    Draw2D_Tex(gCommonTpl, 0x41, &pos, 1.0f, 1.0f);
    pos.x += TPL_GetWidth(gCommonTpl, 0x41);
    Draw2D_Tex(gCommonTpl, 0x42, &pos, 1.0f, 1.0f);
}

void SlideShow::DrawCaption(const wchar_t* text, s32 alpha, f32 x, f32 y, f32 width, f32 height) {
    ut::TextWriterBase<wchar_t> writer;
    Draw2D_SetupGX();
    Draw2D_SetOrtho();
    writer.SetFont(*gSysFont);
    writer.SetDrawFlag(0x22);
    writer.SetupGX();
    writer.SetScale(0.5f);
    f32 textWidth = writer.CalcStringWidth(text);
    f32 maxWidth = 300.0f * (gWidescreen ? 1.3684211f : 1.0f);
    if (textWidth > maxWidth) {
        writer.SetScale((0.5f * maxWidth) / textWidth, 0.5f);
    }
    writer.SetTextColor(ut::Color(32, 32, 32, alpha));
    writer.SetCursor(1.0f + (x + width), 1.0f + (y + height));
    writer.Print(text);
    writer.SetTextColor(ut::Color(224, 224, 224, alpha));
    writer.SetCursor(x + width, y + height);
    writer.Print(text);
}

void SlideShow::NextArticle() {
    s32 start = mCategory;
    s32 num = GetCategory(start)->mNumArticles;
    s32 numCategories = lbl_803575E0;
    if (++mArticleIdx >= num) {
        if (mLoop) {
            for (;;) {
                if (++mCategory >= numCategories) {
                    mCategory = 0;
                }
                if (start == mCategory) {
                    mArticleIdx = 0;
                    return;
                }
                if (GetCategory(mCategory)->mArticles != NULL) {
                    mArticleIdx = 0;
                    return;
                }
            }
        } else {
            mArticleIdx = 0;
        }
    }
}

void SlideShow::PrevArticle() {
    s32 start = mCategory;
    s32 num = GetCategory(start)->mNumArticles;
    s32 numCategories = lbl_803575E0;
    if (--mArticleIdx < 0) {
        if (mLoop) {
            for (;;) {
                if (--mCategory < 0) {
                    mCategory = numCategories - 1;
                }
                if (start == mCategory) {
                    mArticleIdx = GetCategory(mCategory)->mNumArticles - 1;
                    return;
                }
                Category* cat = GetCategory(mCategory);
                if (cat->mArticles != NULL) {
                    mArticleIdx = cat->mNumArticles - 1;
                    return;
                }
            }
        } else {
            mArticleIdx = num - 1;
        }
    }
}

static inline void SetPicCenter(SlideShow* s, f32 x, f32 y) {
    s->mPicCenter[0] = x;
    s->mPicCenter[1] = y;
}

// Not referenced, so the linker strips it. The original has a function here
// that uses these two constants: they come before those of LayoutArticle in
// the constant pool.
void WrapGlobeAngles(math::VEC2& to, const math::VEC2& from) {
    if (math::FAbs(to.x - from.x) > 180.0f) {
        to.x -= 360.0f;
    }
    if (math::FAbs(to.y - from.y) > 180.0f) {
        to.y -= 360.0f;
    }
}

#pragma explicit_zero_data on
static f32 sGlobeOfsX = 0.0f;
static f32 sGlobeOfsY = 0.0f;
#pragma explicit_zero_data reset

void SlideShow::LayoutArticle() {
    f32 y = 273.6f;
    if (mArticle->mLocation != NULL) {
        mViewTarget[0] = 0.0f;
        mViewTarget[1] = y;
        mTextTarget[0] = GetSideMargin();
        mTextTarget[1] = y;
        if (GetPictureTexture(mArticle) != NULL) {
            mPicArea.left = 0.5f * (u32)GetScreenWidth();
            mPicArea.top = 73.0f;
            mPicArea.right = (u32)GetContentRight();
            mPicArea.bottom = y - 20.0f;
            mGlobeZoomTo = 0.6f;
        } else {
            mGlobeZoomTo = 0.0f;
        }
        if (gGlobe != NULL) {
            mGlobeZoomFrom = gGlobe->mZoom;
        } else {
            mGlobeZoomFrom = mGlobeZoomTo;
        }
        mGlobeAngle = 0;
        GetArticleLocation(&mGlobeTo, mArticle);
        mUnk288.x = sGlobeOfsX;
        mUnk288.y = sGlobeOfsY;
        if (gGlobe != NULL) {
            GlobeCamera* camera = gGlobe->mCamera;
            if (camera != NULL) {
                f32 lat = camera->mLat;
                f32 lon = camera->mLon;
                mGlobeFrom.x = lon;
                mGlobeFrom.y = lat;
                if (math::FAbs(mGlobeTo.x - mGlobeFrom.x) > 180.0f) {
                    mGlobeTo.x -= 360.0f;
                }
                if (math::FAbs(mGlobeTo.y - mGlobeFrom.y) > 180.0f) {
                    mGlobeTo.y -= 360.0f;
                }
            }
        }
        if (lbl_8035697C) {
            mGlobeAngle = 0x8000;
            mGlobeZoom = mGlobeZoomTo;
            mGlobeZoomFrom = mGlobeZoomTo;
            Globe_FocusArticle(mArticle, 7, mGlobeZoomTo, -0.3f);
            lbl_8035697C = 0;
        } else {
            gGlobe->SetZoom(((u8*)mArticle->mLocation)[0xC]);
        }
        mShowPicture = false;
    } else {
        mViewTarget[0] = 0.0f;
        mViewTarget[1] = y;
        mTextTarget[0] = GetSideMargin();
        mTextTarget[1] = y;
        mPicArea.left = GetSideMargin();
        mPicArea.top = 73.0f;
        mPicArea.right = (u32)GetContentRight();
        mPicArea.bottom = y - 20.0f;
    }

    f32 w = mPicArea.GetWidth();
    f32 h = mPicArea.GetHeight();
    SetPicCenter(this, mPicArea.left + 0.5f * w, mPicArea.top + 0.5f * h);
    if (GetPictureTexture(mArticle) != NULL) {
        f32 texWidth = GetPictureTexture(mArticle)->width;
        f32 texHeight = GetPictureTexture(mArticle)->height;
        f32 aspect = h / w;
        f32 texAspect = texHeight / texWidth;
        mPicScale = texAspect > aspect ? h / texHeight : w / texWidth;
    } else {
        mPicScale = 1.0f;
    }
}

static inline void UpdateLayoutAlpha(SlideShow* s) {
    s32 alpha = 32;
    f32 t = math::SinFIdx(FIdxRad((1.5708f * (15 - s->mFooterFade)) / 15.0f));
    alpha += (s32)(255.0f - alpha) * t;
    s->mCurLayout->SetBlend(alpha, s->mFooterFade, 15);
}

void SlideShow::CheckPointer() {
    f32 minDist;
    f32 x;
    f32 y;
    f32 left = -16.0f;
    f32 right = 16.0f + GetScreenWidth();
    f32 minY = 63.0f;
    f32 maxY = 393.0f;
    minDist = 900.0f;
    bool moved = false;
    bool inside = false;
    bool used = false;
    bool dragging = false;
    bool outside = false;
    for (s32 i = 0; i < 4; i++) {
        if (IsPointerValid(i)) {
            x = gCursorX[i][0];
            y = gCursorY[i][0];
            f32 px, py;
            if (gPointerHistory.GetOldest(i, &px, &py)) {
                if (x >= left && x < right && px >= left && px < right &&
                    (x - px) * (x - px) + (y - py) * (y - py) > minDist) {
                    moved = true;
                } else if (y <= minY || y > maxY) {
                    moved = true;
                }
            }
            if (x >= left && x < right) {
                inside = true;
            }
            if (gHoverButtons[i] != 0) {
                used = true;
            }
            if (y < minY || y > maxY) {
                outside = true;
            }
        }
    }

    bool drag = false;
    if (mDragging[0]) {
        drag = true;
    } else if (mDragging[1]) {
        drag = true;
    } else if (mDragging[2]) {
        drag = true;
    } else if (mDragging[3]) {
        drag = true;
    }
    if (drag) {
        moved = false;
        inside = false;
        dragging = true;
        used = false;
    }

    if (inside) {
        mPointerOutTimer = 0;
    } else if (mPointerOutTimer < 10) {
        mPointerOutTimer++;
    }

    if (moved || used) {
        mIdleTimer = 0;
    } else if (mIdleTimer < 90) {
        mIdleTimer++;
    }

    if (dragging) {
        mCurLayout->SlideOut(10);
        outside = true;
    } else if (mPointerOutTimer < 10 && mIdleTimer < 90) {
        mCurLayout->SlideIn(15);
    } else {
        mCurLayout->SlideOut(30);
        outside = true;
    }

    if (used) {
        if (mFooterFade > 0) {
            mFooterFade--;
        }
    } else if (mFooterFade < 15) {
        mFooterFade++;
    }

    bool flag = true;
    if (gUpdateMsgType != 1) {
        flag = outside;
    }
    gHideClock = flag;

    UpdateLayoutAlpha(this);
}

BOOL SlideShow::StartGrab(s32 chan, const ut::Rect* rect) {
    if (!IsPointerValid(chan)) {
        return FALSE;
    }
    f32 x = gCursorX[chan][0];
    f32 y = gCursorY[chan][0];
    if (x > rect->left && x < rect->right && y > rect->top && y < rect->bottom) {
        if (!(gHold[chan] & 0x400) && (gTrig[chan] & 0x200)) {
            mGrabbing[chan] = true;
            for (s32 i = 0; i < 4; i++) {
                if (i != chan) {
                    mGrabbing[i] = false;
                }
            }
            mGrabPos[chan].x = mGrabStart[chan].x = gPointerX[chan];
            mGrabPos[chan].y = mGrabStart[chan].y = gPointerY[chan];
            return TRUE;
        }
    }
    return FALSE;
}

s32 SlideShow::UpdateGrab(s32 chan, const ut::Rect* rect) {
    f32 x = gPointerX[chan];
    f32 y = gPointerY[chan];
    if (mGrabbing[chan]) {
        if (!(gHoldAll & 0xFDFF) && (gHold[chan] & 0x200) && y > rect->top &&
            y < rect->bottom) {
            mGrabPos[chan].x = x;
            if (x < rect->left) {
                mGrabPos[chan].x = rect->left;
            } else if (mGrabPos[chan].x > rect->right) {
                mGrabPos[chan].x = rect->right;
            }
            mGrabPos[chan].y = y;
            if (y < rect->top) {
                mGrabPos[chan].y = rect->top;
            } else if (mGrabPos[chan].y > rect->bottom) {
                mGrabPos[chan].y = rect->bottom;
            }
            return 1;
        }
        mGrabbing[chan] = false;
        return 2;
    }
    return 0;
}

void SlideShow::LayoutTitle() {
    if (lbl_80357564 != NULL) {
        ut::TextWriterBase<wchar_t> writer;
        mHasTitle = true;
        mTitleScale = 0.75f;
        writer.SetFont(*gSysFont);
        writer.SetDrawFlag(0x122);
        writer.SetupGX();
        writer.SetScale(mTitleScale);
        writer.SetCharSpace(0.0f);
        f32 width = writer.CalcStringWidth(lbl_80357564);
        if (width > mTitleMaxWidth) {
            mTitleScale *= mTitleMaxWidth / width;
            width = mTitleMaxWidth;
        }
        mTitleRect.left = mTitleRight - width;
    }
}

f32 sPictureScale = 1.0f;
