#ifndef NEWS_GLOBE_PIN_H
#define NEWS_GLOBE_PIN_H

#include <types.h>
#include <news/GlobePoint.h>
#include <news/Scroller.h>
#include <nw4r/math/math_types.h>

class NewsArticle;
class Camera;

namespace nw4r {
namespace ut {
class CharWriter;
class Font;
}
} // namespace nw4r

// A news location shown on the globe: a pin with a ripple effect, a label
// with the location name and, when hovered, the article picture cards of
// every article at that location (GlobePins at the same place are chained
// through mNext).
class GlobePin : public GlobePoint {
public:
    typedef void (GlobePin::*StateFunc)();

    struct Ripple {
        f32 mScale;   // at 0x0
        s32 mAlpha;   // at 0x4
        s32 mAngle;   // at 0x8
    };

    GlobePin(s32 arg1, s32 arg2, NewsArticle* article, f32 radius);
    virtual ~GlobePin();

    void Draw(u8 alpha);
    nw4r::math::VEC2 GetPos();
    BOOL DrawCards(u8 alpha);
    void DrawLabel();
    void DrawName();
    void DrawHeadline(nw4r::ut::CharWriter* writer);
    void Update(Camera* camera);
    void UpdateCards(f32 alpha);
    void StateHidden();
    void StateRipple();
    void TruncateHeadline(nw4r::ut::CharWriter* writer);
    void TruncateLocation(nw4r::ut::CharWriter* writer);
    f32 CalcHeadlineWidth(const wchar_t* str, const nw4r::ut::Font* font, f32 scale, f32 space);
    void LayoutPicture(f32 size);
    s32 CompareLabel(GlobePin* other);

    void ResetRipples() {
        mRipples[0].mAngle = 0;
        mRipples[0].mScale = 0.0f;
        mRipples[0].mAlpha = 0;
        mRipples[1].mAngle = -0x2000;
        mRipples[1].mScale = 0.0f;
        mRipples[1].mAlpha = 0;
    }

    void ChangeState(StateFunc state) {
        if (mState) {
            mPhase = -1;
            (this->*mState)();
        }
        mPhase = 0;
        mState = state;
        if (mState) {
            (this->*mState)();
        }
    }

    GlobePin* mNext;               // at 0x028
    u32 m2C;                       // at 0x02C
    NewsArticle* mArticle;         // at 0x030
    StateFunc mState;              // at 0x034
    PC_PMF_PAD(mState)
    Ripple mRipples[2];            // at 0x040
    Scroller mHeadlineScroller;    // at 0x058
    Scroller mLocationScroller;    // at 0x088
    f32 mPicX;                     // at 0x0B8
    f32 mPicY;                     // at 0x0BC
    f32 mC0;                       // at 0x0C0
    f32 mPicScale;                 // at 0x0C4
    u32 mHeadlineLen;              // at 0x0C8
    u32 mCC;                       // at 0x0CC
    u32 mD0;                       // at 0x0D0
    u32 mD4;                       // at 0x0D4
    u32 mLocationLen;              // at 0x0D8
    s32 mDC;                       // at 0x0DC
    s32 mE0;                       // at 0x0E0
    s32 mE4;                       // at 0x0E4
    s32 mPressedChan;              // at 0x0E8
    s32 mPhase;                    // at 0x0EC
    s32 mCount;                    // at 0x0F0
    bool mHover[4];                // at 0x0F4
    bool mActive;                  // at 0x0F8
    bool mOpened;                  // at 0x0F9
    bool mLabelHidden;             // at 0x0FA
    u8 mLabelAlpha;                // at 0x0FB
    u8 mHoverTime;                 // at 0x0FC
    Quaternion mQuat;              // at 0x100
    Quaternion mCurQuat;           // at 0x110
    nw4r::math::MTX34 mCardMtx;    // at 0x120
    f32 mCardDist;                 // at 0x150
    f32 mCardW;                    // at 0x154
    f32 mCardH;                    // at 0x158
    u8 mCardAlpha;                 // at 0x15C
    nw4r::math::VEC2 mLabelPos;    // at 0x160
    f32 mLabelW;                   // at 0x168
    f32 mLabelH;                   // at 0x16C
};

#endif
