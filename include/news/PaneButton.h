#ifndef NEWS_PANE_BUTTON_H
#define NEWS_PANE_BUTTON_H

#include <types.h>
#include <nw4r/lyt/lyt_pane.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_Color.h>
#include <nw4r/ut/ut_Rect.h>

namespace nw4r {
namespace ut {
template <typename T> class TagProcessorBase;
}
namespace lyt {
class DrawInfo;
}
} // namespace nw4r

// Colour scheme of a PaneButton (one per mColorSet index).
struct PaneButtonColors {
    nw4r::ut::Color mBaseTop;           // at 0x00
    nw4r::ut::Color mBaseBottom;        // at 0x04
    nw4r::ut::Color mIconTop;           // at 0x08
    nw4r::ut::Color mIconBottom;        // at 0x0C
    nw4r::ut::Color mText;              // at 0x10
    nw4r::ut::Color mBaseTopHover;      // at 0x14
    nw4r::ut::Color mBaseBottomHover;   // at 0x18
    nw4r::ut::Color mIconTopHover;      // at 0x1C
    nw4r::ut::Color mIconBottomHover;   // at 0x20
    nw4r::ut::Color mTextHover;         // at 0x24
    nw4r::ut::Color mBaseTopSelect;     // at 0x28
    nw4r::ut::Color mBaseBottomSelect;  // at 0x2C
    nw4r::ut::Color mIconTopSelect;     // at 0x30
    nw4r::ut::Color mIconBottomSelect;  // at 0x34
    nw4r::ut::Color mTextSelect;        // at 0x38
    u32 mUnk3C;                         // at 0x3C
    u32 mUnk40;                         // at 0x40
    nw4r::ut::Color mIconTopBlend;      // at 0x44
    nw4r::ut::Color mIconBottomBlend;   // at 0x48
    nw4r::ut::Color mTextBlend;         // at 0x4C
};

// A button built from a layout pane. The pane's children are looked up by
// name suffix: "B" (base), "R" (hit rect), "I" (icon), "T" (text),
// "F0"/"F1" (frames).
class PaneButton {
public:
    typedef void (*TextColorCallback)(nw4r::lyt::Pane* pane, const nw4r::ut::Color& color);
    typedef void (*MoveCallback)(void* arg);

    PaneButton(nw4r::lyt::Pane* pane, const nw4r::lyt::DrawInfo* drawInfo,
               PaneButtonColors* colors, nw4r::ut::TagProcessorBase<wchar_t>* tagProcessor);
    ~PaneButton();

    void Reset();
    void UpdateFrame();
    void UpdatePane();
    void Draw();
    bool HitTest(f32 x, f32 y);
    void SetHover();
    void SetPressed(bool pressed);
    void Press();
    void SetSelIndex(s32 idx);
    void SetText(const wchar_t* text);
    void Hide();
    void SetAlpha(s32 alpha);
    nw4r::lyt::Pane* FindPane(const char* name);

    void SetSlide(f32 step) { mOffsetY = step * (mPane->GetTranslate().y > 0.0f ? 1 : -1); }
    void SetBaseAlpha(s32 alpha) { mAlpha = alpha; }

    void SetBlend(s32 alpha, s32 blend, s32 blendMax) {
        mFadeAlpha = alpha;
        mBlend = blend;
        mBlendMax = blendMax;
    }

    s32 mUnk00;                              // at 0x00
    PaneButtonColors* mColors;               // at 0x04
    nw4r::lyt::Pane* mPane;                  // at 0x08
    const nw4r::lyt::DrawInfo* mDrawInfo;    // at 0x0C
    nw4r::lyt::Pane* mBasePane;              // at 0x10
    nw4r::lyt::Pane* mIconPane;              // at 0x14
    nw4r::lyt::Pane* mTextPane;              // at 0x18
    nw4r::lyt::Pane* mFrame0Pane;            // at 0x1C
    nw4r::lyt::Pane* mFrame1Pane;            // at 0x20
    PaneButton* mLinkA;                      // at 0x24
    PaneButton* mLinkB;                      // at 0x28
    void* mCallbackArg;                      // at 0x2C
    TextColorCallback mTextColorCallback;    // at 0x30
    MoveCallback mMoveCallback;              // at 0x34
    nw4r::ut::Color mTextColor;              // at 0x38
    nw4r::ut::Rect mRect;                    // at 0x3C
    nw4r::math::VEC3 mBasePos;               // at 0x4C
    nw4r::math::VEC3 mTextPos;               // at 0x58
    nw4r::lyt::Size mTextSize;               // at 0x64
    s32 mColorSet;                           // at 0x6C
    s32 mAlpha;                              // at 0x70
    s32 mFadeAlpha;                          // at 0x74
    s32 mHoverFrame;                         // at 0x78
    s32 mPressFrame;                         // at 0x7C
    s32 mSelIndex;                           // at 0x80
    s32 mNextSelIndex;                       // at 0x84
    s32 mBlend;                              // at 0x88
    s32 mBlendMax;                           // at 0x8C
    bool mFixed;                             // at 0x90
    bool mUnk91;                             // at 0x91
    bool mToggle;                            // at 0x92
    bool mInactive;                          // at 0x93
    bool mHidden;                            // at 0x94
    bool mHover;                             // at 0x95
    bool mPressed;                           // at 0x96
    bool mSelected;                          // at 0x97
    bool mDisabled;                          // at 0x98
    bool mFadeFixed;                         // at 0x99
    f32 mOffsetY;                            // at 0x9C
};

#endif
