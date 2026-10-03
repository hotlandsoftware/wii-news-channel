#ifndef NEWS_MAIN_SCREEN_H
#define NEWS_MAIN_SCREEN_H

#include <types.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_Rect.h>
#include <nw4r/ut/ut_TextWriterBase.h>
#include <news/HeadlineList.h>
#include <news/TextButton.h>
#include <news/Scroller.h>

struct Layout;
class PaneButton;
class HeadlineList;
class FrameTextButton;
class NewsArticle;

// Item of the list at MainScreen::mRelated (not yet decompiled).
struct RelatedItem {
    u8 unk0[0x28];
    RelatedItem* mNext;       // at 0x28
    RelatedItem* mPrev;       // at 0x2C
    NewsArticle* mArticle;    // at 0x30
    u8 unk34[0x58 - 0x34];
    Scroller mText;           // at 0x58
    Scroller mNumber;         // at 0x88
    f32 mThumbX;              // at 0xB8
    f32 mThumbY;              // at 0xBC
    u8 unkC0[0xC4 - 0xC0];
    f32 mThumbScale;          // at 0xC4
    u8 unkC8[0xDC - 0xC8];
    s32 mSection;             // at 0xDC
    s32 mIndex;               // at 0xE0
};

struct GlobeCamera {
    u8 unk0[0x90];
    f32 mX;         // at 0x90
    f32 mY;         // at 0x94
    u8 unk98[0xA4 - 0x98];
    f32 mDistance;  // at 0xA4
};

// Globe view (not yet decompiled).
struct Globe {
    u32 unk0;
    GlobeCamera* mCamera;   // at 0x04
    f32 mX;                 // at 0x08
    f32 mY;                 // at 0x0C
    u8 unk10[0x6C - 0x10];
    f32 mUnk6C;             // at 0x6C
    f32 mUnk70;             // at 0x70
    f32 mHeight;            // at 0x74
    u8 unk78[0x8C - 0x78];
    bool mGrab[4];          // at 0x8C
    u8 unk90;               // at 0x90
    bool mZoomOut;          // at 0x91
    bool mZoomIn;           // at 0x92
    bool mRotA;             // at 0x93
    bool mRotB;             // at 0x94
    u8 unk95[0x9C - 0x95];
    s32 mZoom;      // at 0x9C
    s32 mRotation;  // at 0xA0
};

extern HeadlineList* lbl_8035755C; // headline list being shown
extern u32 lbl_803575E0;           // number of news sections
extern Globe* lbl_8035775C;

void HeadlineList_Draw(const f32& offsetX, f32 alpha, f32 headerAlpha);

// Base of MainScreen (not yet decompiled, 0x800493A8): the text writer and the
// screen area.
class ScreenBase {
public:
    ScreenBase(nw4r::ut::TextWriterBase<wchar_t>* writer, f32 x, f32 y, f32 width, f32 height);
    ~ScreenBase();

    nw4r::ut::TextWriterBase<wchar_t>* mWriter; // at 0x00
    f32 mX;                                     // at 0x04
    f32 mY;                                     // at 0x08
    f32 mWidth;                                 // at 0x0C
    f32 mHeight;                                // at 0x10
};

// Main screen of the channel: the headline lists of every news section, the
// section buttons, the globe ("earth" layout) and the text zoom.
// Layouts: main.brlyt (headline list), head.brlyt (article view), earth.brlyt (globe).
class MainScreen : public ScreenBase {
public:
    enum {
        MAX_CATEGORIES = 14,
    };

    typedef void (MainScreen::*ModeFunc)();
    typedef void (MainScreen::*StateFunc)(s32* arg);
    typedef void (MainScreen::*Func)();

    MainScreen(u32 arc, nw4r::ut::TextWriterBase<wchar_t>* writer, nw4r::math::VEC2& pos,
               nw4r::math::VEC2& size);
    ~MainScreen();

    void Start();
    void LayoutSectionButtons();
    void Draw();
    void DrawGlobe();
    void DrawRelated();
    void DrawButtons();
    void DrawButtons2();
    void DrawButtonsGlobe();
    void DrawCursor();
    void Update();
    void ResetZoom();
    void SetMode(ModeFunc mode);
    void ModeMain();
    void ModeWait();
    BOOL CheckSelect();
    s32 OpenArticle(s32* arg);
    void ChangeState(StateFunc state, s32* arg);

    void State16960(s32* arg);
    void StateList(s32* arg);
    void State17E6C(s32* arg);
    void State18770(s32* arg);
    void State195A0(s32* arg);
    void State195B8(s32* arg);
    void State1A750(s32* arg);
    void State1AC60(s32* arg);
    void State1B134(s32* arg);
    void State1B694(s32* arg);
    void State1BD60(s32* arg);
    void State1C600(s32* arg);
    void Hook1ED20();

    BOOL IsState(StateFunc state) { return mState == state; }
    bool IsListState() {
        return IsState(&MainScreen::State16960) || IsState(&MainScreen::StateList);
    }


