#define NW4R_UT_COLOR_WORD_COPY
#include <news/MainScreen.h>
#include <news/Common.h>
#include <news/Draw2D.h>
#include <news/HeadlineList.h>
#include <news/MathUtil.h>
#include <news/NewsArticle.h>
#include <news/PaneButton.h>
#include <news/System.h>
#include <news/TextButton.h>
#include <news/Ticker.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_Font.h>
#include <nw4r/lyt/lyt_textBox.h>
#include <string.h>

using namespace nw4r;

// Not yet decompiled: layouts, input and globals in other files.
extern u8 lbl_801EE270[];          // layout resource accessor
extern f32 lbl_803575DC;
extern u8 lbl_803575BF;
extern s32 lbl_80357598;
extern s32 lbl_80356970;           // text zoom level (0-9)
extern u8 lbl_8035697C;
extern void* lbl_80357730;
extern s32 lbl_80357760;
extern const wchar_t* lbl_801B0D08[];
extern const wchar_t* lbl_801B0E30[];
// Rows visible / rows scrolled per step for each text zoom level.
static const s32 sVisibleRows[10] = {3, 3, 3, 3, 3, 3, 2, 2, 1, 1};
static const s32 sScrollRows[10] = {3, 2, 2, 2, 2, 1, 1, 1, 1, 1};
// Sound effect per article region.
static const u32 sRegionSE[10] = {75, 76, 77, 78, 79, 80, 81, 82, 83, 84};
extern u8 lbl_803575BC;
extern bool lbl_80356CA0;
extern u8 lbl_80357729;
extern u8 lbl_803575BD;
extern s32 lbl_803575A8;
extern NewsArticle* lbl_80357560;  // article being read
extern f32 lbl_803575C8;
extern s32 lbl_801EDFD0[4];        // pointer cursor shape per channel
extern s32 lbl_80357580;
extern s32 lbl_8020E4A0[4];
extern u8 lbl_803575B9;
extern u8 lbl_803575BA;
extern f32 lbl_801EDFA0[6];
extern f32 lbl_801EDFB8[6];
extern f32 lbl_803575D4;
extern f32 lbl_803575D0;
extern const f32 lbl_801922D0[];   // text scale per zoom level
extern const s32 lbl_80192370[];
extern u8 lbl_8020DE24[];          // pointer history
extern f32 lbl_80356C98;           // layout alpha
const char* GetLanguageSuffix();
extern const s32 lbl_80192398[];
extern f32 lbl_803575D8;
extern GXColor lbl_80357600;
extern f32 lbl_8020E468[];         // pointer movement
extern u8 lbl_803575BB;
extern ut::Color lbl_803575FC;

// Layout (0x80047B50, not yet decompiled) scroll position.
struct LayoutScroll {
    u8 unk0[0x41C];
    s32 mMax;   // at 0x41C
    s32 mPos;   // at 0x420
};

void DrawScreenFade(s32 alpha);

extern "C" {
Layout* fn_80047B50(void* mem, u32 arc, const char* name, void* resAccessor, u32 arg);
void fn_80047DE8(Layout* layout, s32 flags);
void fn_80047EFC(Layout* layout);
void fn_80047F70(Layout* layout);
void fn_80048154(Layout* layout);
PaneButton* fn_80048364(Layout* layout, const char* name);
void fn_80048444(Layout* layout, s32 arg);
void fn_8004BD60(Layout* layout, u32 arg);
s32 fn_8004C000(const char* name, u32 button);
s32 fn_8004C13C(const char* name, u32 button);
void fn_800323D8(f32* scale, s32 arg);
void fn_80032414(math::VEC2* pos, const bool* held, f32 scale, f32 delta);
void fn_80032464(f32 x);
f32 fn_80035188(s32 zoom);
void fn_8001F730(nw4r::lyt::Pane* pane, const nw4r::ut::Color& color);
void fn_8000E180(RelatedItem* item, ut::TextWriterBase<wchar_t>* writer);
void fn_80033FFC(math::VEC2* pos, s32 arg1, s32 arg2, f32 arg3, f32 scale);
BOOL fn_8003567C(ut::Rect* rect, f32 x, f32 y, f32 scale);
BOOL fn_80035764(ut::Rect* rect);
void fn_80035A3C(ut::Rect* r0, ut::Rect* r1, f32 alpha);
void fn_80032644(void);
BOOL fn_8004DABC(Globe* globe);
void fn_8004D0B0(Globe* globe);
void fn_8004D170(Globe* globe);
void fn_800329CC(void);
BOOL fn_8003251C(void);
void fn_80048D20(void* obj, s32 frames);
void fn_800491EC(void* obj, ut::Color color, s32 arg);
void fn_80048C80(void* obj, s32 frames);
void fn_80032658(u8 region);
void fn_80032580(NewsArticle* article, s32 arg, f32 x, f32 y);
void fn_80030720(s32 arg);
void fn_80032554(s32* arg);
f32 fn_80034844(void);
void fn_80034CDC(void);
void fn_800332B4(math::VEC2* pos, f32 delta);
void fn_800323BC(f32* scale);
BOOL fn_80034158(void);
void fn_8003481C(f32 width);
void fn_8003356C(math::VEC2* pos, math::VEC2* origin, math::VEC2* thumbPos, f32* thumbScale);
void fn_80032A94(s32 arg);
bool fn_8003519C(bool zoomIn, bool zoomOut);
s32* fn_80032538(void);
void fn_80030544(void);
math::VEC2 fn_8000D6A0(RelatedItem* item);
void fn_80032B04(s32 section, s32 index, s32 arg);
void fn_800333C4(f32 scale);
void fn_80033374(f32 scale);
void fn_80032AC0(s32 section, s32 index, s32 arg);
void fn_80033538(void);
f32 fn_80034164(void);
void fn_80030650(void);
BOOL fn_80034F6C(ut::Rect* rect);
void fn_80034FD4(void);
void fn_8004D2E8(Globe* globe);
BOOL fn_80032BE0(s32 chan);
void fn_8004DA8C(Globe* globe, s32 arg1, s32 arg2);
BOOL fn_80034598(void);
BOOL fn_80034770(void);
BOOL fn_80034780(void);
void fn_80034690(s32 zoom, f32* scroll);
void fn_800345E4(s32 zoom, f32* scroll);
f32 fn_80034D34(f32 scroll, f32 velocity);
BOOL fn_800324A0(void);
BOOL fn_80032478(void);
void fn_80032450(void);
void fn_8003243C(void);
void fn_80032508(void);
void fn_800324F4(f32 velocity);
void fn_8004D1D4(Globe* globe, const s32* table);
void fn_8004DB80(Globe* globe, s32 arg, const s32* table);
void fn_8004DD8C(Globe* globe, s32 arg);
void fn_8004CBE0(Globe* globe);
void fn_8004CE00(Globe* globe);
void fn_8004CC20(Globe* globe);
s32 fn_8004D628(Globe* globe, s32 chan);
BOOL fn_8004D300(Globe* globe, s32 chan);
void fn_8004E0E8(Globe* globe, s32 arg);
void fn_8004F8E0(u32 id, f32 pitch, f32 volume, f32 pan);
RelatedItem* fn_80032B60(s32* section, s32* index);
BOOL fn_80048854(void* history, s32 chan, f32* x, f32* y);
void fn_80048418(Layout* layout, s32 frames);
void fn_800483EC(Layout* layout, s32 frames);
void fn_80048470(Layout* layout, s32 alpha, s32 frame, s32 frames);
f32 fn_8000F73C(RelatedItem* item, const wchar_t* text, ut::Font* font, f32 scale, f32 space);
void fn_8000F8A8(RelatedItem* item, f32 rowHeight);
void fn_8000EFFC(RelatedItem* item, ut::TextWriterBase<wchar_t>* writer);
void fn_8000F3F0(RelatedItem* item, ut::TextWriterBase<wchar_t>* writer);
void fn_8003256C(math::VEC2* pos, NewsArticle* article);
void fn_8003300C(NewsArticle* article, const wchar_t* name, NewsTexture* texture, math::VEC2* origin,
                 math::VEC2* thumbPos, const f32& thumbScale, math::VEC2* size, s32 arg7,
                 BOOL hasLocation, f32 scale);
}

static math::VEC2 sShadowOffset(2.0f, 2.0f);
static ut::Color sGrayColor(180, 180, 180, 255);
static ut::Color sShadowColor(0, 0, 0, 100);

const char* sGenreNames[] = {
    "genre_a", "genre_b", "genre_c", "genre_d", "genre_e", "genre_f", "genre_g",
    "genre_h", "genre_i", "genre_j", "genre_k", "genre_l", "genre_m", "genre_n",
};

#pragma explicit_zero_data on
f32 sDragStartX = 0.0f;
f32 sDragStartY = 0.0f;
f32 sDragPosX = 0.0f;
f32 sDragPosY = 0.0f;
#pragma explicit_zero_data off

static inline f32 Lerp(f32 a, f32 b, f32 t) {
    return a + (b - a) * t;
}

static inline f32 Lerp2(const f32& a, const f32& b, f32 t) {
    return a + (b - a) * t;
}

static inline u32 GetCursorAreaRight() {
    u32 cursorW = TPL_GetWidth(gCursorTpl, 6);
    return GetContentRight() - cursorW - 10;
}

static inline Ticker* GetListItem(HeadlineList* list, s32 index) {
    return list->mNumItems != 0 ? &list->mItems[index] : NULL;
}

static inline void EnableButton(PaneButton* button) {
    button->mDisabled = false;
}

static inline void DisableButton(PaneButton* button) {
    button->mDisabled = true;
    button->Press();
}

MainScreen::MainScreen(u32 arc, ut::TextWriterBase<wchar_t>* writer, math::VEC2& pos,
                       math::VEC2& size)
    : ScreenBase(writer, pos.x, pos.y, size.x, size.y),
      mMainLayout(NULL),
      mHeadLayout(NULL),
      mEarthLayout(NULL),
      mActiveLayout(NULL),
      mUpButton(NULL),
      mDownButton(NULL),
      mZoomInButton(NULL),
      mZoomOutButton(NULL),
      mHeadBackButton(NULL),
      mHeadUpButton(NULL),
      mHeadDownButton(NULL),
      mHeadZoomInButton(NULL),
      mHeadZoomOutButton(NULL),
      mSlideButton(NULL),
      mTextButton(NULL),
      mRotAButton(NULL),
      mRotBButton(NULL),
      mEarthZoomOutButton(NULL),
      mEarthZoomInButton(NULL),
      mResetButton(NULL),
      mEarthBackButton(NULL),
      mState(NULL),
      mPrevState(NULL),
      mUnk104(NULL),
      mDraw(NULL),
      mUnk11C(NULL),
      mUnk128(NULL),
      mUnk134(NULL),
      mUnk14C(0.0f, 0.0f),
      mUnk154(-GetScreenWidth(), 0.0f),
      mUnk15C(pos.x + GetScreenWidth(), pos.y),
      mUnk164(0.0f, 0.0f),
      mUnk16C(sShadowOffset.x + (pos.x + GetScreenWidth()), pos.y + sShadowOffset.y),
      mListSize(size.x, size.y - 2.0f * sShadowOffset.y),
      mUnk17C(0.0f),
      mUnk180(0.0f),
      mUnk184(0.0f),
      mUnk188(0.0f),
      mUnk18C(0.0f, 0.0f),
      mUnk194(0.0f, 0.0f),
      mUnk19C(0.0f),
      mUnk1A0(0.0f),
      mUnk1A4(0.0f),
      mUnk1A8(0.0f),
      mUnk1AC(0.0f),
      mUnk1B0(0.0f),
      mUnk1F4(0.0f, 0.0f, 0.0f, 0.0f),
      mFadeRect(0.0f, 0.0f, 0.0f, 0.0f),
      mScreenRect(0.0f, 0.0f, GetScreenWidth(), 456.0f),
      mUnk224(0.0f),
      mUnk228(0.0f),
      mUnk22C(0.0f),
      mUnk230(0.0f),
      mUnk234(0.0f),
      mUnk238(GetScreenWidth()),
      mUnk23C(1.0f),
      mUnk240(1.0f),
      mUnk244(0.0f),
      mUnk248(1.0f),
      mUnk24C(48.0f),
      mUnk250(1.0f),
      mUnk25C(0.0f),
      mUnk260(0.0f),
      mUnk264(0.0f),
      mUnk268(0.0f),
      mUnk26C((456 - (gWidescreen ? 19 : 34)) - 393.0f),
      mUnk270(gWidescreen ? -0.7125f : -0.625f),
      mUnk274(gWidescreen ? -0.7125f : -0.625f),
      mUnk278(gWidescreen ? -0.7125f : -0.625f),
      mUnk27C(0.0f),
      mUnk280(0.0f),
      mUnk284(0.0f),
      mUnk288(0.0f),
      mUnk28C(0.0f),
      mZoomOutPressed(false),
      mUpPressed(false),
      mZoomInPressed(false),
      mBackPressed(false),
      mDownPressed(false),
      mSlidePressed(false),
      mEarthPressed(false),
      mRotAPressed(false),
      mRotBPressed(false),
      mResetPressed(false),
      mUnk2B6(false),
      mUnk2B7(false),
      mUnk2B8(false),
      mShowRelated(false),
      mUnk2BE(false),
      mUnk2BF(false),
      mUnk2C0(false),
      mUnk2C4(0),
      mUnk2C8(10),
      mModeStep(0),
      mStateStep(0),
      mUnk2D4(0),
      mUnk2D8(0),
      mUnk2E4(-1),
      mUnk2E8(-1),
      mUnk2EC(0),
      mUnk2F0(0),
      mUnk2F4(0),
      mUnk2FC(0),
      mUnk300(0),
      mUnk324(255),
      mUnk328(0),
      mUnk32C(0x8000),
      mUnk330(0),
      mUnk334(0),
      mUnk338(0),
      mUnk33C(0),
      mUnk340(0),
      mUnk344(0),
      mSoundId(-1),
      mUnk34C(0),
      mUnk350(0),
      mUnk354(false),
      mUnk355(false) {
    if (IsErrorState()) {
        return;
    }

    mHeld[0] = false;
    mHeld[1] = false;
    mHeld[2] = false;
    mHeld[3] = false;

    Layout* layout = (Layout*)operator new(0x434);
    if (layout != NULL) {
        layout = fn_80047B50(layout, arc, "main.brlyt", lbl_801EE270, 0);
    }
    mMainLayout = layout;
    if (mMainLayout == NULL) {
        gAllocFailed = true;
        return;
    }
    layout = (Layout*)operator new(0x434);
    if (layout != NULL) {
        layout = fn_80047B50(layout, arc, "head.brlyt", lbl_801EE270, 0);
    }
    mHeadLayout = layout;
    if (mHeadLayout == NULL) {
        gAllocFailed = true;
        return;
    }
    layout = (Layout*)operator new(0x434);
    if (layout != NULL) {
        layout = fn_80047B50(layout, arc, "earth.brlyt", lbl_801EE270, 0);
    }
    mEarthLayout = layout;
    if (mEarthLayout == NULL) {
        gAllocFailed = true;
        return;
    }

    mUpButton = fn_80048364(mMainLayout, "up");
    if (mUpButton == NULL) {
        gFatalError = true;
        return;
    }
    mDownButton = fn_80048364(mMainLayout, "down");
    if (mDownButton == NULL) {
        gFatalError = true;
        return;
    }
    mZoomInButton = fn_80048364(mMainLayout, "zoom_in");
    if (mZoomInButton == NULL) {
        gFatalError = true;
        return;
    }
    mZoomOutButton = fn_80048364(mMainLayout, "zoom_out");
    if (mZoomOutButton == NULL) {
        gFatalError = true;
        return;
    }
    mHeadBackButton = fn_80048364(mHeadLayout, "back");
    if (mHeadBackButton == NULL) {
        gFatalError = true;
        return;
    }
    mHeadUpButton = fn_80048364(mHeadLayout, "up");
    if (mHeadUpButton == NULL) {
        gFatalError = true;
        return;
    }
    mHeadDownButton = fn_80048364(mHeadLayout, "down");
    if (mHeadDownButton == NULL) {
        gFatalError = true;
        return;
    }
    mTextButton = fn_80048364(mHeadLayout, "text");
    if (mTextButton == NULL) {
        gFatalError = true;
        return;
    }
    mHeadZoomOutButton = fn_80048364(mHeadLayout, "zoom_out");
    if (mHeadZoomOutButton == NULL) {
        gFatalError = true;
        return;
    }
    mHeadZoomInButton = fn_80048364(mHeadLayout, "zoom_in");
    if (mHeadZoomInButton == NULL) {
        gFatalError = true;
        return;
    }
    mSlideButton = fn_80048364(mHeadLayout, "slide");
    if (mSlideButton == NULL) {
        gFatalError = true;
        return;
    }

    mUpButton->mUnk91 = true;
    mDownButton->mUnk91 = true;
    PaneButton* button = fn_80048364(mMainLayout, "zoom_out");
    if (button != NULL) {
        button->mUnk91 = true;
        button->mTextColorCallback = fn_8001F730;
    }
    fn_80048364(mMainLayout, "zoom_in")->mUnk91 = true;
    fn_80048364(mMainLayout, "back")->mUnk91 = true;
    fn_80048364(mMainLayout, "earth")->mUnk91 = true;
    mHeadBackButton->mUnk91 = true;
    DisableButton(mHeadBackButton);
    mHeadUpButton->mUnk91 = true;
    mHeadDownButton->mUnk91 = true;
    mHeadZoomOutButton->mUnk91 = true;
    mHeadZoomOutButton->mTextColorCallback = fn_8001F730;
    mHeadZoomInButton->mUnk91 = true;
    mSlideButton->mUnk91 = true;
    DisableButton(mSlideButton);
    mTextButton->Hide();

    mRotAButton = fn_80048364(mEarthLayout, "rot_a");
    if (mRotAButton == NULL) {
        gFatalError = true;
        return;
    }
    mRotAButton->mUnk91 = true;
    mRotBButton = fn_80048364(mEarthLayout, "rot_b");
    if (mRotBButton == NULL) {
        gFatalError = true;
        return;
    }
    mRotBButton->mUnk91 = true;
    mEarthZoomOutButton = fn_80048364(mEarthLayout, "zoom_out");
    if (mEarthZoomOutButton == NULL) {
        gFatalError = true;
        return;
    }
    mEarthZoomOutButton->mUnk91 = true;
    mEarthZoomOutButton->mTextColorCallback = fn_8001F730;
    mEarthZoomInButton = fn_80048364(mEarthLayout, "zoom_in");
    if (mEarthZoomInButton == NULL) {
        gFatalError = true;
        return;
    }
    mEarthZoomInButton->mUnk91 = true;
    mEarthBackButton = fn_80048364(mEarthLayout, "back");
    if (mEarthBackButton == NULL) {
        gFatalError = true;
        return;
    }
    mEarthBackButton->mUnk91 = true;
    mResetButton = fn_80048364(mEarthLayout, "reset");
    if (mResetButton == NULL) {
        gFatalError = true;
        return;
    }
    mResetButton->mUnk91 = true;

    mUnk230 = mUnk154.x;
    for (s32 i = 0; i < 4; i++) {
        mDragging[i] = false;
        mDragStart[i].x = sDragStartX;
        mDragStart[i].y = sDragStartY;
        mDragPos[i].x = sDragPosX;
        mDragPos[i].y = sDragPosY;
    }

    math::VEC2 listPos(GetSideMargin(), 73.0f);
    for (s32 i = 0; i < MAX_CATEGORIES; i++) {
        mLists[i] = NULL;
    }
    for (u32 i = 0; i < lbl_803575E0; i++) {
        mLists[i] = new HeadlineList(&gNewsData->mCategories[i], writer, listPos, mListSize, i);
        if (mLists[i] == NULL) {
            gAllocFailed = true;
            return;
        }
        if (IsErrorState()) {
            return;
        }
    }
    lbl_8035755C = mLists[0];

    u32 i;
    s32 align;
    f32 width;
    if (gLanguage == 0) {
        align = TextButton::ALIGN_CENTER;
        width = 180.0f;
    } else {
        align = TextButton::ALIGN_LEFT;
        width = 448.0f;
    }
    for (i = 0; i < lbl_803575E0; i++) {
        mButtons[i] = new FrameTextButton(gNewsData->mCategories[i].mName,
                                          math::VEC2(width, 56.0f), i, true, align, 0.9f);
        if (mButtons[i] == NULL) {
            gAllocFailed = true;
            return;
        }
        if (IsErrorState()) {
            return;
        }
    }

    f32 cursorX = GetCursorAreaRight();
    lbl_803575DC = GetContentRight() - cursorX;
    Start();
}

