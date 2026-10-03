#ifndef NEWS_PANE_LAYOUT_H
#define NEWS_PANE_LAYOUT_H

#include <types.h>
#include <nw4r/math/math_types.h>
#include <revolution/mtx.h>

namespace nw4r {
namespace lyt {
class ArcResourceAccessor;
class Layout;
class DrawInfo;
} // namespace lyt
} // namespace nw4r

class PaneButton;
struct PaneButtonColors;
class ColorTagProcessor;

// A layout whose top-level panes are PaneButtons (PaneLayout.cpp, 0x80047B50).
// Allocated with operator new(0x434).
struct Layout {
    enum {
        MAX_BUTTONS = 256,
    };

    Layout(void* arc, const char* name, PaneButtonColors* colors, bool influencedAlpha);
    ~Layout();

    void Reset();
    void Calc();
    void Draw();
    PaneButton* HitTest(f32 x, f32 y);
    PaneButton* FindButton(const char* name);
    void SlideIn(s32 frames);
    void SlideOut(s32 frames);
    void FadeIn(s32 frames);
    void SetBlend(s32 alpha, s32 blend, s32 blendMax);
    void SetAlpha(s32 alpha);
    void SetViewMtx(const Mtx mtx);
    void SetButtonAlpha(s32 alpha);
    void UpdatePanes();
    void SetSlide(f32 step);

    nw4r::lyt::ArcResourceAccessor* mResAccessor; // at 0x000
    nw4r::lyt::Layout* mLayout;                   // at 0x004
    nw4r::lyt::DrawInfo* mDrawInfo;               // at 0x008
    u32 mUnk00C;                                  // at 0x00C
    ColorTagProcessor* mTagProcessor;             // at 0x010
    PaneButton* mButtons[MAX_BUTTONS];            // at 0x014
    s32 mButtonCount;                             // at 0x414
    bool mSlideOut;                               // at 0x418
    s32 mSlideLength;                             // at 0x41C
    s32 mSlideFrame;                              // at 0x420
    bool mFadeOut;                                // at 0x424
    s32 mFadeLength;                              // at 0x428
    s32 mFadeFrame;                               // at 0x42C
    s32 mAlpha;                                   // at 0x430
};

// Scene input helpers (d_scene.cpp): button hover/press handling of a Layout
// for every pointer.
void UpdateLayoutButtons(Layout* layout, u32 se);
void ClearButtonHover();
s32 CheckButtonHold(const char* name, u32 button);
s32 CheckButtonTrig(const char* name, u32 button);

#endif
