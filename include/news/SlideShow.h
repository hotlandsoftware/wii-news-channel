#ifndef NEWS_SLIDE_SHOW_H
#define NEWS_SLIDE_SHOW_H

#include <types.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_Color.h>
#include <nw4r/ut/ut_Rect.h>
#include <nw4r/ut/ut_TextWriterBase.h>

struct Layout;
class PaneButton;
class NewsArticle;
struct NewsTexture;

// The slide show: shows the articles of the selected category one after
// another (picture or globe, plus the article text), with zoom, scrolling
// and next/previous controls.
class SlideShow {
public:
    typedef BOOL (SlideShow::*StateFunc)(const s32* arg);
    typedef void (SlideShow::*SubStateFunc)();
    typedef void (SlideShow::*DrawFunc)();

    SlideShow(u32 arc);
    ~SlideShow();

    void LoadArticle();
    void Start();
    void Stop();
    void Calc();
    void Draw();
    void CalcArrows();
    void CalcTextPos();
    BOOL CheckInput();

    BOOL StateStop(const s32* arg);
    BOOL StateShow(const s32* arg);
    void StartZoomOut();
    BOOL StateZoom(const s32* arg);
    BOOL StateMove(const s32* arg);
    BOOL StateMessage(const s32* arg);
    BOOL StateEnd(const s32* arg);

    void SubStateIdle();
    void SubStateWait();
    void SubStateScroll();
    void SubStateDrag();

    void DrawPictures();
    void DrawSelection();
    void DrawFooterA();
    void DrawFooterB();
    void DrawCaption(const wchar_t* text, u8 alpha, f32 x, f32 y, f32 width, f32 height);

    void NextArticle();
    void PrevArticle();
    void LayoutArticle();
    void CheckPointer();
    BOOL StartGrab(s32 chan, const nw4r::ut::Rect* rect);
    s32 UpdateGrab(s32 chan, const nw4r::ut::Rect* rect);
    void LayoutTitle();

    bool IsState(StateFunc state) {
        return mState == state;
    }

    void ChangeState(StateFunc state, const s32* arg = NULL) {
        if (mState) {
            mStateFrame = -1;
            (this->*mState)(arg);
        }
        mState = state;
        mStateFrame = 0;
        if (mState) {
            (this->*mState)(arg);
        }
    }

    void ChangeSubState(SubStateFunc state) {
        if (mSubState) {
            mSubStateFrame = -1;
            (this->*mSubState)();
        }
        mSubState = state;
        mSubStateFrame = 0;
        (this->*mSubState)();
    }