MainScreen::~MainScreen() {
    for (u32 i = 0; i < lbl_803575E0; i++) {
        if (mButtons[i] != NULL) {
            delete mButtons[i];
        }
    }
    for (u32 i = 0; i < lbl_803575E0; i++) {
        if (mLists[i] != NULL) {
            delete mLists[i];
        }
    }
    if (mEarthLayout != NULL) {
        fn_80047DE8(mEarthLayout, 1);
    }
    if (mHeadLayout != NULL) {
        fn_80047DE8(mHeadLayout, 1);
    }
    if (mMainLayout != NULL) {
        fn_80047DE8(mMainLayout, 1);
    }
}

void MainScreen::Start() {
    fn_80047EFC(mMainLayout);
    fn_80047EFC(mHeadLayout);
    fn_80047EFC(mEarthLayout);
    for (u32 i = 0; i < lbl_803575E0; i++) {
        mLists[i]->SetScale(gTextScale);
    }
    mUnk23C = 1.0f;
    mUnk154.x = mUnk230;
    mScreenRect.right = mUnk238;
    fn_80032464(mUnk14C.x + GetSideMargin());
    mUnk34C = 0;
    mUnk350 = 0;
    mUnk354 = false;
    mUnk355 = false;
    mUnk356[0] = false;
    mUnk356[1] = false;
    mUnk356[2] = false;
    mUnk356[3] = false;
    ChangeState(&MainScreen::StateList, NULL);
    LayoutSectionButtons();
}

void MainScreen::LayoutSectionButtons() {
    FrameTextButton** button = &mButtons[1];
    s32 count = lbl_803575E0 - 1;
    f32 top = 30.0f + lbl_8035755C->GetTopButtonBottom();
    math::VEC2 pos;
    if (gLanguage == 0) {
        f32 left = mUnk164.x + mUnk14C.x + (GetSideMargin() + 90);
        if (gWidescreen) {
            left += 112.0f;
        }
        for (s32 i = 0; i < count; i++, button++) {
            pos.x = left + (i % 3) * 185;
            pos.y = top + (i / 3) * 60;
            (*button)->Update(pos);
        }
    } else {
        pos.x = mUnk164.x + mUnk14C.x + GetScreenWidth() / 2;
        for (s32 i = 0; i < count; i++, button++) {
            pos.y = top + i * 60;
            (*button)->Update(pos);
        }
    }
}
#pragma explicit_zero_data on
f32 sCursorY = 0.0f;
f32 sCursorZ = 0.0f;
f32 sLineZ = 0.0f;
f32 sPopupY = 83.0f;
f32 sGlobeX = 0.0f;
f32 sGlobeY = 0.0f;
f32 sGlobe2X = 0.0f;
f32 sGlobe2Y = 0.0f;
#pragma explicit_zero_data off

void MainScreen::Draw() {
    Draw2D_SetupGX();
    Draw2D_SetOrtho();
    f32 scale = gWidescreen ? 832.0f / 608.0f : 1.0f;
    f32 alpha = mUnk248;
    GXColor black;
    black.r = 0;
    black.g = 0;
    black.b = 0;
    GXColor white;
    white.r = 255;
    u8 a = 255.0f * alpha;
    black.a = a;
    u8 bgAlpha = mUnk324 * alpha;
    white.g = 255;
    white.b = 255;
    white.a = a;
    math::VEC3 pos;
    pos.x = mScreenRect.right - scale * TPL_GetWidth(gCommonTpl, 0);
    pos.y = sCursorY;
    pos.z = sCursorZ;
    if (IsState(&MainScreen::State195A0) || IsState(&MainScreen::State17E6C)) {
        ut::Color bg(0xDE, 0xDE, 0xDE, bgAlpha);
        Draw2D_FillRect(&mScreenRect, &bg);
    } else {
        GXSetTevColor(GX_TEVREG0, ut::Color(255, 255, 255, bgAlpha));
        Draw2D_Tex(gCommonTpl, 0, &pos, scale, 1.0f);
    }
    pos.x = mScreenRect.right - 2.0f;
    math::VEC3 end(pos.x, 456.0f, sLineZ);
    Draw2D_SetupGX();
    Draw2D_Line(pos, end, 12, white, white);
    pos.x += 1.0f;
    end.x += 1.0f;
    Draw2D_Line(pos, end, 12, black, black);
    if (mDraw) {
        (this->*mDraw)();
    }
    if (mUnk2BF) {
        Draw2D_SetupGX();
        Draw2D_SetOrtho();
        GXSetTevColor(GX_TEVREG0, ut::Color(255, 255, 255, mUnk328));
        Draw2D_TexRect(gCommonTpl, 0, &mFadeRect, 0.0f, 0);
    }
    if (mActiveLayout) {
        bool inList = IsState(&MainScreen::State16960) || IsState(&MainScreen::StateList);
        if (inList && lbl_8035755C != NULL && lbl_8035755C->mNumItems == 0) {
            mTextButton->SetAlpha((s32)(255.0f * mUnk240));
        }
        fn_80048154(mActiveLayout);
    }
    if (IsState(&MainScreen::State1C600)) {
        DrawCursor();
    }
}

inline void MainScreen::DrawGlobeInline() {
    math::VEC2 pos(mUnk16C.x, 123.0f + mUnk224);
    Draw2D_SetScissor(0, 0, mScreenRect.right, 456);
    f32 scale = 1.0f + 0.05f * math::SinRad(1.5707964f * (mUnk350 / 8.0f));
    f32 alpha = mUnk244;
    fn_80033FFC(&pos, mUnk334, 1, alpha, scale);
    Draw2D_SetScissor(0, 0, GetScreenWidth(), 456);
    DrawRelated();
}

void MainScreen::DrawGlobe() {
    DrawGlobeInline();
}

void MainScreen::DrawRelated() {
    if (!mShowRelated) {
        return;
    }
    f32 alpha = mUnk25C;
    ut::Color highlight = gHighlightColor;
    ut::Color sep = gSeparatorColor;
    GXColor black;
    GXColor white;
    u8 a = 255.0f * alpha;
    u8 bgAlpha = 220.0f * alpha;
    black.r = 0;
    black.g = 0;
    black.b = 0;
    black.a = a;
    white.r = 255;
    white.g = 255;
    white.b = 255;
    white.a = a;
    math::VEC2 pos;
    pos.y = mUnk1A0;
    pos.x = mUnk19C - 0.5f * mUnk1A4;
    f32 fontScale;
    if (gLargeFont) {
        if (gLanguage == 0) {
            fontScale = 1.2f;
        } else {
            fontScale = 1.0f;
        }
    } else {
        fontScale = gDefaultFontScale;
    }
    f32 rowHeight = mUnk24C * fontScale * fn_80035188(lbl_80356970);
    f32 scrollY = mUnk254;
    highlight.a = highlight.a * mUnk25C;
    sep.a = sep.a * mUnk25C;

    Draw2D_SetupGX();
    Draw2D_SetOrtho();
    math::VEC3 quad[4];
    quad[0].z = quad[1].z = quad[2].z = quad[3].z = 0.0f;
    f32 left = mUnk19C - 0.5f * mUnk1A4;
    f32 top = mUnk1A0;
    f32 right = mUnk19C + 0.5f * mUnk1A4;
    f32 bottom = top + mUnk1A8;
    ut::Rect rect;
    rect.left = left;
    rect.top = top;
    rect.right = right;
    rect.bottom = bottom;
    Draw2D_SetupGX();
    ut::Color bg(0xDE, 0xDE, 0xDE, bgAlpha);
    Draw2D_FillRect(&rect, &bg);

    rect.right = left;
    rect.left = left - 1.0f;
    rect.top = top - 2.0f;
    rect.bottom = 1.0f + bottom;
    Draw2D_FillRect(&rect, (ut::Color*)&black);
    rect.left = 1.0f + right;
    rect.right = 2.0f + right;
    Draw2D_FillRect(&rect, (ut::Color*)&black);
    rect.bottom = top - 1.0f;
    rect.left = left - 1.0f;
    Draw2D_FillRect(&rect, (ut::Color*)&black);
    rect.top = bottom;
    rect.bottom = 1.0f + bottom;
    Draw2D_FillRect(&rect, (ut::Color*)&black);
    rect.left = left;
    rect.bottom = top;
    rect.right = 1.0f + right;
    rect.top = top - 1.0f;
    Draw2D_FillRect(&rect, (ut::Color*)&white);
    rect.left = right;
    rect.bottom = bottom;
    Draw2D_FillRect(&rect, (ut::Color*)&white);
    rect.left = left;
    rect.right = right;

    ut::Color titleColor0;
    titleColor0.r = 255;
    titleColor0.g = 255;
    titleColor0.b = 255;
    titleColor0.a = a;
    ut::Color titleColor1;
    titleColor1.r = 255;
    titleColor1.g = 0x88;
    titleColor1.b = 0x4B;
    titleColor1.a = a;
    {
        ut::TextWriterBase<wchar_t> writer;
        Draw2D_SetupGX();
        rect.top = 11.0f + top;
        rect.bottom = 45.0f + rect.top;
        GXSetTevColor(GX_TEVREG0, titleColor0);
        Draw2D_TexRect(gCommonTpl, 1, &rect, 0.0f, 0);
        f32 textX;
        f32 textY;
        textX = 20.0f + rect.left;
        textY = 22.5f + rect.top;
        rect.top = rect.bottom;
        rect.bottom = 11.0f + rect.bottom;
        GXSetTevColor(GX_TEVREG0, titleColor1);
        Draw2D_TexRect(gCommonTpl, 1, &rect, 0.0f, 0);
        writer.SetFont(*gSysFont);
        writer.SetDrawFlag(0x100);
        writer.SetupGX();
        writer.SetTextColor(ut::Color(0, 0, 0, a));
        writer.SetScale(1.0f);
        writer.SetCharSpace(gCharSpaceScale);
        writer.SetCursor(textX, textY);
        if (gUpdateMsgType == 1) {
            writer.Print(lbl_801B0D08[gLanguage]);
        } else {
            writer.Print(lbl_801B0E30[gLanguage]);
        }
    }

    pos.x += 5.0f;
    pos.y += 72.0f;
    u32 clipTop = pos.y;
    u32 clipBottom = mUnk1B0 - 10.0f;
    Draw2D_SetupGX();
    Draw2D_SetOrtho();
    Draw2D_SetScissor(0, clipTop, GetScreenWidth(), clipBottom);

    pos.y += scrollY;
    f32 margin = 5.0f;
    for (s32 i = 0; i < 4; i++) {
        if (mUnk304[i] >= 0) {
            quad[0].x = quad[1].x = pos.x - margin;
            quad[2].x = quad[3].x = (pos.x + mUnk1AC) - margin;
            f32 y = pos.y + rowHeight * mUnk304[i];
            quad[0].y = quad[3].y = y;
            quad[1].y = quad[2].y = y + rowHeight;
            Draw2D_FillQuad(quad, &highlight);
        }
    }

    f32 half = 0.5f * rowHeight;
    f32 lineHalf = half - 5.0f;
    f32 iconX;
    f32 startY = pos.y;
    iconX = pos.x + (5.0f + mUnk260);
    f32 scale = mUnk250;
    u32 drawFlag = mWriter->GetDrawFlag();
    f32 textOfs = 30.0f * scale;
    math::VEC3 iconPos(0.0f, 0.0f, 0.0f);
    ut::Color opaque;
    opaque.r = 255;
    opaque.g = 255;
    opaque.b = 255;
    opaque.a = 255;
    mWriter->SetDrawFlag(0x100);
    mWriter->SetFont(*gSysFont);
    mWriter->SetupGX();
    mWriter->SetCharSpace(gCharSpaceScale);
    mWriter->SetScale(0.8f * scale);
    Draw2D_SetScissor(pos.x, clipTop, 5.0f + mRelated->mNumber.mViewWidth, clipBottom);

    RelatedItem* item;
    pos.y = startY + half;
    for (item = mRelated; item != NULL; item = item->mNext) {
        if (pos.y > 0.0f && pos.y < 456.0f) {
            mWriter->SetCursor(pos.x + item->mNumber.mPos, pos.y);
            if (item->mArticle->mFlags & 1) {
                mWriter->SetTextColor(ut::Color(0x50, 0x50, 0x50, 0xFF));
            } else {
                mWriter->SetTextColor(ut::Color(0, 0, 0, 0xFF));
            }
            mWriter->Print(item->mArticle->unk40);
        }
        pos.y += rowHeight;
    }

    iconPos.x = iconX + textOfs;
    pos.y = startY + half;
    for (item = mRelated; item != NULL; item = item->mNext) {
        mWriter->SetCharSpace(scale * gCharSpaceScale);
        mWriter->SetScale(scale);
        if (pos.y > 0.0f && pos.y < 456.0f) {
            Draw2D_SetScissor(iconPos.x, clipTop, item->mText.mViewWidth - textOfs, clipBottom);
            mWriter->SetCursor(iconPos.x + item->mText.mPos, pos.y);
            if (item->mArticle->mFlags & 1) {
                mWriter->SetTextColor(ut::Color(0x50, 0x50, 0x50, 0xFF));
            } else {
                mWriter->SetTextColor(ut::Color(0, 0, 0, 0xFF));
            }
            fn_8000E180(item, mWriter);
        }
        pos.y += rowHeight;
    }

    Draw2D_SetupGX();
    GXSetTevColor(GX_TEVREG0, opaque);
    Draw2D_SetScissor(0, clipTop, GetScreenWidth(), clipBottom);
    pos.y = startY + half;
    for (item = mRelated; item != NULL; item = item->mNext) {
        if (pos.y > 0.0f && pos.y < 456.0f) {
            iconPos.x = iconX;
            iconPos.y = pos.y;
            ut::TextWriterBase<wchar_t>* writer = mWriter;
            u32 icon = item->mArticle->GetCategoryIcon();
            Draw2D_Icon(icon, &iconPos, scale, scale, writer->GetDrawFlag());
            if (item->mArticle->GetTexture()) {
                iconPos.x = iconX + item->mText.mViewWidth + item->mThumbX - 5.0f;
                iconPos.y += item->mThumbY;
                Draw2D_Texture(item->mArticle->GetTexture(), &iconPos, item->mThumbScale);
            }
        }
        pos.y += rowHeight;
    }

    quad[2].x = pos.x;
    pos.y = startY + half;
    quad[3].x = pos.x + (u32)(mUnk1AC - 10.0f);
    quad[2].y = quad[3].y = startY + rowHeight;
    for (item = mRelated; item != NULL; item = item->mNext) {
        if (pos.y > 0.0f && pos.y < 456.0f) {
            quad[0].x = quad[1].x = pos.x + mUnk260;
            quad[0].y = pos.y - lineHalf;
            quad[1].y = pos.y + lineHalf;
            Draw2D_Line(quad[0], quad[1], 6, sep, sep);
        }
        if (quad[2].y > 0.0f && quad[2].y < 456.0f) {
            Draw2D_Line(quad[2], quad[3], 6, sep, sep);
        }
        quad[2].y += rowHeight;
        quad[3].y += rowHeight;
        pos.y += rowHeight;
    }

    Draw2D_SetScissor(0, 0, GetScreenWidth(), 456);
    mWriter->SetFont(*gArticleFont);
    mWriter->SetDrawFlag(drawFlag);
}