    void SetSubState(Func func);
    void SetInputHook(Func func);
    void ScrollArticle();
    void LayoutArticle();
    void LayoutList();
    void UpdateHeadButtons();
    void ResetListPos();
    void LayoutTicker(const bool* held);
    BOOL CheckSectionButtons();
    void Sub1D9DC();
    void SetGlobeFunc(Func func);
    void UpdateGlobeCamera();
    void UpdateScrollBar();
    void Hook1E758();
    NewsArticle* GetSelectedArticle();
    BOOL CheckArticleSwitch();
    void SetArticleWidth();
    void SetDragRect(s32 chan);
    bool NoButtonPressed();
    void CloseArticle();
    void SetButtonsEnabled(bool enabled);
    void OpenGlobe();
    void OpenSelected();
    void SetListHook();
    void EaseZoom();
    void ReturnToTop();
    BOOL StartDrag(s32 chan, const nw4r::ut::Rect* rect);
    s32 UpdateDrag(s32 chan, const nw4r::ut::Rect* rect);
    BOOL OpenRelated(RelatedItem* item, s32* dir);
    void Sub1D338();
    void ResetGlobe(Globe* globe);
    BOOL ExitGlobe(BOOL related);
    void Globe1DFD0();
    void UpdateGlobeInput();
    void UpdateLayoutAlpha();
    BOOL IsHovered(s32 index);
    void Globe1E2BC();
    void LayoutRelated();
    void SetFunc140(Func func);
    RelatedItem* GetRelated(s32 index);
    void UpdateRelatedScroll();
    void UpdateRelatedButtons();
    void Func1C7F4();
    void Func1CAC8();
    void Sub1D594();
    void Sub1DC30();
    void Globe1DF3C();
    void SetLocation(NewsArticle* article);

    void DrawButtonsInline() {
        HeadlineList_Draw(mUnk164.x, mUnk23C, mUnk240);
        if (lbl_8035755C != NULL && lbl_8035755C->mMode != 2) {
            s32 i;
            s32 count = lbl_803575E0 - 1;
            FrameTextButton** button = &mButtons[1];
            for (i = 0; i < count; i++, button++) {
                (*button)->Draw(mUnk23C);
            }
        }
    }

