#ifndef NEWS_LAYOUT_SCREEN_H
#define NEWS_LAYOUT_SCREEN_H

#include <types.h>
#include <nw4r/lyt/lyt_pane.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_Color.h>
#include <nw4r/ut/ut_Rect.h>
#include <nw4r/ut/ut_TagProcessorBase.h>

namespace nw4r {
namespace lyt {
class ArcResourceAccessor;
class Layout;
class DrawInfo;
} // namespace lyt
} // namespace nw4r

class ColorTagProcessor;

extern "C" void fn_800409EC(void* p);

// One button of a LayoutScreen, built from a top-level pane of the layout.
// Child panes are looked up by name suffix: "B" (base), "R" (hit rect),
// "I" (icon), "T" (text), "F0"/"F1" (frames) and "M" (marks).
class LayoutScreenItem {
public:
    LayoutScreenItem(nw4r::lyt::Pane* pane, const nw4r::lyt::DrawInfo* drawInfo,
                   nw4r::ut::TagProcessorBase<wchar_t>* tagProcessor, int noScale);

    ~LayoutScreenItem() {
        if (mUnk88 != NULL) {
            fn_800409EC(mUnk88);
        }
        if (mUnk84 != NULL) {
            fn_800409EC(mUnk84);
        }
    }

    void Reset() {
        mOffsetY = 0.0f;
        mAlpha = 255;
        mFadeAlpha = 255;
        mHover = false;
        mHoverFrame = 0;
        mPressed = false;
        mPressFrame = 0;
        mTextIndex = 0;
        mSelected = false;
    }

    void SetHover();
    void Update();
    void Draw();
    nw4r::lyt::Pane* GetMarkPane(int index);

    nw4r::lyt::Pane* mPane;                 // at 0x00
    const nw4r::lyt::DrawInfo* mDrawInfo;   // at 0x04
    nw4r::lyt::Pane* mBasePane;             // at 0x08
    nw4r::lyt::Pane* mIconPane;             // at 0x0C
    nw4r::lyt::Pane* mTextPane;             // at 0x10
    nw4r::lyt::Pane* mFrame0Pane;           // at 0x14
    nw4r::lyt::Pane* mFrame1Pane;           // at 0x18
    nw4r::lyt::Pane* mMarkPane;             // at 0x1C
    nw4r::ut::Rect mRect;                   // at 0x20
    nw4r::math::VEC3 mBasePos;              // at 0x30
    bool mFixed;                            // at 0x3C
    s32 mColorSet;                          // at 0x40
    bool mUnk44;                            // at 0x44
    bool mUnk45;                            // at 0x45
    bool mToggle;                           // at 0x46
    bool mUnk47;                            // at 0x47
    bool mInactive;                         // at 0x48
    bool mHidden;                           // at 0x49
    bool mDisabled;                         // at 0x4A
    bool mUnk4B;                            // at 0x4B
    LayoutScreenItem* mLinkA;                 // at 0x4C
    LayoutScreenItem* mLinkB;                 // at 0x50
    f32 mOffsetY;                           // at 0x54
    s32 mAlpha;                             // at 0x58
    s32 mFadeAlpha;                         // at 0x5C
    bool mHover;                            // at 0x60
    s32 mHoverFrame;                        // at 0x64
    bool mPressed;                          // at 0x68
    s32 mPressFrame;                        // at 0x6C
    s32 mTextIndex;                         // at 0x70
    bool mSelected;                         // at 0x74
    u8 mPad75[3];                           // at 0x75
    nw4r::ut::Color mTextColor;             // at 0x78
    s32 mMarkIndex;                         // at 0x7C
    s32 mMarkSubIndex;                      // at 0x80
    void* mUnk84;                           // at 0x84
    void* mUnk88;                           // at 0x88
    s32 mUnk8C;                             // at 0x8C
    s32 mUnk90;                             // at 0x90
    s32 mUnk94;                             // at 0x94
};

// A menu screen built from a layout archive: every top-level pane becomes a LayoutScreenItem.
class LayoutScreen {
public:
    LayoutScreen(void* archive, const char* layoutName, int noScale);
    ~LayoutScreen();

    void Reset();

    void Calc();
    void Draw();

    nw4r::lyt::ArcResourceAccessor* mResAccessor; // at 0x000
    nw4r::lyt::Layout* mLayout;                   // at 0x004
    nw4r::lyt::DrawInfo* mDrawInfo;               // at 0x008
    LayoutScreenItem* mItems[64];                   // at 0x00C
    s32 mItemCount;                               // at 0x10C
    ColorTagProcessor* mTagProcessor;             // at 0x110
    bool mFadeOut;                                // at 0x114
    s32 mFadeLength;                              // at 0x118
    s32 mFadeFrame;                               // at 0x11C
    bool mAlphaFadeOut;                           // at 0x120
    s32 mAlphaFadeLength;                         // at 0x124
    s32 mAlphaFadeFrame;                          // at 0x128
};

#endif