void MainScreen::DrawButtons() {
    fn_800323F8(&mUnk164, mUnk23C, mUnk240);
    if (lbl_8035755C != NULL && lbl_8035755C->mMode != HeadlineList::MODE_SECTION) {
        s32 i;
        s32 count = lbl_803575E0 - 1;
        FrameTextButton** button = &mButtons[1];
        for (i = 0; i < count; i++, button++) {
            (*button)->Draw(mUnk23C);
        }
    }
}

void MainScreen::DrawButtons2() {
    DrawButtonsInline();
}

void MainScreen::DrawButtonsGlobe() {
    DrawButtonsInline();
    DrawGlobeInline();
}

#pragma auto_inline off
void MainScreen::DrawCursor() {
    f32 s = math::SinRad(1.5707964f * (mUnk34C / 15.0f));
    DrawScreenFade(255.0f * s);
    ut::Rect r0(0.0f, 0.0f, 0.0f, 0.0f);
    ut::Rect r1(0.0f, 0.0f, 0.0f, 0.0f);
    f32 scale = 1.0f + 0.05f * math::SinRad(1.5707964f * (mUnk350 / 8.0f));
    if (fn_8003567C(&r0, mUnk16C.x, 123.0f + mUnk224, scale) &&
        fn_80035764(&r1))
    {
        fn_80035A3C(&r0, &r1, s);
    }
}

#pragma auto_inline reset
void MainScreen::Update() {
    mSoundId = -1;
    if (mMode) {
        (this->*mMode)();
    } else {
        SetMode(&MainScreen::ModeMain);
    }
}

inline void MainScreen::ResetListPos() {
    fn_800323D8(&gTextScale, 0);
    mUnk154.x = mUnk230 = mUnk23C = 0.0f;
    fn_80032464(mUnk14C.x + GetSideMargin());
}

inline void MainScreen::LayoutTicker(const bool* held) {
    f32 zoom = fn_80035188(lbl_80356970);
    fn_80032414(&mUnk164, held, gTextScale, gTextScale - zoom);
}

void MainScreen::ResetZoom() {
    ResetListPos();
    LayoutTicker(NULL);
}


void MainScreen::SetMode(ModeFunc mode) {
    if (mMode) {
        mModeStep = -1;
        (this->*mMode)();
    }
    mMode = mode;
    mModeStep = 0;
    Update();
}

static inline void SetButtonEnabled(PaneButton* button, BOOL enabled) {
    if (enabled) {
        button->mDisabled = false;
    } else {
        DisableButton(button);
    }
}

void MainScreen::ModeMain() {
    HeadlineList* list = lbl_8035755C;
    Globe* globe = lbl_8035775C;
    Ticker* ticker = NULL;
    NewsArticle* article = NULL;
    BOOL held = FALSE;
    if (list != NULL) {
        ticker = GetListItem(list, mSelected);
        if (ticker != NULL) {
            article = ticker->mArticle;
        }
    }

    switch (mModeStep) {
    case 0:
        mModeStep++;
    default:
        fn_80032644();
        mHeld[0] = false;
        mHeld[1] = false;
        mHeld[2] = false;
        mHeld[3] = false;
        mZoomOutPressed = false;
        mUpPressed = false;
        mZoomInPressed = false;
        mBackPressed = false;
        mDownPressed = false;
        mSlidePressed = false;
        mEarthPressed = false;
        mRotAPressed = false;
        mRotBPressed = false;
        mResetPressed = false;
        mUnk2B6 = false;
        mUnk2B7 = false;
        mUnk2C0 = false;
        if (mUnk2BE) {
            mUnk324 -= 20;
            if (mUnk324 < 220) {
                mUnk324 = 220;
            }
        } else {
            mUnk324 += 20;
            if (mUnk324 > 255) {
                mUnk324 = 255;
            }
        }

        if (fn_8004DABC(lbl_8035775C)) {
            EnableButton(mResetButton);
        } else {
            DisableButton(mResetButton);
        }
        if (!gNewsData->mEmpty && list != NULL &&
            (list->mMode != HeadlineList::MODE_SECTION || list->mNumItems != 0))
        {
            EnableButton(mSlideButton);
        } else {
            DisableButton(mSlideButton);
        }
        EnableButton(mHeadBackButton);

        if (IsState(&MainScreen::State18770) || IsState(&MainScreen::State195B8)) {
            if (ticker != NULL) {
                if (article != NULL && article->mLocationName != NULL) {
                    EnableButton(fn_80048364(mMainLayout, "earth"));
                } else {
                    DisableButton(fn_80048364(mMainLayout, "earth"));
                }
            } else {
                DisableButton(fn_80048364(mMainLayout, "earth"));
            }
        } else if (IsState(&MainScreen::State1B694) || IsState(&MainScreen::State1BD60)) {
            if (mUnk2FC < mUnk2F8 - sVisibleRows[lbl_80356970]) {
                EnableButton(mDownButton);
            } else {
                DisableButton(mDownButton);
            }
            if (mUnk2FC != 0) {
                EnableButton(mUpButton);
            } else {
                DisableButton(mUpButton);
            }
            if (lbl_80356970 <= 0) {
                DisableButton(mZoomOutButton);
            } else {
                EnableButton(mZoomOutButton);
            }
            if (lbl_80356970 >= 9) {
                DisableButton(mZoomInButton);
            } else {
                EnableButton(mZoomInButton);
            }
            DisableButton(fn_80048364(mMainLayout, "earth"));
        }

        if (globe != NULL) {
            if (globe->mZoom >= 9) {
                DisableButton(mEarthZoomOutButton);
            } else {
                EnableButton(mEarthZoomOutButton);
            }
            if (globe->mZoom <= 0) {
                DisableButton(mEarthZoomInButton);
            } else {
                EnableButton(mEarthZoomInButton);
            }
            if (globe->mRotation <= 0) {
                DisableButton(mRotBButton);
            } else {
                EnableButton(mRotBButton);
            }
            if (globe->mRotation >= 10) {
                DisableButton(mRotAButton);
            } else {
                EnableButton(mRotAButton);
            }
        }

        fn_80048444(mMainLayout, 15);
        fn_80048444(mHeadLayout, 15);
        fn_80048444(mEarthLayout, 15);
        fn_80047F70(mMainLayout);
        fn_80047F70(mHeadLayout);
        fn_80047F70(mEarthLayout);

        if (!IsState(&MainScreen::State1C600)) {
            if (mActiveLayout) {
                fn_8004BD60(mActiveLayout, 0x23);
            }
            for (s32 i = 0; i < 4; i++) {
                if (gHold[i] & 0x400) {
                    mHeld[i] = true;
                    held = TRUE;
                }
            }
            if (mUnk134) {
                (this->*mUnk134)();
            }
            if (lbl_80357598 == 1) {
                return;
            }
            if (!gNewsData->mEmpty && fn_8004C13C("slide", 0x800) >= 0) {
                mSlidePressed = true;
                PlaySE(0x36);
                lbl_80357598 = 1;
                return;
            }
            if (!held) {
                if (fn_8004C000("up", 0x800) >= 0) {
                    mUpPressed = true;
                } else if (gRepeatFastAll & 8) {
                    mUpPressed = true;
                    if (mActiveLayout == mMainLayout) {
                        mUpButton->SetPressed(true);
                    } else if (mActiveLayout == mHeadLayout) {
                        mHeadUpButton->SetPressed(true);
                    }
                }
                if (fn_8004C000("down", 0x800) >= 0) {
                    mDownPressed = true;
                } else if (gRepeatFastAll & 4) {
                    mDownPressed = true;
                    if (mActiveLayout == mMainLayout) {
                        mDownButton->SetPressed(true);
                    } else if (mActiveLayout == mHeadLayout) {
                        mHeadDownButton->SetPressed(true);
                    }
                }
            }
            if (fn_8004C000("zoom_out", 0x800) >= 0) {
                mZoomOutPressed = true;
            } else if (gRepeatFastAll & 0x1000) {
                mZoomOutPressed = true;
                if (mActiveLayout == mMainLayout) {
                    mZoomOutButton->SetPressed(true);
                } else if (mActiveLayout == mHeadLayout) {
                    mHeadZoomOutButton->SetPressed(true);
                } else if (mActiveLayout == mEarthLayout) {
                    mEarthZoomOutButton->SetPressed(true);
                }
            }
            if (fn_8004C000("zoom_in", 0x800) >= 0) {
                mZoomInPressed = true;
            } else if (gRepeatFastAll & 0x10) {
                mZoomInPressed = true;
                if (mActiveLayout == mMainLayout) {
                    mZoomInButton->SetPressed(true);
                } else if (mActiveLayout == mHeadLayout) {
                    mHeadZoomInButton->SetPressed(true);
                } else if (mActiveLayout == mEarthLayout) {
                    mEarthZoomInButton->SetPressed(true);
                }
            }
            if (fn_8004C13C("back", 0x800) >= 0) {
                mBackPressed = true;
            }
            if (fn_8004C13C("earth", 0x800) >= 0) {
                mEarthPressed = true;
            }
            if (fn_8004C000("rot_a", 0x800) >= 0) {
                mRotAPressed = true;
            } else if (gRepeatFastAll & 8) {
                mRotAPressed = true;
                mRotAButton->SetPressed(true);
            }
            if (fn_8004C000("rot_b", 0x800) >= 0) {
                mRotBPressed = true;
            } else if (gRepeatFastAll & 4) {
                mRotBPressed = true;
                mRotBButton->SetPressed(true);
            }
            if (fn_8004C13C("reset", 0x800) >= 0) {
                mResetPressed = true;
            }
            if (gRepeatFastAll & 1) {
                mUnk2B6 = true;
            }
            if (gRepeatFastAll & 2) {
                mUnk2B7 = true;
            }
        }

        if (mState) {
            (this->*mState)(NULL);
        }
        if (IsState(&MainScreen::StateList) && fn_8003251C()) {
            lbl_80357598 = 2;
            return;
        }
        if (CheckSelect()) {
            if (IsState(&MainScreen::State16960) || IsState(&MainScreen::StateList)) {
                LayoutSectionButtons();
            }
        }

        Globe* g = lbl_8035775C;
        if (g != NULL) {
            mUnk32C += 0x200;
            f32 range = mUnk278 - mUnk274;
            if (mUnk32C > 0x8000) {
                mUnk32C = 0x8000;
            }
            g->mHeight = mUnk270 = mUnk274 + range * CosineEase(mUnk32C);
            if (lbl_8035775C != NULL && mUnk11C) {
                (this->*mUnk11C)();
            }
            fn_800329CC();
            if (lbl_8035775C != NULL && mUnk128) {
                (this->*mUnk128)();
            }
            fn_8004D0B0(g);
            fn_8004D170(g);
            if (mSoundId != 0xFFFFFFFF) {
                PlaySE(mSoundId);
            }
        }
        break;
    case -1:
        break;
    }
}

void MainScreen::ModeWait() {
    switch (mModeStep) {
    case 0: {
        mModeStep++;
        fn_80048D20(lbl_80357730, 30);
        ut::Color color(0, 0, 0, 0xA0);
        *(GXColor*)((u8*)lbl_80357730 + 0x10) = color;
        fn_800491EC(lbl_80357730, color, 0);
    }
    default:
        if (*(s32*)((u8*)lbl_80357730 + 0x50) == 0 && lbl_80357760 != 0) {
            fn_80048C80(lbl_80357730, 30);
            OpenSelected();
            SetMode(&MainScreen::ModeMain);
        }
        break;
    case -1:
        break;
    }
}

BOOL MainScreen::CheckSelect() {
    bool selectable = false;
    bool noInput = false;
    if (!mZoomInPressed && !mUpPressed && !mZoomOutPressed && !mBackPressed && !mDownPressed &&
        !mSlidePressed)
    {
        noInput = true;
    }
    if (noInput) {
        if (IsState(&MainScreen::StateList)) {
            selectable = true;
        }
    }
    if (selectable && lbl_8035755C != NULL) {
        if (lbl_8035755C->mSelected >= 0) {
            mSelected = lbl_8035755C->mSelected;
            Ticker* ticker = GetListItem(lbl_8035755C, mSelected);
            if (ticker != NULL) {
                u8 flags = ticker->mArticle->mFlags;
                if ((flags & 2) && !(flags & 1)) {
                    PlaySE(0x3E);
                } else {
                    PlaySE(0x3D);
                }
                if (lbl_80357760 == 0) {
                    SetMode(&MainScreen::ModeWait);
                    return FALSE;
                }
                OpenSelected();
            }
        }
    }
    return TRUE;
}