    void DrawGlobeInline();

public:
    Layout* mMainLayout;            // at 0x014
    Layout* mHeadLayout;            // at 0x018
    Layout* mEarthLayout;           // at 0x01C
    Layout* mActiveLayout;          // at 0x020
    PaneButton* mUpButton;          // at 0x024 (main "up")
    PaneButton* mDownButton;        // at 0x028 (main "down")
    PaneButton* mZoomInButton;      // at 0x02C (main "zoom_in")
    PaneButton* mZoomOutButton;     // at 0x030 (main "zoom_out")
    PaneButton* mHeadBackButton;    // at 0x034
    PaneButton* mHeadUpButton;      // at 0x038
    PaneButton* mHeadDownButton;    // at 0x03C
    PaneButton* mHeadZoomInButton;  // at 0x040
    PaneButton* mHeadZoomOutButton; // at 0x044
    PaneButton* mSlideButton;       // at 0x048
    PaneButton* mTextButton;        // at 0x04C
    PaneButton* mRotAButton;        // at 0x050
    PaneButton* mRotBButton;        // at 0x054
    PaneButton* mEarthZoomOutButton; // at 0x058
    PaneButton* mEarthZoomInButton; // at 0x05C
    PaneButton* mResetButton;       // at 0x060
    PaneButton* mEarthBackButton;   // at 0x064
    HeadlineList* mLists[MAX_CATEGORIES];       // at 0x068
    RelatedItem* mRelated;                      // at 0x0A0
    RelatedItem* mCurRelated;                   // at 0x0A4
    FrameTextButton* mButtons[MAX_CATEGORIES];  // at 0x0A8
    ModeFunc mMode;                 // at 0x0E0
    StateFunc mState;               // at 0x0EC
    StateFunc mPrevState;           // at 0x0F8
    Func mUnk104;                   // at 0x104
    Func mDraw;                     // at 0x110
    Func mUnk11C;                   // at 0x11C
    Func mUnk128;                   // at 0x128
    Func mUnk134;                   // at 0x134
    Func mUnk140;                   // at 0x140
    nw4r::math::VEC2 mUnk14C;       // at 0x14C
    nw4r::math::VEC2 mUnk154;       // at 0x154
    nw4r::math::VEC2 mUnk15C;       // at 0x15C
    nw4r::math::VEC2 mUnk164;       // at 0x164
    nw4r::math::VEC2 mUnk16C;       // at 0x16C
    nw4r::math::VEC2 mListSize;     // at 0x174
    f32 mUnk17C;                    // at 0x17C
    f32 mUnk180;                    // at 0x180
    f32 mUnk184;                    // at 0x184
    f32 mUnk188;                    // at 0x188
    nw4r::math::VEC2 mUnk18C;       // at 0x18C
    nw4r::math::VEC2 mUnk194;       // at 0x194
    f32 mUnk19C;                    // at 0x19C
    f32 mUnk1A0;                    // at 0x1A0
    f32 mUnk1A4;                    // at 0x1A4
    f32 mUnk1A8;                    // at 0x1A8
    f32 mUnk1AC;                    // at 0x1AC
    f32 mUnk1B0;                    // at 0x1B0
    nw4r::math::VEC2 mDragStart[4]; // at 0x1B4
    nw4r::math::VEC2 mDragPos[4];   // at 0x1D4
    nw4r::ut::Rect mUnk1F4;         // at 0x1F4
    nw4r::ut::Rect mFadeRect;       // at 0x204
    nw4r::ut::Rect mScreenRect;     // at 0x214
    f32 mUnk224;                    // at 0x224
    f32 mUnk228;                    // at 0x228
    f32 mUnk22C;                    // at 0x22C
    f32 mUnk230;                    // at 0x230
    f32 mUnk234;                    // at 0x234
    f32 mUnk238;                    // at 0x238
    f32 mUnk23C;                    // at 0x23C
    f32 mUnk240;                    // at 0x240
    f32 mUnk244;                    // at 0x244
    f32 mUnk248;                    // at 0x248
    f32 mUnk24C;                    // at 0x24C
    f32 mUnk250;                    // at 0x250
    f32 mUnk254;                    // at 0x254
    f32 mUnk258;                    // at 0x258
    f32 mUnk25C;                    // at 0x25C
    f32 mUnk260;                    // at 0x260
    f32 mUnk264;                    // at 0x264
    f32 mUnk268;                    // at 0x268
    f32 mUnk26C;                    // at 0x26C
    f32 mUnk270;                    // at 0x270
    f32 mUnk274;                    // at 0x274
    f32 mUnk278;                    // at 0x278
    f32 mUnk27C;                    // at 0x27C
    f32 mUnk280;                    // at 0x280
    f32 mUnk284;                    // at 0x284
    f32 mUnk288;                    // at 0x288
    f32 mUnk28C;                    // at 0x28C
    f32 mUnk290;                    // at 0x290
    f32 mUnk294;                    // at 0x294
    f32 mUnk298;                    // at 0x298
    f32 mUnk29C;                    // at 0x29C
    f32 mUnk2A0;                    // at 0x2A0
    f32 mUnk2A4;                    // at 0x2A4
    bool mZoomOutPressed;           // at 0x2A8
    bool mUpPressed;                // at 0x2A9
    bool mZoomInPressed;            // at 0x2AA
    bool mBackPressed;              // at 0x2AB
    bool mDownPressed;              // at 0x2AC
    bool mSlidePressed;             // at 0x2AD
    bool mEarthPressed;             // at 0x2AE
    bool mRotAPressed;              // at 0x2AF
    bool mRotBPressed;              // at 0x2B0
    bool mResetPressed;             // at 0x2B1
    bool mHeld[4];                  // at 0x2B2
    bool mUnk2B6;                   // at 0x2B6
    bool mUnk2B7;                   // at 0x2B7
    bool mUnk2B8;                   // at 0x2B8
    bool mShowRelated;              // at 0x2B9
    bool mDragging[4];              // at 0x2BA
    bool mUnk2BE;                   // at 0x2BE
    bool mUnk2BF;                   // at 0x2BF
    bool mUnk2C0;                   // at 0x2C0
    s32 mUnk2C4;                    // at 0x2C4
    s32 mUnk2C8;                    // at 0x2C8
    s32 mModeStep;                  // at 0x2CC
    s32 mStateStep;                 // at 0x2D0
    s32 mUnk2D4;                    // at 0x2D4
    s32 mUnk2D8;                    // at 0x2D8
    s32 mUnk2DC;                    // at 0x2DC
    s32 mSelected;                  // at 0x2E0
    s32 mUnk2E4;                    // at 0x2E4
    s32 mUnk2E8;                    // at 0x2E8
    s32 mUnk2EC;                    // at 0x2EC
    s32 mUnk2F0;                    // at 0x2F0
    s32 mUnk2F4;                    // at 0x2F4
    s32 mUnk2F8;                    // at 0x2F8
    s32 mUnk2FC;                    // at 0x2FC
    s32 mUnk300;                    // at 0x300
    s32 mUnk304[4];                 // at 0x304
    s32 mUnk314[4];                 // at 0x314
    s32 mUnk324;                    // at 0x324
    s32 mUnk328;                    // at 0x328
    s32 mUnk32C;                    // at 0x32C
    s32 mUnk330;                    // at 0x330
    s32 mUnk334;                    // at 0x334
    s32 mUnk338;                    // at 0x338
    s32 mUnk33C;                    // at 0x33C
    s32 mUnk340;                    // at 0x340
    s32 mUnk344;                    // at 0x344
    u32 mSoundId;                   // at 0x348
    s32 mUnk34C;                    // at 0x34C
    s32 mUnk350;                    // at 0x350
    bool mUnk354;                   // at 0x354
    bool mUnk355;                   // at 0x355
    bool mUnk356[4];                // at 0x356
};

#endif