    Layout* mCurLayout;                   // at 0x000
    Layout* mMainLayout;                  // at 0x004
    Layout* mSlideLayout;                 // at 0x008
    Layout* mBeltLayout;                  // at 0x00C
    PaneButton* mUpButton;                // at 0x010
    PaneButton* mDownButton;              // at 0x014
    PaneButton* mBackButton;              // at 0x018
    PaneButton* mZoomInButton;            // at 0x01C
    PaneButton* mZoomOutButton;           // at 0x020
    PaneButton* mEndButton;               // at 0x024
    NewsArticle* mArticle;                // at 0x028
    NewsTexture* mPrevPicture;            // at 0x02C
    const wchar_t* mPrevCaption;          // at 0x030
    StateFunc mState;                     // at 0x034
    PC_PMF_PAD(mState)
    SubStateFunc mSubState;               // at 0x040
    PC_PMF_PAD(mSubState)
    DrawFunc mDrawFooter;                 // at 0x04C
    PC_PMF_PAD(mDrawFooter)
    nw4r::ut::TextWriterBase<wchar_t> mWriter; // at 0x058
    u8 mUnkB8[0xF8 - 0xB8];               // at 0x0B8
    nw4r::ut::Color mArrowColors[3];      // at 0x0F8
    nw4r::ut::Rect mView;                 // at 0x104
    nw4r::ut::Rect mText;                 // at 0x114
    nw4r::ut::Rect mPicArea;              // at 0x124
    nw4r::ut::Rect mTitleRect;            // at 0x134
    u8 mUnk144[0x204 - 0x144];            // at 0x144
    nw4r::ut::Rect mUnk204;               // at 0x204
    nw4r::math::VEC2 mUnk214;             // at 0x214
    u8 mUnk21C[0x228 - 0x21C];            // at 0x21C
    nw4r::math::VEC3 mUpArrow[3];         // at 0x228
    nw4r::math::VEC3 mDownArrow[3];       // at 0x24C
    f32 mViewWidth;                       // at 0x270
    f32 mViewHeight;                      // at 0x274
    nw4r::math::VEC2 mGlobeFrom;          // at 0x278
    nw4r::math::VEC2 mGlobeTo;            // at 0x280
    nw4r::math::VEC2 mUnk288;             // at 0x288
    f32 mViewTarget[2];                   // at 0x290
    f32 mTextTarget[2];                   // at 0x298
    f32 mPicCenter[2];                    // at 0x2A0
    f32 mPrevPicCenter[2];                // at 0x2A8
    nw4r::math::VEC2 mGrabStart[4];       // at 0x2B0
    nw4r::math::VEC2 mGrabPos[4];         // at 0x2D0
    f32 mTitleRight;                      // at 0x2F0
    f32 mTitleY;                          // at 0x2F4
    nw4r::ut::Rect mSelect;               // at 0x2F8
    bool mZoomOutPressed;                 // at 0x308
    bool mPrevPressed;                    // at 0x309
    bool mNextPressed;                    // at 0x30A
    bool mZoomInPressed;                  // at 0x30B
    bool mBackPressed;                    // at 0x30C
    bool mMainPressed;                    // at 0x30D
    bool mUnk30E;                         // at 0x30E
    bool mUpPressed;                      // at 0x30F
    bool mDownPressed;                    // at 0x310
    bool mDragging[4];                    // at 0x311
    bool mShowPicture;                    // at 0x315
    bool mZoomed;                         // at 0x316
    bool mTextMoving;                     // at 0x317
    bool mTextVisible;                    // at 0x318
    bool mGrabbing[4];                    // at 0x319
    bool mHasTitle;                       // at 0x31D
    bool mShowMain;                       // at 0x31E
    bool mPlaySound;                      // at 0x31F
    s32 mCategory;                        // at 0x320
    s32 mArticleIdx;                      // at 0x324
    s32 mUnk328;                          // at 0x328
    s32 mUnk32C;                          // at 0x32C
    s32 mUnk330;                          // at 0x330
    s32 mUnk334;                          // at 0x334
    s32 mUnk338;                          // at 0x338
    s32 mUnk33C;                          // at 0x33C
    s32 mTimer;                           // at 0x340
    s32 mPointerOutTimer;                 // at 0x344
    s32 mIdleTimer;                       // at 0x348
    s32 mFooterFade;                      // at 0x34C
    s32 mPicAlpha;                        // at 0x350
    s32 mPrevPicAlpha;                    // at 0x354
    s32 mSlideAngle;                      // at 0x358
    s32 mGlobeAngle;                      // at 0x35C
    s32 mAlpha;                           // at 0x360
    s32 mFooterAlpha;                     // at 0x364
    s32 mZoomAngle;                       // at 0x368
    s32 mZoomSpeed;                       // at 0x36C
    s32 mDirection;                       // at 0x370
    s32 mUnk374;                          // at 0x374
    f32 mUnk378;                          // at 0x378
    f32 mUnk37C;                          // at 0x37C
    f32 mUnk380;                          // at 0x380
    f32 mUnk384;                          // at 0x384
    f32 mUnk388;                          // at 0x388
    f32 mScroll;                          // at 0x38C
    f32 mScrollTarget;                    // at 0x390
    f32 mScrollSpeed;                     // at 0x394
    f32 mPicScale;                        // at 0x398
    f32 mPrevPicScale;                    // at 0x39C
    f32 mSlideDist;                       // at 0x3A0
    f32 mGlobeZoomFrom;                   // at 0x3A4
    f32 mGlobeZoomTo;                     // at 0x3A8
    f32 mGlobeZoom;                       // at 0x3AC
    f32 mTextOfs;                         // at 0x3B0
    f32 mTitleMaxWidth;                   // at 0x3B4
    f32 mTitleScale;                      // at 0x3B8
    f32 mZoomFrom[8];                     // at 0x3BC
    f32 mUnk3DC;                          // at 0x3DC
    f32 mUnk3E0;                          // at 0x3E0
    u16 mUnk3E4;                          // at 0x3E4
    s8 mStateFrame;                       // at 0x3E6
    s8 mSubStateFrame;                    // at 0x3E7
    s32 mMessageFade;                     // at 0x3E8
    s32 mHoldTimer;                       // at 0x3EC
    bool mMessageFlag;                    // at 0x3F0
    bool mPointerIn[4];                   // at 0x3F1
    bool mLoop;                           // at 0x3F5
    s32 mBeltFade;                        // at 0x3F8
    bool mBeltVisible;                    // at 0x3FC
    s32 mSpeed;                           // at 0x400
    s32 mBounceTimer;                     // at 0x404
    bool mQuickMove;                      // at 0x408
    bool mZoomDone;                       // at 0x409
    s32 mTimerFade;                       // at 0x40C
};

#endif