s32 MainScreen::OpenArticle(s32* arg) {
    HeadlineList* list = lbl_8035755C;
    Ticker* ticker = GetListItem(list, mSelected);
    if (ticker != NULL) {
        NewsArticle* article = ticker->mArticle;
        math::VEC2 thumbPos = ticker->GetThumbPos();
        s32 noLocation;
        s32 result;
        math::VEC2 origin;
        math::VEC2 size;
        if (article->mLocationName == NULL) {
            result = 1;
            noLocation = 1;
            mUnk164.x = mUnk234 = 0.0f;
            mUnk238 = GetScreenWidth();
            mUnk16C.x = mUnk164.x + GetSideMargin();
            f32 h = 330.0f;
            size.x = GetContentRight() - GetSideMargin();
            size.y = h;
        } else {
            result = 2;
            noLocation = 0;
            mUnk234 = 0.0f;
            mUnk238 = (f32)GetCursorAreaRight();
            mUnk16C.x = mUnk164.x + GetSideMargin();
            f32 h = 330.0f;
            size.x = mUnk238 - GetSideMargin() - 5.0f;
            size.y = h;
            PlaySE(sRegionSE[((u8*)article->mLocation)[0xC]]);
            if (arg != NULL && *arg == 1) {
                fn_80032658(((u8*)article->mLocation)[0xC]);
            } else {
                fn_80032580(article, 5, gWidescreen ? -0.7125f : -0.625f, 0.0f);
            }
            lbl_8035697C = 0;
            SetLocation(article);
        }
        ticker->GetOrigin(origin);
        origin.y = (origin.y + list->mScroll) - 40.0f;
        thumbPos.y += list->mScroll;
        mUnk224 = 0.0f;
        mUnk228 = 0.0f;
        NewsTexture* texture = ticker->mArticle->GetTexture();
        BOOL hasLocation = article->mLocationName != NULL;
        fn_8003300C(article, list->mCategory->mName, texture, &origin, &thumbPos,
                    ticker->GetThumbScale(), &size, noLocation, hasLocation,
                    gTextScale);
        mUnk2E4 = mUnk2C4;
        mUnk2E8 = mSelected;
        return result;
    }
    return 0;
}

void MainScreen::OpenSelected() {
    switch (OpenArticle(NULL)) {
    case 1:
        ChangeState(&MainScreen::State18770, NULL);
        break;
    case 2:
        ChangeState(&MainScreen::State18770, NULL);
        break;
    }
}

void MainScreen::ChangeState(StateFunc state, s32* arg) {
    if (mState) {
        mStateStep = -1;
        (this->*mState)(arg);
    }
    if (state == &MainScreen::State17E6C || state == &MainScreen::State195A0 ||
        state == &MainScreen::State1B694 || state == &MainScreen::State1BD60 ||
        state == &MainScreen::State1C600)
    {
        lbl_803575BF = true;
    } else {
        lbl_803575BF = false;
    }
    mUnk2BE = false;
    mState = state;
    mStateStep = 0;
    if (mState) {
        (this->*mState)(arg);
    }
}

inline void MainScreen::SetSubState(Func func) {
    if (mUnk104) {
        mUnk2D4 = -1;
        (this->*mUnk104)();
    }
    mUnk104 = func;
    mUnk2D4 = 0;
    if (mUnk104) {
        (this->*mUnk104)();
    }
}

inline void MainScreen::SetInputHook(Func func) {
    if (!func) {
        lbl_80356CA0 = gUpdateMsgType == 1;
    }
    mUnk134 = func;
}

inline void MainScreen::ScrollArticle() {
    f32 delta = gTextScale - fn_80035188(lbl_80356970);
    f32 min = fn_80034844();
    if (mUnk224 > 0.0f) {
        mUnk228 = mUnk224 = 0.0f;
    } else if (mUnk224 < min) {
        mUnk228 = mUnk224 = min;
    }
    fn_80034CDC();
    math::VEC2 pos(mUnk16C.x, 123.0f + mUnk224);
    fn_800332B4(&pos, delta);
}

inline void MainScreen::SetArticleWidth() {
    fn_8003481C(fn_80034158() ? GetContentRight() - GetSideMargin()
                              : mUnk238 - GetSideMargin() - 5.0f);
}

inline void MainScreen::LayoutArticle() {
    mUnk16C.x = fn_80034158() ? mUnk164.x + GetSideMargin() : mUnk164.x + GetSideMargin();
    SetArticleWidth();
    ScrollArticle();
}

inline void MainScreen::LayoutList() {
    fn_80032464(mUnk14C.x + GetSideMargin());
    LayoutTicker(NULL);
}

inline void MainScreen::UpdateHeadButtons() {
    HeadlineList* list = lbl_8035755C;
    if (list != NULL) {
        if (list->mMode != HeadlineList::MODE_SECTION) {
            mHeadBackButton->SetSelIndex(0);
        } else {
            mHeadBackButton->SetSelIndex(1);
            if (list->mNumItems == 0) {
                DisableButton(mHeadDownButton);
                DisableButton(mHeadUpButton);
            }
        }
        if (list->mNumItems == 0) {
            DisableButton(mHeadZoomInButton);
            DisableButton(mHeadZoomOutButton);
        }
    } else {
        mHeadBackButton->SetSelIndex(1);
    }
}

void MainScreen::SetListHook() {
    SetInputHook(&MainScreen::Hook1ED20);
}

void MainScreen::EaseZoom() {
    Ease(&mUnk23C, mUnk288, 0.2f, 1.0f, 0.01f);
    mUnk244 = 1.0f - mUnk23C;
}

void MainScreen::State16960(s32* arg) {
    HeadlineList* list = lbl_8035755C;
    switch (mStateStep) {
    case -1:
        lbl_803575BC = false;
        break;
    case 0: {
        fn_80030720(0);
        lbl_803575BC = true;
        mActiveLayout = mHeadLayout;
        SetSubState(NULL);
        SetInputHook(&MainScreen::Hook1ED20);
        if (list != NULL && list->mMode != HeadlineList::MODE_SECTION) {
            fn_80032554(&mUnk330);
        }
        ScrollArticle();
        fn_800323BC(&gTextScale);
        LayoutTicker(NULL);
        mUnk2A4 = 1.0f / 15.0f;
        mUnk23C = 0.0f;
        mUnk288 = 1.0f;
        mUnk338 = 0;
        mUnk240 = 1.0f;
        mUnk28C = __fabsf(mUnk288 - mUnk23C) / 15.0f;
        if (arg != NULL) {
            mStateStep = 2;
            mDraw = &MainScreen::DrawButtons2;
            mUnk230 = -GetScreenWidth();
            mUnk33C = 0x71C;
            switch (*arg) {
            case 0:
                mUnk154.x = mUnk230 - 150.0f;
                mUnk14C.x = mUnk154.x + GetScreenWidth();
                break;
            case 1:
            default:
                mUnk154.x = 150.0f + mUnk230;
                mUnk14C.x = mUnk154.x + GetScreenWidth();
                mUnk240 = 0.0f;
                if (list != NULL && list->mNumItems == 0) {
                    mTextButton->mHidden = false;
                }
                break;
            }
            mUnk280 = mUnk154.x;
            mUnk284 = mUnk230 - mUnk280;
        } else {
            mStateStep = 1;
            mDraw = &MainScreen::DrawButtonsGlobe;
            mUnk234 = 0.0f;
            mUnk280 = mUnk164.x;
            mUnk284 = mUnk234 - mUnk280;
            mUnk33C = 0x71C;
            if (list != NULL) {
                Ticker* ticker = GetListItem(list, mSelected);
                if (ticker != NULL) {
                    math::VEC2 origin;
                    f32 dy = list->mScroll - mUnk224;
                    math::VEC2 thumbPos = ticker->GetThumbPos();
                    math::VEC2 pos(mUnk16C.x, 123.0f + mUnk224);
                    dy -= 40.0f;
                    ticker->GetOrigin(origin);
                    origin.y += dy;
                    thumbPos.y += dy;
                    f32 thumbScale = ticker->mThumbScale;
                    fn_8003356C(&pos, &origin, &thumbPos, &thumbScale);
                }
                if (list->mNumItems == 0) {
                    mTextButton->mHidden = false;
                }
            }
        }
        fn_80032464(mUnk14C.x + GetSideMargin());
        mUnk238 = GetScreenWidth();
        mUnk290 = mScreenRect.right;
        mUnk294 = mUnk238 - mUnk290;
        UpdateHeadButtons();
        if (lbl_80356970 <= 0) {
            DisableButton(fn_80048364(mHeadLayout, "zoom_out"));
        } else {
            EnableButton(fn_80048364(mHeadLayout, "zoom_out"));
        }
        if (lbl_80356970 >= 9) {
            DisableButton(fn_80048364(mHeadLayout, "zoom_in"));
        } else {
            EnableButton(fn_80048364(mHeadLayout, "zoom_in"));
        }
        break;
    }
    default: {
        UpdateHeadButtons();
        if (mBackPressed && list != NULL && list->mMode != HeadlineList::MODE_SECTION) {
            PlaySE(0x21);
            lbl_80357729 = true;
            return;
        }
        mUnk338 += mUnk33C;
        if (mUnk338 > 0x8000) {
            mUnk338 = 0x8000;
        }
        f32 t = CosineEase(mUnk338);
        mScreenRect.right = mUnk290 + mUnk294 * t;
        mUnk23C += 1.0f / 18.0f;
        if (mUnk23C > 1.0f) {
            mUnk23C = 1.0f;
        }
        mUnk244 -= 1.0f / 9.0f;
        if (mUnk244 < 0.0f) {
            mUnk244 = 0.0f;
        }
        mUnk240 += mUnk2A4;
        if (mUnk240 > 1.0f) {
            mUnk240 = 1.0f;
        }
        switch (mStateStep) {
        case 1:
            if (mUnk338 >= 0x8000 && IsNearlyZero(mUnk288 - mUnk23C)) {
                mUnk23C = 1.0f;
                mUnk244 = 0.0f;
                mUnk164.x = mUnk234;
                mScreenRect.right = mUnk238;
                LayoutArticle();
                ChangeState(&MainScreen::StateList, NULL);
                return;
            }
            mUnk164.x = mUnk280 + mUnk284 * t;
            LayoutArticle();
            break;
        case 2:
        default:
            if (mUnk338 >= 0x8000 && IsNearlyZero(mUnk288 - mUnk23C)) {
                mUnk23C = 1.0f;
                mUnk154.x = mUnk230;
                mScreenRect.right = mUnk238;
                if (mStateStep == 2) {
                    mUnk14C.x = mUnk154.x + GetScreenWidth();
                }
                LayoutList();
                ChangeState(&MainScreen::StateList, NULL);
                return;
            }
            mUnk154.x = mUnk280 + mUnk284 * t;
            if (mStateStep == 2) {
                mUnk14C.x = mUnk154.x + GetScreenWidth();
            }
            LayoutList();
            break;
        }
        break;
    }
    }
}

inline void MainScreen::SetButtonsEnabled(bool enabled) {
    for (u32 i = 0; i < lbl_803575E0; i++) {
        mButtons[i]->SetEnabled(enabled);
    }
}

void MainScreen::StateList(s32* arg) {
    HeadlineList* list = lbl_8035755C;
    switch (mStateStep) {
    case -1:
        mPrevState = mState;
        mTextButton->Hide();
        SetButtonsEnabled(false);
        fn_80032A94(0);
        lbl_803575BC = false;
        break;
    case 0:
        mStateStep++;
        fn_80030720(0);
        lbl_803575BC = true;
        mUnk23C = 1.0f;
        mUnk244 = 0.0f;
        mActiveLayout = mHeadLayout;
        fn_800323BC(&gTextScale);
        SetSubState(&MainScreen::Sub1D9DC);
        SetListHook();
        mDraw = &MainScreen::DrawButtons;
        mUnk154.x = mUnk230 = -GetScreenWidth();
        mUnk14C.x = 0.0f;
        mUnk164.x = 0.0f;
        lbl_8035697C = true;
        fn_80032464(mUnk14C.x + GetSideMargin());
        UpdateHeadButtons();
        EnableButton(mHeadBackButton);
        if (list != NULL && list->mMode != HeadlineList::MODE_SECTION) {
            SetButtonsEnabled(true);
        }
        if (!gNewsData->mEmpty && list != NULL &&
            (list->mMode != HeadlineList::MODE_SECTION || list->mNumItems != 0))
        {
            EnableButton(mSlideButton);
        } else {
            DisableButton(mSlideButton);
        }
        break;
    default:
        UpdateHeadButtons();
        switch (mStateStep) {
        case 1:
            if (mBackPressed) {
                if (list != NULL && list->mMode == HeadlineList::MODE_SECTION) {
                    PlaySE(0x22);
                } else {
                    PlaySE(0x21);
                    lbl_80357729 = true;
                    return;
                }
                mUnk338 = 8;
                mStateStep++;
                return;
            }
            if (CheckSectionButtons()) {
                return;
            }
            if (list != NULL && list->mNumItems != 0) {
                if (fn_8003519C(mZoomInPressed, mZoomOutPressed) && list != NULL) {
                    list->ResetItems();
                }
            } else {
                fn_8003519C(false, false);
            }
            if (mUnk104) {
                (this->*mUnk104)();
            }
            if (list != NULL) {
                if (list->mNumItems == 0) {
                    DisableButton(mHeadZoomInButton);
                    DisableButton(mHeadZoomOutButton);
                } else {
                    if (lbl_80356970 <= 0) {
                        DisableButton(mHeadZoomOutButton);
                    } else {
                        EnableButton(mHeadZoomOutButton);
                    }
                    if (lbl_80356970 >= 9) {
                        DisableButton(mHeadZoomInButton);
                    } else {
                        EnableButton(mHeadZoomInButton);
                    }
                }
            } else {
                DisableButton(mHeadZoomInButton);
                DisableButton(mHeadZoomOutButton);
            }
            break;
        default:
            if (mUnk338 != 0) {
                mUnk338--;
                break;
            }
            if (lbl_8035755C != NULL && lbl_8035755C->mMode != HeadlineList::MODE_SECTION) {
                lbl_80357729 = true;
                return;
            }
            ReturnToTop();
            return;
        }
        break;
    }
    LayoutTicker(mHeld);
}

void MainScreen::ReturnToTop() {
    s32 dir = 0;
    mUnk2C4 = 0;
    lbl_8035755C = mLists[0];
    ResetListPos();
    ChangeState(&MainScreen::State16960, &dir);
}

BOOL MainScreen::CheckSectionButtons() {
    FrameTextButton** button = &mButtons[1];
    s32 count = lbl_803575E0 - 1;
    for (s32 i = 0; i < count; i++, button++) {
        if ((*button)->GetPressedChan() >= 0) {
            s32 dir = 1;
            mUnk330 = *fn_80032538();
            s32 section = i + 1;
            mUnk2C4 = section;
            lbl_8035755C = mLists[section];
            ResetListPos();
            ChangeState(&MainScreen::State16960, &dir);
            PlaySE(0x21);
            return TRUE;
        }
    }
    return FALSE;
}

inline void MainScreen::SetGlobeFunc(Func func) {
    if (mUnk11C) {
        mUnk2D8 = -1;
        (this->*mUnk11C)();
    }
    mUnk11C = func;
    mUnk2D8 = 0;
    if (lbl_8035775C != NULL && mUnk11C) {
        (this->*mUnk11C)();
    }
}

inline void MainScreen::UpdateGlobeCamera() {
    Globe* globe = lbl_8035775C;
    if (globe != NULL) {
        GlobeCamera* camera = globe->mCamera;
        if (camera != NULL) {
            f32 t = CosineEase(mUnk32C);
            f32 dx = mUnk194.x - mUnk18C.x;
            camera->mX = mUnk18C.x + dx * t;
            f32 dy = mUnk194.y - mUnk18C.y;
            camera->mY = mUnk18C.y + dy * t;
            globe->mX = camera->mX;
            globe->mY = camera->mY;
        }
    }
}

inline void MainScreen::UpdateScrollBar() {
    if (mActiveLayout != NULL) {
        LayoutScroll* scroll = (LayoutScroll*)mActiveLayout;
        lbl_803575C8 = mUnk26C * ((f32)scroll->mPos / (f32)scroll->mMax);
    } else {
        lbl_803575C8 = 0.0f;
    }
}

void MainScreen::State17E6C(s32* arg) {
    switch (mStateStep) {
    case -1:
        lbl_803575BD = false;
        lbl_803575A8 = 0;
        break;
    case 0: {
        mStateStep++;
        fn_80030544();
        lbl_803575BD = true;
        lbl_803575A8 = 1;
        mUnk334 = 1;
        mUnk2BE = true;
        mActiveLayout = mMainLayout;
        DisableButton(fn_80048364(mMainLayout, "earth"));
        DisableButton(mUpButton);
        SetSubState(NULL);
        SetInputHook(&MainScreen::Hook1E758);
        mUnk230 = -(GetScreenWidth() * 2);
        fn_80032464(mUnk14C.x + GetSideMargin());
        mUnk164.x = mUnk234;
        mScreenRect.right = mUnk238;
        mUnk16C.x = fn_80034158() ? mUnk164.x + GetSideMargin() : mUnk164.x + GetSideMargin();
        if (arg != NULL) {
            mUnk244 = mUnk248 = 1.0f;
            mUnk338 = 0x8000;
            mUnk33C = 0xAAA;
            mUnk340 = 0;
        } else {
            RelatedItem* item = mRelated;
            mUnk244 = mUnk248 = 0.0f;
            mUnk338 = 0;
            mUnk33C = 0xAAA;
            mUnk340 = 0;
            mUnk280 = mUnk234;
            mUnk284 = mUnk238;
            mUnk288 = 0.0f;
            mUnk28C = 456.0f;
            math::VEC2 ofs(fn_8000D6A0(item).x - mUnk16C.x, fn_8000D6A0(item).y - (123.0f + mUnk224));
            mUnk280 = mUnk284 = mFadeRect.left = mFadeRect.right = fn_8000D6A0(item).x;
            mUnk288 = mUnk28C = mFadeRect.top = mFadeRect.bottom = fn_8000D6A0(item).y;
            mUnk2BF = true;
            mUnk328 = 255;
            mUnk290 = mUnk234 - mUnk280;
            mUnk294 = mUnk238 - mUnk284;
            mUnk298 = -mUnk288;
            mUnk29C = 456.0f - mUnk28C;
        }
        mDraw = &MainScreen::DrawGlobe;
        fn_80032A94(1);
        fn_80032B04(mUnk2C4, mSelected, 1);
        SetGlobeFunc(&MainScreen::Globe1DF3C);
        Globe* globe = lbl_8035775C;
        if (globe != NULL) {
            mUnk274 = globe->mHeight;
            mUnk278 = gWidescreen ? -0.7125f : -0.625f;
            mUnk32C = 0;
        }
        fn_800333C4(gTextScale);
        ScrollArticle();
        if (lbl_80357560 != NULL) {
            lbl_80357560->MarkRead();
        }
        UpdateScrollBar();
        break;
    }
    default: {
        lbl_803575A8 = 1;
        UpdateGlobeCamera();
        mUnk338 += mUnk33C;
        mUnk340++;
        if (mUnk338 > 0x8000) {
            mUnk338 = 0x8000;
        }
        f32 t = CosineEase(mUnk338);
        mUnk244 += 1.0f / 12.0f;
        if (mUnk244 > 1.0f) {
            mUnk244 = 1.0f;
        }
        mUnk248 = mUnk244;
        mUnk328 -= 21;
        if (mUnk328 < 0) {
            mUnk328 = 0;
        }
        if (mStateStep == 1) {
            fn_80032B04(mUnk2C4, mSelected, 1);
            UpdateGlobeCamera();
            Globe* globe = lbl_8035775C;
            if (mUnk2BF) {
                mFadeRect.left = mUnk280 + mUnk290 * t;
                mFadeRect.right = mUnk284 + mUnk294 * t;
                mFadeRect.top = mUnk288 + mUnk298 * t;
                mFadeRect.bottom = mUnk28C + mUnk29C * t;
            }
            if (globe == NULL || mUnk338 >= 0x8000) {
                mUnk2BF = false;
                mUnk164.x = mUnk234;
                mUnk16C.x =
                    fn_80034158() ? mUnk164.x + GetSideMargin() : mUnk164.x + GetSideMargin();
                ChangeState(&MainScreen::State195A0, NULL);
                return;
            }
        }
        fn_80033374(gTextScale);
        ScrollArticle();
        UpdateScrollBar();
        break;
    }
    }
}

inline NewsArticle* MainScreen::GetSelectedArticle() {
    HeadlineList* list = lbl_8035755C;
    Ticker* ticker = list != NULL ? GetListItem(list, mSelected) : NULL;
    return ticker != NULL ? ticker->mArticle : NULL;
}

void MainScreen::State18770(s32* arg) {
    switch (mStateStep) {
    case -1:
        lbl_803575BD = false;
        lbl_803575A8 = 0;
        break;
    case 0:
        fn_80030544();
        lbl_803575BD = true;
        lbl_803575A8 = 1;
        mUnk334 = 0;
        mActiveLayout = mMainLayout;
        DisableButton(mUpButton);
        SetSubState(NULL);
        SetInputHook(&MainScreen::Hook1E758);
        mUnk230 = -(GetScreenWidth() * 2);
        fn_80032464(mUnk14C.x + GetSideMargin());
        mUnk338 = 0;
        mUnk244 = mUnk248 = 1.0f;
        mUnk280 = mUnk164.x;
        mUnk284 = mUnk234 - mUnk164.x;
        mUnk290 = mScreenRect.right;
        mUnk294 = mUnk238 - mScreenRect.right;
        if (arg != NULL) {
            mStateStep = 2;
            mDraw = &MainScreen::DrawGlobe;
            fn_80032AC0(mUnk2C4, mSelected, 1);
            fn_80032B04(mUnk2C4, mSelected, 1);
            SetGlobeFunc(&MainScreen::Globe1DF3C);
            Globe* globe = lbl_8035775C;
            if (globe != NULL) {
                mUnk274 = globe->mHeight;
                mUnk278 = gWidescreen ? -0.7125f : -0.625f;
                mUnk32C = 0;
            }
            mUnk33C = 0x71C;
        } else {
            mStateStep++;
            mDraw = &MainScreen::DrawButtonsGlobe;
            fn_80032A94(0);
            SetGlobeFunc(&MainScreen::Globe1DF3C);
            mUnk288 = 0.0f;
            mUnk33C = 0x71C;
            mUnk28C = __fabsf(mUnk288 - mUnk23C) / 30.0f;
        }
        fn_800333C4(gTextScale);
        ScrollArticle();
        if (lbl_80357560 != NULL) {
            lbl_80357560->MarkRead();
        }
        UpdateScrollBar();
        if (lbl_80356970 <= 0) {
            DisableButton(fn_80048364(mMainLayout, "zoom_out"));
        } else {
            EnableButton(fn_80048364(mMainLayout, "zoom_out"));
        }
        if (lbl_80356970 >= 9) {
            DisableButton(fn_80048364(mMainLayout, "zoom_in"));
        } else {
            EnableButton(fn_80048364(mMainLayout, "zoom_in"));
        }
        break;
    default: {
        lbl_803575A8 = 1;
        f32 prevWidth = mUnk238;
        if (CheckArticleSwitch()) {
            NewsArticle* article = GetSelectedArticle();
            if (article != NULL && article->mLocationName != NULL &&
                (s32)mUnk238 != GetScreenWidth())
            {
                fn_80032AC0(mUnk2C4, mSelected, 1);
                fn_80032B04(mUnk2C4, mSelected, 1);
            } else {
                fn_80032A94(0);
            }
            if (!IsNearlyZero(prevWidth - mUnk238)) {
                mUnk338 = 0;
                mUnk280 = mUnk164.x;
                mUnk284 = mUnk234 - mUnk164.x;
                mUnk290 = mScreenRect.right;
                mUnk294 = mUnk238 - mScreenRect.right;
            }
            return;
        }
        mUnk16C.x = fn_80034158() ? mUnk164.x + GetSideMargin() : mUnk164.x + GetSideMargin();
        mUnk338 += mUnk33C;
        if (mUnk338 > 0x8000) {
            mUnk338 = 0x8000;
        }
        f32 t = CosineEase(mUnk338);
        f32 x = mUnk280 + mUnk284 * t;
        f32 right = mUnk290 + mUnk294 * t;
        mUnk240 -= 1.0f / 18.0f;
        mUnk164.x = x;
        mScreenRect.right = right;
        if (mUnk240 < 0.0f) {
            mUnk240 = 0.0f;
        }
        switch (mStateStep) {
        case 1:
            EaseZoom();
            if (mUnk338 >= 0x8000 && IsNearlyZero(mUnk288 - mUnk23C)) {
                mUnk164.x = mUnk234;
                SetArticleWidth();
                if (IsState(&MainScreen::State17E6C)) {
                    ChangeState(&MainScreen::State195A0, NULL);
                } else {
                    ChangeState(&MainScreen::State195B8, NULL);
                }
                return;
            }
            SetArticleWidth();
            break;
        case 2:
        default:
            fn_80032B04(mUnk2C4, mSelected, 1);
            UpdateGlobeCamera();
            if (lbl_8035775C == NULL || mUnk338 >= 0x8000) {
                mUnk164.x = mUnk234;
                if (IsState(&MainScreen::State17E6C)) {
                    ChangeState(&MainScreen::State195A0, NULL);
                } else {
                    ChangeState(&MainScreen::State195B8, NULL);
                }
                return;
            }
            break;
        }
        fn_80033374(gTextScale);
        ScrollArticle();
        UpdateScrollBar();
        break;
    }
    }
}

BOOL MainScreen::CheckArticleSwitch() {
    HeadlineList* list = lbl_8035755C;
    NewsLocationRec* location = NULL;
    s32 count = 0;
    if (list != NULL) {
        count = list->mNumItems;
        Ticker* ticker = GetListItem(list, mSelected);
        if (ticker != NULL) {
            NewsArticle* article = ticker->mArticle;
            if (article != NULL) {
                location = article->mLocation;
            }
        }
    }
    if (count > 1) {
        s32 hasLocation = location != NULL;
        if (mUnk2B7 && mSelected < count - 1) {
            mSelected++;
            OpenArticle(&hasLocation);
            fn_800333C4(gTextScale);
            fn_80033538();
            ScrollArticle();
            PlaySE(0x38);
            if (lbl_80357560 != NULL) {
                lbl_80357560->MarkRead();
            }
            return TRUE;
        }
        if (mUnk2B6 && mSelected > 0) {
            mSelected--;
            OpenArticle(&hasLocation);
            fn_800333C4(gTextScale);
            fn_80033538();
            ScrollArticle();
            PlaySE(0x39);
            if (lbl_80357560 != NULL) {
                lbl_80357560->MarkRead();
            }
            return TRUE;
        }
    }
    return FALSE;
}

void MainScreen::State195A0(s32* arg) {
    if (mStateStep == 0) {
        mUnk2BE = true;
    }
    State195B8(arg);
}

inline void MainScreen::SetDragRect(s32 chan) {
    if (mDragStart[chan].y < mDragPos[chan].y) {
        mUnk1F4.left = mDragStart[chan].x;
        mUnk1F4.top = mDragStart[chan].y;
        mUnk1F4.right = mDragPos[chan].x;
        mUnk1F4.bottom = mDragPos[chan].y;
    } else {
        mUnk1F4.top = mDragPos[chan].y;
        mUnk1F4.left = mDragPos[chan].x;
        mUnk1F4.bottom = mDragStart[chan].y;
        mUnk1F4.right = mDragStart[chan].x;
    }
}

inline bool MainScreen::NoButtonPressed() {
    return !mZoomInPressed && !mUpPressed && !mZoomOutPressed && !mBackPressed && !mDownPressed &&
           !mSlidePressed;
}

void MainScreen::CloseArticle() {
    if (IsState(&MainScreen::State195A0)) {
        PlaySE(0x3C);
        if (mRelated != NULL && (mRelated->mNext != NULL || mRelated->mPrev != NULL)) {
            fn_80030650();
            ChangeState(&MainScreen::State1B694, NULL);
        } else {
            ChangeState(&MainScreen::State1A750, NULL);
        }
        return;
    }
    PlaySE(0x3F);
    ChangeState(&MainScreen::State16960, NULL);
}

void MainScreen::OpenGlobe() {
    PlaySE(0);
    if (IsState(&MainScreen::State195A0)) {
        ChangeState(&MainScreen::State1A750, NULL);
    } else {
        ChangeState(&MainScreen::State1AC60, NULL);
    }
}

void MainScreen::State195B8(s32* arg) {
    HeadlineList* list = lbl_8035755C;
    NewsLocationRec* location = NULL;
    s32 count = 0;
    if (list != NULL) {
        count = list->mNumItems;
        Ticker* ticker = GetListItem(list, mSelected);
        if (ticker != NULL) {
            NewsArticle* article = ticker->mArticle;
            if (article != NULL) {
                location = article->mLocation;
            }
        }
    }

    switch (mStateStep) {
    case -1:
        lbl_801EDFD0[0] = 1;
        lbl_801EDFD0[1] = 1;
        lbl_801EDFD0[2] = 1;
        lbl_801EDFD0[3] = 1;
        lbl_80357580 = 0;
        mUnk2B8 = false;
        mPrevState = mState;
        lbl_803575BD = false;
        lbl_803575A8 = 0;
        break;
    case 0:
        mStateStep++;
        fn_80030544();
        lbl_803575BD = true;
        lbl_803575A8 = 1;
        mUnk334 = IsState(&MainScreen::State195A0);
        mUnk248 = 1.0f;
        mUnk23C = 0.0f;
        mUnk244 = 1.0f;
        mActiveLayout = mMainLayout;
        mUnk228 = mUnk224 = fn_80034164();
        SetSubState(&MainScreen::Sub1D338);
        SetInputHook(&MainScreen::Hook1E758);
        mDraw = &MainScreen::DrawGlobe;
        SetGlobeFunc(&MainScreen::Globe1DF3C);
        mUnk154.x = mUnk230 = -GetScreenWidth();
        fn_80032464(mUnk14C.x + GetSideMargin());
        fn_80033374(gTextScale);
        fn_800333C4(gTextScale);
        ScrollArticle();
        fn_80032A94(1);
        fn_80032B04(mUnk2C4, mSelected, 1);
        if (lbl_80357560 != NULL) {
            lbl_80357560->MarkRead();
        }
        UpdateScrollBar();
        break;
    default: {
        fn_80032B04(mUnk2C4, mSelected, 1);
        lbl_803575A8 = 1;
        BOOL zoomed = fn_8003519C(mZoomInPressed, mZoomOutPressed);
        if (lbl_80356970 <= 0) {
            DisableButton(fn_80048364(mMainLayout, "zoom_out"));
        } else {
            EnableButton(fn_80048364(mMainLayout, "zoom_out"));
        }
        if (lbl_80356970 >= 9) {
            DisableButton(fn_80048364(mMainLayout, "zoom_in"));
        } else {
            EnableButton(fn_80048364(mMainLayout, "zoom_in"));
        }
        UpdateGlobeCamera();

        if (mEarthPressed && location != NULL) {
            OpenGlobe();
            return;
        }
        if (mBackPressed) {
            CloseArticle();
            return;
        }

        bool canSwitch = false;
        if (IsState(&MainScreen::State195B8) && count > 1) {
            canSwitch = true;
        }
        if (canSwitch) {
            s32 hasLocation = location != NULL;
            if (mUnk2B7 && mSelected < count - 1) {
                mSelected++;
                switch (OpenArticle(&hasLocation)) {
                case 1:
                    ChangeState(&MainScreen::State18770, NULL);
                    break;
                case 2:
                    ChangeState(&MainScreen::State18770, &hasLocation);
                    break;
                }
                fn_800333C4(gTextScale);
                fn_80033538();
                PlaySE(0x38);
                return;
            }
            if (mUnk2B6 && mSelected > 0) {
                mSelected--;
                switch (OpenArticle(&hasLocation)) {
                case 1:
                    ChangeState(&MainScreen::State18770, NULL);
                    break;
                case 2:
                    ChangeState(&MainScreen::State18770, &hasLocation);
                    break;
                }
                fn_800333C4(gTextScale);
                fn_80033538();
                PlaySE(0x39);
                return;
            }
        } else if (IsState(&MainScreen::State195A0)) {
            RelatedItem* item = mCurRelated;
            s32 dir = 0;
            if (item != NULL) {
                if (mUnk2B7 && item->mNext != NULL) {
                    OpenRelated(item->mNext, &dir);
                    return;
                }
                if (mUnk2B6 && item->mPrev != NULL) {
                    OpenRelated(item->mPrev, &dir);
                    return;
                }
            }
        }

        if (!mZoomInPressed && !mUpPressed && !mZoomOutPressed && !mBackPressed &&
            !mDownPressed && !mSlidePressed)
        {
            ut::Rect area(0.0f, 0.0f, mScreenRect.right, 456.0f);
            bool anyHit = false;
            for (s32 i = 0; i < 4; i++) {
                lbl_801EDFD0[i] = 1;
                switch (UpdateDrag(i, &area)) {
                case 0:
                    if (!StartDrag(i, &area)) {
                    if (IsPointerValid(i)) {
                        f32 y = gCursorY[i][0];
                        bool hit = false;
                        ut::Rect rect(0.0f, 0.0f, 0.0f, 0.0f);
                        if (fn_8003567C(&rect, mUnk16C.x, 123.0f + mUnk224, 1.0f) &&
                            lbl_8020E4A0[i] == 0)
                        {
                            f32 x = gCursorX[i][0];
                            if (x >= rect.left && x < rect.right && y >= rect.top &&
                                y < rect.bottom)
                            {
                                anyHit = true;
                                hit = true;
                            }
                        }
                        if (hit && !mUnk356[i]) {
                            StartRumble(i, 3, 20);
                        }
                        mUnk356[i] = hit;
                        if (y > 83.0f && y < 373.0f && (gTrig[i] & 0x800)) {
                            if (hit) {
                                mUnk354 = lbl_80356CA0;
                                mUnk355 = IsState(&MainScreen::State195A0);
                                ChangeState(&MainScreen::State1C600, NULL);
                                return;
                            }
                            if (location == NULL) {
                                PlaySE(0x3F);
                                ChangeState(&MainScreen::State16960, NULL);
                                return;
                            }
                            if (gCursorX[i][0] > area.right) {
                                OpenGlobe();
                                return;
                            }
                            CloseArticle();
                            return;
                        }
                    } else if (gTrig[i] & 0x800) {
                        CloseArticle();
                        return;
                    }
                    } else {
                        SetDragRect(i);
                        if (fn_80034F6C(&mUnk1F4)) {
                            PlaySE(0x2B);
                        }
                    }
                    break;
                case 2:
                    fn_80034FD4();
                    fn_800333C4(fn_80035188(lbl_80356970));
                case 1:
                default:
                    if (mDragging[i]) {
                        SetDragRect(i);
                        if (fn_80034F6C(&mUnk1F4)) {
                            PlaySE(0x2B);
                        }
                        lbl_801EDFD0[i] = 6;
                    }
                    break;
                }
            }
            if (anyHit) {
                if (mUnk350 < 8) {
                    if (++mUnk350 == 8) {
                        PlaySE(0x42);
                    }
                }
            } else if (mUnk350 > 0) {
                mUnk350--;
            }
        }

        fn_80033374(gTextScale);
        if (zoomed) {
            fn_800333C4(fn_80035188(lbl_80356970));
        }
        if (mUnk104) {
            (this->*mUnk104)();
        }
        ScrollArticle();
        UpdateScrollBar();
        break;
    }
    }
}

inline void MainScreen::ResetGlobe(Globe* globe) {
    if (globe != NULL) {
        mUnk274 = globe->mHeight;
        mUnk278 = 0.0f;
        mUnk32C = 0;
        globe->mUnk6C = 0.0f;
        globe->mUnk70 = 0.0f;
    }
}

void MainScreen::State1A750(s32* arg) {
    Globe* globe = lbl_8035775C;
    switch (mStateStep) {
    case 0:
        mUnk2BE = true;
        mStateStep++;
        mActiveLayout = mEarthLayout;
        SetSubState(NULL);
        SetInputHook(&MainScreen::Hook1E758);
        mDraw = &MainScreen::DrawGlobe;
        mUnk234 = 0.0f;
        mUnk238 = (f32)GetCursorAreaRight();
        fn_80032A94(1);
        lbl_80357580 = 0;
        ResetGlobe(globe);
        mUnk338 = 0;
        mUnk33C = 0xAAA;
        mUnk280 = mFadeRect.left = mUnk164.x;
        mUnk284 = mFadeRect.right = mUnk238;
        mUnk288 = mFadeRect.top = 0.0f;
        mUnk28C = mFadeRect.bottom = 456.0f;
        mUnk2BF = true;
        mUnk328 = 0;
        mUnk248 = mUnk244 = 1.0f;
        UpdateScrollBar();
        break;
    case -1:
        break;
    default: {
        mUnk16C.x = fn_80034158() ? mUnk164.x + GetSideMargin() : mUnk164.x + GetSideMargin();
        RelatedItem* item = mRelated;
        math::VEC2 ofs(fn_8000D6A0(item).x - mUnk16C.x, fn_8000D6A0(item).y - (123.0f + mUnk224));
        mUnk244 -= 1.0f / 12.0f;
        if (mUnk244 < 0.0f) {
            mUnk244 = 0.0f;
        }
        mUnk248 = mUnk244;
        mUnk328 += 21;
        if (mUnk328 > 255) {
            mUnk328 = 255;
        }
        mUnk338 += mUnk33C;
        if (mUnk338 > 0x8000) {
            mUnk2BF = false;
            mUnk164.x = mUnk234;
            ChangeState(&MainScreen::State1B134, NULL);
            return;
        }
        f32 t = CosineEase(mUnk338);
        mFadeRect.left = mUnk280 + t * (fn_8000D6A0(item).x - mUnk280);
        mFadeRect.right = mUnk284 + t * (fn_8000D6A0(item).x - mUnk284);
        mFadeRect.top = mUnk288 + t * (fn_8000D6A0(item).y - mUnk288);
        mFadeRect.bottom = mUnk28C + t * (fn_8000D6A0(item).y - mUnk28C);
        UpdateScrollBar();
        break;
    }
    }
}

void MainScreen::State1AC60(s32* arg) {
    Globe* globe = lbl_8035775C;
    switch (mStateStep) {
    case 0:
        mStateStep++;
        mActiveLayout = mEarthLayout;
        SetSubState(NULL);
        SetInputHook(&MainScreen::Hook1E758);
        mDraw = &MainScreen::DrawGlobe;
        f32 w = GetScreenWidth();
        mUnk238 = 0.0f;
        mUnk234 = -w;
        fn_80032A94(1);
        lbl_80357580 = 0;
        ResetGlobe(globe);
        mUnk338 = 0;
        mUnk33C = 0x200;
        mUnk280 = mUnk164.x;
        mUnk284 = mUnk234 - mUnk164.x;
        mUnk290 = mScreenRect.right;
        mUnk294 = mUnk238 - mScreenRect.right;
        UpdateScrollBar();
        break;
    case -1:
        break;
    default: {
        mUnk16C.x = fn_80034158() ? mUnk164.x + GetSideMargin() : mUnk164.x + GetSideMargin();
        mUnk338 += mUnk33C;
        if (mUnk338 > 0x8000) {
            mUnk164.x = mUnk234;
            mScreenRect.right = mUnk238;
            fn_80033374(gTextScale);
            ScrollArticle();
            ChangeState(&MainScreen::State1B134, NULL);
            return;
        }
        f32 t = CosineEase(mUnk338);
        mUnk164.x = mUnk280 + mUnk284 * t;
        mScreenRect.right = mUnk290 + mUnk294 * t;
        if (globe != NULL) {
            globe->mHeight = mUnk288 + mUnk28C * t;
        }
        fn_80033374(gTextScale);
        ScrollArticle();
        UpdateScrollBar();
        break;
    }
    }
}

void MainScreen::State1B134(s32* arg) {
    Globe* globe = lbl_8035775C;
    switch (mStateStep) {
    case -1:
        mUnk128 = NULL;
        fn_8004D2E8(globe);
        lbl_801EDFD0[0] = 1;
        lbl_801EDFD0[1] = 1;
        lbl_801EDFD0[2] = 1;
        lbl_801EDFD0[3] = 1;
        mPrevState = mState;
        break;
    case 0:
        mStateStep++;
        mActiveLayout = mEarthLayout;
        SetGlobeFunc(&MainScreen::Globe1DFD0);
        SetSubState(NULL);
        SetInputHook(&MainScreen::Hook1E758);
        mDraw = &MainScreen::DrawGlobe;
        mUnk128 = &MainScreen::Globe1E2BC;
        fn_80032A94(1);
        if (arg == NULL) {
            fn_80030650();
        }
        UpdateScrollBar();
        break;
    default:
        fn_80032A94(1);
        for (s32 i = 0; i < 4; i++) {
            if (!IsPointerValid(i) && (gTrig[i] & 0x800)) {
                mBackPressed = true;
                break;
            }
        }
        if (mBackPressed) {
            PlaySE(0x22);
            ExitGlobe(FALSE);
            return;
        }
        if (mZoomInPressed) {
            globe->mZoomIn = true;
        } else if (mZoomOutPressed) {
            globe->mZoomOut = true;
        } else if (mRotAPressed) {
            globe->mRotA = true;
        } else if (mRotBPressed) {
            globe->mRotB = true;
        } else if (mResetPressed) {
            PlaySE(0x13);
            fn_8004DA8C(globe, 5, 1);
        }
        for (s32 i = 0; i < 4; i++) {
            f32 y = gCursorY[i][0];
            if (fn_80032BE0(i)) {
                lbl_801EDFD0[i] = 1;
            } else if (globe->mGrab[i]) {
                lbl_801EDFD0[i] = 2;
            } else if (y <= 63.0f || y > 393.0f) {
                lbl_801EDFD0[i] = 1;
            } else {
                lbl_801EDFD0[i] = 3;
            }
        }
        UpdateScrollBar();
        break;
    }
}

static inline f32 GetFontScale() {
    if (gLargeFont) {
        if (gLanguage == 0) {
            return 1.2f;
        } else {
            return 1.0f;
        }
    } else {
        return gDefaultFontScale;
    }
}

void MainScreen::State1B694(s32* arg) {
    Globe* globe = lbl_8035775C;
    switch (mStateStep) {
    case -1:
        mUnk128 = NULL;
        lbl_801EDFD0[0] = 1;
        lbl_801EDFD0[1] = 1;
        lbl_801EDFD0[2] = 1;
        lbl_801EDFD0[3] = 1;
        mShowRelated = false;
        lbl_803575BC = false;
        lbl_803575A8 = 0;
        lbl_803575B9 = true;
        break;
    case 0: {
        mStateStep++;
        lbl_803575BC = true;
        lbl_803575A8 = 1;
        lbl_803575B9 = false;
        for (s32 i = 0; i < 4; i++) {
            mUnk314[i] = mUnk304[i] = -1;
        }
        ResetGlobe(globe);
        mActiveLayout = mMainLayout;
        SetGlobeFunc(&MainScreen::Globe1DF3C);
        SetSubState(NULL);
        SetInputHook(&MainScreen::Hook1ED20);
        mDraw = &MainScreen::DrawGlobe;
        fn_80032A94(1);
        mUnk250 = GetFontScale() * fn_80035188(lbl_80356970);
        f32 split;
        f32 maxHeight = 290.0f;
        f32 width = GetContentRight() - GetSideMargin();
        f32 height = 10.0f + (67.0f + mUnk2F8 * (mUnk24C * mUnk250));
        if (height > maxHeight) {
            height = maxHeight;
        }
        mUnk19C = 0.5f * GetScreenWidth();
        mUnk1A0 = sPopupY;
        mUnk1A4 = width;
        mUnk1A8 = height;
        mUnk1AC = width;
        mUnk1B0 = height - 67.0f;
        mUnk260 = split = 0.3f * width;
        mUnk264 = width - split;
        mShowRelated = true;
        f32 rowHeight = mUnk24C * GetFontScale() * fn_80035188(lbl_80356970);
        mUnk2FC = mUnk300;
        s32 max = mUnk2F8 - sVisibleRows[lbl_80356970];
        if (mUnk2FC >= max) {
            mUnk2FC = max - 1;
            if (mUnk2FC < 0) {
                mUnk2FC = 0;
            }
        }
        mUnk254 = mUnk258 = -(rowHeight * mUnk2FC);
        mUnk25C = 0.0f;
        lbl_803575FC = sShadowColor;
        lbl_803575FC.a = sShadowColor.a * mUnk25C;
        LayoutRelated();
        UpdateScrollBar();
        break;
    }
    default:
        mUnk244 -= 1.0f / 12.0f;
        if (mUnk244 < 0.0f) {
            mUnk244 = 0.0f;
        }
        mUnk248 = mUnk244;
        f32 fs = GetFontScale();
        mUnk250 = fs * gTextScale;
        lbl_803575A8 = 1;
        mUnk25C += 0.1f;
        if (mUnk25C >= 1.0f) {
            mUnk25C = 1.0f;
            lbl_803575FC.a = sShadowColor.a * mUnk25C;
            ChangeState(&MainScreen::State1BD60, NULL);
            return;
        }
        lbl_803575FC.a = sShadowColor.a * mUnk25C;
        UpdateScrollBar();
        break;
    }
}

inline void MainScreen::SetFunc140(Func func) {
    if (mUnk140) {
        mUnk2DC = -1;
        (this->*mUnk140)();
    }
    mUnk2DC = 0;
    mUnk140 = func;
    if (mUnk140) {
        (this->*mUnk140)();
    }
}

inline RelatedItem* MainScreen::GetRelated(s32 index) {
    s32 i = 0;
    for (RelatedItem* item = mRelated; item != NULL; item = item->mNext, i++) {
        if (index == i) {
            return item;
        }
    }
    return NULL;
}

void MainScreen::State1BD60(s32* arg) {
    switch (mStateStep) {
    case -1:
        mUnk128 = NULL;
        lbl_803575BC = false;
        lbl_803575A8 = 0;
        lbl_803575B9 = true;
        lbl_801EDFD0[0] = 1;
        lbl_801EDFD0[1] = 1;
        lbl_801EDFD0[2] = 1;
        lbl_801EDFD0[3] = 1;
        lbl_803575BA = false;
        lbl_803575BB = false;
        mShowRelated = false;
        mUnk25C = 0.0f;
        lbl_803575FC.a = 0;
        mUnk300 = mUnk2FC;
        break;
    case 0:
        mStateStep++;
        lbl_803575BC = true;
        lbl_803575A8 = 1;
        lbl_803575B9 = false;
        mActiveLayout = mMainLayout;
        SetGlobeFunc(&MainScreen::Globe1DF3C);
        SetSubState(NULL);
        SetInputHook(&MainScreen::Hook1ED20);
        mDraw = &MainScreen::DrawGlobe;
        mShowRelated = true;
        fn_80032A94(1);
        LayoutRelated();
        SetFunc140(&MainScreen::Func1C7F4);
        UpdateScrollBar();
        break;
    default: {
        mUnk244 -= 1.0f / 12.0f;
        if (mUnk244 < 0.0f) {
            mUnk244 = 0.0f;
        }
        mUnk248 = mUnk244;
        lbl_803575A8 = 1;
        mUnk2C0 = fn_8003519C(mZoomInPressed, mZoomOutPressed);
        f32 fontScale = GetFontScale();
        mUnk250 = fontScale * gTextScale;
        f32 maxHeight = 290.0f;
        f32 height = 10.0f + (67.0f + mUnk2F8 * (mUnk24C * mUnk250));
        if (height > maxHeight) {
            height = maxHeight;
        }
        mUnk1A8 = height;
        mUnk1B0 = height - 67.0f;
        UpdateRelatedScroll();
        switch (mStateStep) {
        case 1: {
            for (s32 i = 0; i < 4; i++) {
                if (!IsPointerValid(i) && (gTrig[i] & 0x800)) {
                    mBackPressed = true;
                    break;
                }
            }
            if (mBackPressed) {
                mStateStep++;
                PlaySE(0x44);
                return;
            }
            f32 top = 5.0f + (67.0f + mUnk1A0);
            f32 minY = 150.0f;
            f32 left = 5.0f + (mUnk19C - 0.5f * mUnk1A4);
            f32 right = (mUnk19C + 0.5f * mUnk1A4) - 5.0f;
            f32 bottom = (top + mUnk1B0) - 5.0f;
            f32 rowHeight = mUnk24C * mUnk250;
            f32 maxY = 223.0f + minY;
            s32 sel = -1;
            for (s32 i = 0; i < 4; i++) {
                mUnk314[i] = mUnk304[i];
                mUnk304[i] = -1;
            }
            for (s32 i = 0; i < 4; i++) {
                if (IsPointerValid(i)) {
                    f32 x = gCursorX[i][0];
                    f32 y = gCursorY[i][0];
                    if (y > minY && y < maxY && x > left && x < right && y > top && y < bottom) {
                        s32 index = mUnk2FC + (s32)((y - top) / rowHeight);
                        if (index < mUnk2F8) {
                            mUnk304[i] = index;
                            if (index != mUnk314[i]) {
                                PlaySE(0x27);
                            }
                            if (gTrig[i] & 0x800) {
                                sel = i;
                                break;
                            }
                        }
                    }
                }
            }
            UpdateRelatedButtons();
            if (sel >= 0 && OpenRelated(GetRelated(mUnk304[sel]), NULL)) {
                return;
            }
            if (mUnk140) {
                (this->*mUnk140)();
            }
            break;
        }
        case 2:
            mUnk25C -= 0.1f;
            if (mUnk25C <= 0.0f) {
                s32 dir = 0;
                mUnk25C = 0.0f;
                ChangeState(&MainScreen::State1B134, &dir);
                return;
            }
            lbl_803575FC.a = sShadowColor.a * mUnk25C;
            break;
        }
        UpdateScrollBar();
        break;
    }
    }
}

void MainScreen::State1C600(s32* arg) {
    switch (mStateStep) {
    case 0:
        PlaySE(0x40);
        lbl_80356CA0 = true;
        lbl_803575BD = true;
        lbl_803575A8 = 1;
        fn_80032A94(1);
        fn_80032B04(mUnk2C4, mSelected, 1);
        SetSubState(NULL);
        mUnk34C = 0;
        mStateStep = 1;
        break;
    case 1:
        if (mUnk34C < 15) {
            mUnk34C++;
        }
        if (gTrigAll & 0x800) {
            PlaySE(0x41);
            bool show = mUnk354;
            lbl_80356CA0 = gUpdateMsgType == 1 ? true : show;
            mStateStep = 2;
        }
        break;
    case 2:
        if (mUnk34C > 0) {
            mUnk34C--;
        }
        if (mUnk34C == 0) {
            if (mUnk355) {
                ChangeState(&MainScreen::State195A0, NULL);
            } else {
                ChangeState(&MainScreen::State195B8, NULL);
            }
        }
        break;
    case -1:
        break;
    }
}

void MainScreen::Func1C7F4() {
    switch (mUnk2DC) {
    case 0:
        mUnk2DC++;
        break;
    case -1:
        break;
    default: {
        f32 rowHeight = mUnk24C * GetFontScale() * fn_80035188(lbl_80356970);
        s32 visible = sVisibleRows[lbl_80356970];
        if (mUnk2F8 <= visible) {
            mUnk2FC = 0;
            mUnk258 = 0.0f;
        } else {
            s32 max = mUnk2F8 - visible;
            if (mUnk2FC > max) {
                mUnk2FC = max;
                mUnk258 = -(rowHeight * max);
            } else {
                for (s32 i = 0; i < 4; i++) {
                    if (mHeld[i]) {
                        SetFunc140(&MainScreen::Func1CAC8);
                        return;
                    }
                }
                if (mUpPressed && mUnk2FC > 0) {
                    PlaySE(0x25);
                    mUnk2FC -= sScrollRows[lbl_80356970];
                    if (mUnk2FC < 0) {
                        mUnk2FC = 0;
                    }
                } else if (mDownPressed && mUnk2FC < max) {
                    PlaySE(0x25);
                    mUnk2FC += sScrollRows[lbl_80356970];
                    if (mUnk2FC > mUnk2F8 - sVisibleRows[lbl_80356970]) {
                        mUnk2FC = mUnk2F8 - sVisibleRows[lbl_80356970];
                    }
                }
                mUnk258 = -(rowHeight * mUnk2FC);
            }
        }
        if (mUnk2C0) {
            mUnk254 = mUnk258;
            LayoutRelated();
        } else {
            Ease(&mUnk254, mUnk258, 0.2f, 100.0f, 1.0f);
        }
        break;
    }
    }
}

void MainScreen::Func1CAC8() {
    f32 rowHeight = mUnk24C * mUnk250;
    bool held = false;
    switch (mUnk2DC) {
    case -1:
        lbl_801EDFD0[0] = 1;
        lbl_801EDFD0[1] = 1;
        lbl_801EDFD0[2] = 1;
        lbl_801EDFD0[3] = 1;
        lbl_803575BA = false;
        lbl_803575BB = false;
        break;
    case 0:
        mUnk2DC++;
        f32 x = lbl_803575D4;
        f32 y = lbl_803575D8;
        lbl_801EDFA0[1] = x;
        lbl_801EDFB8[1] = y;
        lbl_80357600.a = 255;
        PlaySE(0x16);
        break;
    default: {
        s32 visible = sVisibleRows[lbl_80356970];
        if (mUnk2F8 <= visible) {
            mUnk2FC = 0;
            mUnk258 = 0.0f;
            SetFunc140(&MainScreen::Func1C7F4);
            return;
        }
        s32 max = mUnk2F8 - visible;
        if (mUnk2FC > max) {
            mUnk2FC = max;
            mUnk258 = -(rowHeight * max);
            SetFunc140(&MainScreen::Func1C7F4);
            return;
        }
        if (mHeld[0]) {
            held = true;
        } else if (mHeld[1]) {
            held = true;
        } else if (mHeld[2]) {
            held = true;
        } else if (mHeld[3]) {
            held = true;
        }
        if (!held) {
            mUnk2FC = -(s32)(mUnk254 / rowHeight);
            if (mUnk27C < 0.0f) {
                mUnk2FC++;
            }
            mUnk258 = -(rowHeight * mUnk2FC);
            SetFunc140(&MainScreen::Func1C7F4);
            return;
        }
        f32 velocity = 0.0f;
        s32 count = 0;
        for (s32 i = 0; i < 4; i++) {
            lbl_801EDFD0[i] = 1;
            if (mHeld[i]) {
                count++;
                lbl_801EDFD0[i] = 5;
                velocity += 0.1f * lbl_8020E468[i];
            }
        }
        if (count != 0) {
            mUnk27C = velocity / count;
        }
        f32 min = -(rowHeight * (mUnk2F8 - sVisibleRows[lbl_80356970]));
        mUnk254 += mUnk27C;
        if (mUnk254 > 0.0f) {
            mUnk254 = 0.0f;
        } else if (mUnk254 < min) {
            mUnk254 = min;
        }
        mUnk2FC = -(s32)(mUnk254 / rowHeight);
        if (mUnk254 >= 0.0f) {
            lbl_803575BA = false;
            DisableButton(mUpButton);
        } else {
            lbl_803575BA = true;
            EnableButton(mUpButton);
        }
        if (mUnk254 <= min) {
            lbl_803575BB = false;
            DisableButton(mDownButton);
        } else {
            lbl_803575BB = true;
            EnableButton(mDownButton);
        }
        break;
    }
    }
}

BOOL MainScreen::OpenRelated(RelatedItem* item, s32* arg) {
    if (item != NULL) {
        mUnk2C4 = item->mSection;
        mSelected = item->mIndex;
        mCurRelated = item;
    }
    lbl_8035755C = mLists[mUnk2C4];
    HeadlineList* list = lbl_8035755C;
    if (list != NULL) {
        Globe* globe = lbl_8035775C;
        Ticker* ticker = GetListItem(list, mSelected);
        if (ticker != NULL) {
            NewsArticle* article = ticker->mArticle;
            math::VEC2 thumbPos = ticker->GetThumbPos();
            SetLocation(article);
            globe->mUnk6C = sGlobeX;
            globe->mUnk70 = sGlobeY;
            mUnk234 = 0.0f;
            mUnk238 = (f32)GetCursorAreaRight();
            mUnk16C.x = mUnk164.x + GetSideMargin();
            math::VEC2 origin;
            math::VEC2 size;
            f32 h = 330.0f;
            size.x = mUnk238 - GetSideMargin() - 5.0f;
            size.y = h;
            ticker->GetOrigin(origin);
            mUnk224 = 0.0f;
            mUnk228 = 0.0f;
            NewsTexture* texture = ticker->mArticle->GetTexture();
            BOOL hasLocation = article->mLocationName != NULL;
            fn_8003300C(article, list->mCategory->mName, texture, &origin, &thumbPos,
                        ticker->GetThumbScale(), &size, 0, hasLocation, gTextScale);
            fn_80033374(gTextScale);
            fn_800333C4(gTextScale);
            fn_80033538();
            ScrollArticle();
            u8 flags = article->mFlags;
            if ((flags & 2) && !(flags & 1)) {
                PlaySE(0x3B);
            } else {
                PlaySE(0x3A);
            }
            ChangeState(&MainScreen::State17E6C, arg);
            return TRUE;
        }
    }
    return FALSE;
}

void MainScreen::Sub1D338() {
    switch (mUnk2D4) {
    case 0:
        mUnk2D4++;
        break;
    case -1:
        break;
    default:
        if (!fn_80034598()) {
            for (s32 i = 0; i < 4; i++) {
                if (mHeld[i]) {
                    SetSubState(&MainScreen::Sub1D594);
                    return;
                }
            }
        }
        if (fn_80034770()) {
            DisableButton(fn_80048364(mMainLayout, "up"));
        } else {
            EnableButton(fn_80048364(mMainLayout, "up"));
        }
        if (fn_80034780()) {
            DisableButton(fn_80048364(mMainLayout, "down"));
        } else {
            EnableButton(fn_80048364(mMainLayout, "down"));
        }
        if (mDownPressed && !fn_80034780()) {
            PlaySE(0x25);
            fn_80034690(lbl_80356970, &mUnk224);
            mUnk2B8 = true;
        } else if (mUpPressed && !fn_80034770()) {
            PlaySE(0x25);
            fn_800345E4(lbl_80356970, &mUnk224);
            mUnk2B8 = true;
        }
        mUnk228 = fn_80034164();
        if (IsNearlyZero(gTextScale - lbl_801922D0[lbl_80356970])) {
            Ease(&mUnk224, mUnk228, 0.2f, 20.0f, 1.0f);
        } else {
            mUnk224 = mUnk228;
        }
        break;
    }
}

void MainScreen::Sub1D594() {
    bool held = false;
    switch (mUnk2D4) {
    case -1:
        lbl_801EDFD0[0] = 1;
        lbl_801EDFD0[1] = 1;
        lbl_801EDFD0[2] = 1;
        lbl_801EDFD0[3] = 1;
        lbl_803575BA = false;
        lbl_803575BB = false;
        mUnk228 = fn_80034D34(mUnk224, mUnk22C);
        break;
    case 0:
        mUnk2D4++;
        lbl_801EDFA0[1] = gWidescreen ? 19 : 34;
        lbl_801EDFB8[1] = (456 - (gWidescreen ? 19 : 34)) - lbl_803575D0;
        lbl_80357600.a = 100;
        PlaySE(0x16);
        break;
    default: {
        if (fn_80034598()) {
            SetSubState(&MainScreen::Sub1D338);
            return;
        }
        if (mHeld[0]) {
            held = true;
        } else if (mHeld[1]) {
            held = true;
        } else if (mHeld[2]) {
            held = true;
        } else if (mHeld[3]) {
            held = true;
        }
        if (!held) {
            SetSubState(&MainScreen::Sub1D338);
            return;
        }
        f32 velocity = 0.0f;
        s32 count = 0;
        for (s32 i = 0; i < 4; i++) {
            lbl_801EDFD0[i] = 1;
            if (gHold[i] & 0x400) {
                count++;
                lbl_801EDFD0[i] = 5;
                velocity += 0.1f * lbl_8020E468[i];
            }
        }
        if (count != 0) {
            mUnk22C = velocity / count;
        }
        f32 min = fn_80034844();
        mUnk224 += mUnk22C;
        if (mUnk224 > 0.0f) {
            mUnk224 = 0.0f;
        } else if (mUnk224 < min) {
            mUnk224 = min;
        }
        if (mUnk224 >= 0.0f) {
            lbl_803575BA = false;
            DisableButton(mUpButton);
        } else {
            lbl_803575BA = true;
            EnableButton(mUpButton);
        }
        if (mUnk224 <= min) {
            lbl_803575BB = false;
            DisableButton(mDownButton);
        } else {
            lbl_803575BB = true;
            EnableButton(mDownButton);
        }
        break;
    }
    }
}

void MainScreen::Sub1D9DC() {
    bool held = false;
    switch (mUnk2D4) {
    case 0:
        mUnk2D4++;
        if (fn_800324A0()) {
            DisableButton(mHeadDownButton);
        } else {
            EnableButton(mHeadDownButton);
        }
        if (fn_80032478()) {
            DisableButton(mHeadUpButton);
        } else {
            EnableButton(mHeadUpButton);
        }
        break;
    case -1:
        break;
    default: {
        HeadlineList* list = lbl_8035755C;
        if (list == NULL) {
            break;
        }
        if (list->mMode == HeadlineList::MODE_SECTION && list->mNumItems == 0) {
            break;
        }
        if (mHeld[0]) {
            held = true;
        } else if (mHeld[1]) {
            held = true;
        } else if (mHeld[2]) {
            held = true;
        } else if (mHeld[3]) {
            held = true;
        }
        if (held) {
            SetSubState(&MainScreen::Sub1DC30);
            return;
        }
        if (mDownPressed && !fn_800324A0()) {
            PlaySE(0x25);
            fn_80032450();
        } else if (mUpPressed && !fn_80032478()) {
            PlaySE(0x25);
            fn_8003243C();
        }
        if (fn_800324A0()) {
            DisableButton(mHeadDownButton);
        } else {
            EnableButton(mHeadDownButton);
        }
        if (fn_80032478()) {
            DisableButton(mHeadUpButton);
        } else {
            EnableButton(mHeadUpButton);
        }
        break;
    }
    }
}

void MainScreen::Sub1DC30() {
    bool held = false;
    switch (mUnk2D4) {
    case -1:
        fn_80032508();
        lbl_801EDFD0[0] = 1;
        lbl_801EDFD0[1] = 1;
        lbl_801EDFD0[2] = 1;
        lbl_801EDFD0[3] = 1;
        lbl_803575BA = false;
        lbl_803575BB = false;
        break;
    case 0:
        mUnk2D4++;
        f32 x = lbl_803575D4;
        f32 y = lbl_803575D8;
        lbl_801EDFA0[1] = x;
        lbl_801EDFB8[1] = y;
        lbl_80357600.a = 100;
        PlaySE(0x16);
        break;
    default: {
        if (mHeld[0]) {
            held = true;
        } else if (mHeld[1]) {
            held = true;
        } else if (mHeld[2]) {
            held = true;
        } else if (mHeld[3]) {
            held = true;
        }
        if (!held) {
            SetSubState(&MainScreen::Sub1D9DC);
            return;
        }
        f32 velocity = 0.0f;
        s32 count = 0;
        for (s32 i = 0; i < 4; i++) {
            lbl_801EDFD0[i] = 1;
            if (gHold[i] & 0x400) {
                count++;
                lbl_801EDFD0[i] = 5;
                velocity += 0.1f * lbl_8020E468[i];
            }
        }
        if (count != 0) {
            fn_800324F4(velocity / count);
        }
        if (fn_800324A0()) {
            DisableButton(fn_80048364(mHeadLayout, "down"));
            lbl_803575BB = false;
        } else {
            EnableButton(fn_80048364(mHeadLayout, "down"));
            lbl_803575BB = true;
        }
        if (fn_80032478()) {
            DisableButton(fn_80048364(mHeadLayout, "up"));
            lbl_803575BA = false;
        } else {
            EnableButton(fn_80048364(mHeadLayout, "up"));
            lbl_803575BA = true;
        }
        break;
    }
    }
}

void MainScreen::Globe1DF3C() {
    Globe* globe = lbl_8035775C;
    switch (mUnk2D8) {
    case 0:
        mUnk2D8++;
    default:
        fn_8004D1D4(globe, lbl_80192370);
        fn_8004DB80(globe, 0, lbl_80192398);
        fn_8004DD8C(globe, 0);
        fn_8004CBE0(globe);
        fn_8004CE00(globe);
        fn_8004CC20(globe);
        break;
    case -1:
        break;
    }
}

void MainScreen::Globe1DFD0() {
    Globe* globe = lbl_8035775C;
    switch (mUnk2D8) {
    case 0:
        mUnk2D8++;
    default:
        fn_8004D1D4(globe, lbl_80192370);
        UpdateGlobeInput();
        fn_8004DB80(globe, 1, lbl_80192398);
        fn_8004DD8C(globe, 0);
        fn_8004CBE0(globe);
        fn_8004CE00(globe);
        GlobeCamera* camera = globe->mCamera;
        globe->mX = camera->mX;
        globe->mY = camera->mY;
        fn_8004CC20(globe);
        break;
    case -1:
        break;
    }
}

static inline f32 GetGlobeDistance(Globe* globe) {
    return globe->mCamera != NULL ? globe->mCamera->mDistance : 0.0f;
}

void MainScreen::UpdateGlobeInput() {
    Globe* globe = lbl_8035775C;
    f32 maxY = 393.0f;
    f32 prevDistance = GetGlobeDistance(globe);
    bool moving = false;
    for (s32 i = 0; i < 4; i++) {
        switch (fn_8004D628(globe, i)) {
        case 0:
            if (IsState(&MainScreen::State1B134)) {
                if (gCursorY[i][0] > 63.0f && gCursorY[i][0] < maxY && fn_8004D300(globe, i)) {
                    mSoundId = 0x15;
                }
            }
            break;
        case 2:
            fn_8004E0E8(globe, 20);
        case 1:
            moving = true;
            break;
        }
    }
    if (!moving) {
        mUnk268 = 0.0f;
    }
    f32 delta = __fabsf(GetGlobeDistance(lbl_8035775C) - prevDistance);
    if (!IsNearlyZero(delta) && IsNearlyZero(mUnk268)) {
        f32 s = delta > 2.0f ? 2.0f : delta;
        s *= 0.5f;
        fn_8004F8E0(0x12, 0.5f + 0.5f * s, 1.0f + s, 0.0f);
    }
    mUnk268 += delta;
    if (mUnk268 > 10.0f) {
        mUnk268 = 0.0f;
    }
}

void MainScreen::Globe1E2BC() {
    mRelated = fn_80032B60(&mUnk2C4, &mSelected);
    RelatedItem* item = mRelated;
    if (item != NULL) {
        if (item->mNext != NULL) {
            mUnk2F8 = 0;
            for (; item != NULL; item = item->mNext) {
                mUnk2F8++;
            }
            PlaySE(0x43);
            mSoundId = -1;
            mUnk300 = 0;
            ChangeState(&MainScreen::State1B694, NULL);
        } else {
            mCurRelated = item;
            lbl_8035755C = mLists[mUnk2C4];
            if (ExitGlobe(TRUE)) {
                Ticker* ticker = GetListItem(lbl_8035755C, mSelected);
                if (ticker != NULL) {
                    u8 flags = ticker->mArticle->mFlags;
                    if ((flags & 2) && !(flags & 1)) {
                        PlaySE(0x3B);
                    } else {
                        PlaySE(0x3A);
                    }
                    mSoundId = -1;
                }
            }
        }
    }
}

BOOL MainScreen::ExitGlobe(BOOL related) {
    if (!related) {
        mUnk2C4 = mUnk2E4;
        mSelected = mUnk2E8;
        lbl_8035755C = mLists[mUnk2C4];
    }
    HeadlineList* list = lbl_8035755C;
    if (list != NULL) {
        Globe* globe = lbl_8035775C;
        Ticker* ticker = GetListItem(list, mSelected);
        if (ticker != NULL) {
            NewsArticle* article = ticker->mArticle;
            s32 dir = 0;
            math::VEC2 thumbPos = ticker->GetThumbPos();
            SetLocation(article);
            globe->mUnk6C = sGlobe2X;
            globe->mUnk70 = sGlobe2Y;
            if (!related) {
                f32 w = GetScreenWidth();
                mScreenRect.right = 0.0f;
                mUnk164.x = -w;
            }
            mUnk234 = 0.0f;
            mUnk238 = (f32)GetCursorAreaRight();
            mUnk16C.x = mUnk164.x + GetSideMargin();
            math::VEC2 origin;
            math::VEC2 size;
            f32 h = 330.0f;
            size.x = mUnk238 - GetSideMargin() - 5.0f;
            size.y = h;
            ticker->GetOrigin(origin);
            mUnk224 = 0.0f;
            mUnk228 = 0.0f;
            NewsTexture* texture = ticker->mArticle->GetTexture();
            BOOL hasLocation = article->mLocationName != NULL;
            fn_8003300C(article, list->mCategory->mName, texture, &origin, &thumbPos,
                        ticker->GetThumbScale(), &size, 0, hasLocation, gTextScale);
            fn_80033374(gTextScale);
            fn_800333C4(gTextScale);
            fn_80033538();
            ScrollArticle();
            if (related) {
                ChangeState(&MainScreen::State17E6C, NULL);
            } else {
                ChangeState(&MainScreen::State18770, &dir);
            }
            return TRUE;
        }
    }
    return FALSE;
}

inline void MainScreen::UpdateLayoutAlpha() {
    s32 alpha = 0;
    f32 t = math::SinRad((1.5708f * (15 - mUnk2F4)) / 15.0f);
    alpha += (s32)(255.0f - alpha) * t;
    fn_80048470(mEarthLayout, alpha, mUnk2F4, 15);
    fn_80048470(mMainLayout, alpha, mUnk2F4, 15);
}

void MainScreen::Hook1E758() {
    f32 left = -16.0f;
    f32 right = 16.0f + GetScreenWidth();
    f32 minY = 63.0f;
    f32 maxY = 393.0f;
    Globe* globe = lbl_8035775C;
    f32 minDist = 900.0f;
    bool moved = false;
    bool inside = false;
    bool dragging = false;
    bool held = false;
    bool showButtons = false;
    for (s32 i = 0; i < 4; i++) {
        if (globe != NULL && globe->mGrab[i]) {
            continue;
        }
        if (!IsPointerValid(i)) {
            continue;
        }
        f32 x = gCursorX[i][0];
        f32 y = gCursorY[i][0];
        f32 prevX;
        f32 prevY;
        if (fn_80048854(lbl_8020DE24, i, &prevX, &prevY)) {
            if (x >= left && x < right && prevX >= left && prevX < right &&
                (x - prevX) * (x - prevX) + (y - prevY) * (y - prevY) > minDist)
            {
                moved = true;
            } else if (y <= minY || y > maxY) {
                moved = true;
            }
        }
        if (x >= left && x < right) {
            inside = true;
        }
        if (lbl_8020E4A0[i] != 0) {
            dragging = true;
        }
        bool inArticle = !IsState(&MainScreen::State1B134) && !IsState(&MainScreen::State1AC60);
        if (inArticle && (y < minY || y > maxY)) {
            showButtons = true;
        }
    }
    if (IsState(&MainScreen::State195B8) || IsState(&MainScreen::State195A0)) {
        bool anyHeld = false;
        if (mHeld[0]) {
            anyHeld = true;
        } else if (mHeld[1]) {
            anyHeld = true;
        } else if (mHeld[2]) {
            anyHeld = true;
        } else if (mHeld[3]) {
            anyHeld = true;
        }
        if (anyHeld) {
            moved = false;
            inside = false;
            held = true;
            dragging = false;
        }
    }
    if (inside) {
        mUnk2EC = 0;
    } else if (mUnk2EC < 10) {
        mUnk2EC++;
    }
    if (moved || dragging) {
        mUnk2F0 = 0;
    } else if (mUnk2F0 < 90) {
        mUnk2F0++;
    }
    if (held) {
        fn_80048418(mEarthLayout, 10);
        fn_80048418(mMainLayout, 10);
        showButtons = true;
    } else if (mUnk2EC < 10 && mUnk2F0 < 90) {
        fn_800483EC(mEarthLayout, 15);
        fn_800483EC(mMainLayout, 15);
    } else {
        fn_80048418(mEarthLayout, 30);
        fn_80048418(mMainLayout, 30);
        showButtons = true;
    }
    fn_800483EC(mHeadLayout, 15);
    if (dragging && !mUnk2B8) {
        if (mUnk2F4 > 0) {
            mUnk2F4--;
        }
    } else {
        if (!dragging) {
            mUnk2B8 = false;
        }
        if (mUnk2F4 < 15) {
            mUnk2F4++;
        }
    }
    lbl_80356CA0 = gUpdateMsgType == 1 ? true : showButtons;
    UpdateLayoutAlpha();
}

void MainScreen::Hook1ED20() {
    mUnk2EC = 0;
    mUnk2F0 = 0;
    fn_800483EC(mMainLayout, 15);
    fn_800483EC(mHeadLayout, 15);
    fn_800483EC(mEarthLayout, 15);
    if (mUnk2F4 > 0) {
        mUnk2F4--;
    }
    UpdateLayoutAlpha();
    lbl_80356CA0 = gUpdateMsgType == 1;
}

void MainScreen::LayoutRelated() {
    f32 h = mUnk24C;
    f32 scale = mUnk250;
    f32 rowHeight = h * scale;
    f32 charSpace = scale * gCharSpaceScale;
    RelatedItem* item = mRelated;
    ut::TextWriterBase<wchar_t> writer;
    writer.SetFont(*gSysFont);
    writer.SetCharSpace(charSpace);
    for (; item != NULL; item = item->mNext) {
        writer.SetScale(mUnk250);
        f32 width = fn_8000F73C(item, item->mArticle->mHeadline, gSysFont, mUnk250, charSpace);
        f32 textWidth = 30.0f * mUnk250 + width;
        item->mText.Reset();
        item->mText.mViewWidth =
            item->mArticle->GetTexture() ? (mUnk264 - 20.0f) - rowHeight : mUnk264 - 10.0f;
        item->mText.mContentWidth = textWidth;
        writer.SetScale(0.8f * mUnk250);
        f32 numberWidth = writer.CalcStringWidth(item->mArticle->mLocationName);
        item->mNumber.Reset();
        item->mNumber.mViewWidth = mUnk260 - 10.0f;
        item->mNumber.mContentWidth = numberWidth;
        fn_8000F8A8(item, rowHeight);
    }
}

void MainScreen::UpdateRelatedScroll() {
    f32 h = mUnk24C;
    f32 scale = mUnk250;
    f32 rowHeight = h * scale;
    f32 charSpace = scale * gCharSpaceScale;
    RelatedItem* item = mRelated;
    ut::TextWriterBase<wchar_t> writer;
    writer.SetFont(*gSysFont);
    writer.SetCharSpace(charSpace);
    for (; item != NULL; item = item->mNext) {
        writer.SetScale(mUnk250);
        f32 width = fn_8000F73C(item, item->mArticle->mHeadline, gSysFont, mUnk250, charSpace);
        f32 textWidth = 30.0f * mUnk250 + width;
        item->mText.mViewWidth =
            item->mArticle->GetTexture() ? (mUnk264 - 20.0f) - rowHeight : mUnk264 - 10.0f;
        item->mText.mContentWidth = textWidth;
        writer.SetScale(0.8f * mUnk250);
        f32 numberWidth = writer.CalcStringWidth(item->mArticle->mLocationName);
        item->mNumber.mViewWidth = mUnk260 - 10.0f;
        item->mNumber.mContentWidth = numberWidth;
        fn_8000F8A8(item, rowHeight);
    }
}

inline BOOL MainScreen::IsHovered(s32 index) {
    for (s32 i = 0; i < 4; i++) {
        if (index == mUnk304[i]) {
            return TRUE;
        }
    }
    return FALSE;
}

void MainScreen::UpdateRelatedButtons() {
    RelatedItem* item = mRelated;
    f32 y = mUnk1A0;
    f32 maxY = 456.0f;
    s32 i = 0;
    f32 rowHeight = mUnk24C * mUnk250;
    ut::TextWriterBase<wchar_t> writer;
    y += 5.0f;
    y += mUnk254;
    y += 0.5f * rowHeight;
    writer.SetFont(*gSysFont);
    writer.SetupGX();
    f32 space = gCharSpaceScale;
    writer.SetCharSpace(mUnk250 * space);
    for (; item != NULL; item = item->mNext, i++) {
        item->mText.mActive = IsHovered(i);
        item->mText.Update();
        item->mNumber.mActive = IsHovered(i);
        item->mNumber.Update();
        if (y > 0.0f && y < maxY) {
            writer.SetScale(mUnk250);
            fn_8000EFFC(item, &writer);
            writer.SetScale(0.8f * mUnk250);
            fn_8000F3F0(item, &writer);
        }
        y += rowHeight;
        fn_8000F8A8(item, rowHeight);
    }
}

// END

BOOL MainScreen::StartDrag(s32 chan, const ut::Rect* rect) {
    if (!IsPointerValid(chan)) {
        return FALSE;
    }
    f32 x = gCursorX[chan][0];
    f32 y = gCursorY[chan][0];
    if (x > rect->left && x < rect->right && y > rect->top && y < rect->bottom &&
        !(gHold[chan] & 0x400) && (gTrig[chan] & 0x200))
    {
        mDragging[chan] = true;
        for (s32 i = 0; i < 4; i++) {
            if (i != chan) {
                mDragging[i] = false;
            }
        }
        mDragPos[chan].x = mDragStart[chan].x = gPointerX[chan];
        mDragPos[chan].y = mDragStart[chan].y = gPointerY[chan];
        return TRUE;
    }
    return FALSE;
}

s32 MainScreen::UpdateDrag(s32 chan, const ut::Rect* rect) {
    f32 x = gPointerX[chan];
    f32 y = gPointerY[chan];
    if (mDragging[chan]) {
        if (!(gHoldAll & 0xFDFF) && (gHold[chan] & 0x200) && y > rect->top &&
            y < rect->bottom)
        {
            mDragPos[chan].x = x;
            if (x < rect->left) {
                mDragPos[chan].x = rect->left;
            } else if (mDragPos[chan].x > rect->right) {
                mDragPos[chan].x = rect->right;
            }
            mDragPos[chan].y = y;
            if (y < rect->top) {
                mDragPos[chan].y = rect->top;
            } else if (mDragPos[chan].y > rect->bottom) {
                mDragPos[chan].y = rect->bottom;
            }
            return 1;
        }
        mDragging[chan] = false;
        return 2;
    }
    return 0;
}

void MainScreen::SetLocation(NewsArticle* article) {
    fn_8003256C(&mUnk194, article);
    if (mUnk194.y > 180.0f) {
        mUnk194.y -= 360.0f;
    }
    Globe* globe = lbl_8035775C;
    if (globe != NULL) {
        GlobeCamera* camera = globe->mCamera;
        if (camera != NULL) {
            f32 y = camera->mY;
            f32 x = camera->mX;
            mUnk18C.x = x;
            mUnk18C.y = y;
            if (y > 180.0f) {
                mUnk18C.y -= 360.0f;
            }
            if (__fabsf(mUnk194.x - mUnk18C.x) > 180.0f) {
                mUnk194.x -= 360.0f;
            }
            if (__fabsf(mUnk194.y - mUnk18C.y) > 360.0f) {
                mUnk194.y -= 360.0f;
            }
        }
    }
}

extern "C" void fn_8001F730(lyt::Pane* pane, const ut::Color& color) {
    const char name[100] = "zoom_outT";
    strcat((char*)name, GetLanguageSuffix());
    lyt::Pane* found = pane->FindPaneByName(name, true);
    if (found != NULL) {
        lyt::TextBox* textBox = ut::DynamicCast<lyt::TextBox*>(found);
        ut::Color c = color;
        f32 alpha = lbl_80356C98;
        c.a = c.a * alpha;
        textBox->SetTextColor(c, c);
    }
}
