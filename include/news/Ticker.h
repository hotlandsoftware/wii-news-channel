#ifndef NEWS_TICKER_H
#define NEWS_TICKER_H

#include <types.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_TextWriterBase.h>

class NewsArticle;

// One headline row in the article list. Long headlines scroll horizontally
// while the pointer hovers over them.
class Ticker {
public:
    typedef void (Ticker::*StateFunc)();

    enum Mode {
        MODE_WAIT,
        MODE_SCROLL,
        MODE_RETURN,
    };

    Ticker();
    ~Ticker();

    BOOL Init(NewsArticle* article, nw4r::ut::TextWriterBase<wchar_t>* writer, f32 thumbX,
              f32 thumbY, f32 arg3, f32 arg4);
    void ResetState();
    void Draw(const nw4r::math::VEC2& pos, const f32& offsetX, const f32& scale,
              const f32& alpha);
    void DrawSeparator(const f32& offsetX, const f32& alpha);
    void Layout(nw4r::math::VEC2& pos);
    u32 UpdateHover();
    void Update();

    void StateWait();
    void StateScroll();
    void StateReturn();

    void SetLayout(nw4r::math::VEC2& pos, f64 scale);
    void Dummy();
    void GetOrigin(nw4r::math::VEC2& out);
    void TruncateText();
    f32 CalcTextWidth(const wchar_t* str);

private:
    void ChangeState(StateFunc state) {
        if (mState) {
            mPhase = -1;
            (this->*mState)();
        }
        mState = state;
        mPhase = 0;
        if (mState) {
            (this->*mState)();
        }
    }

    BOOL IsState(StateFunc state) { return mState == state; }

    f32 GetScale() const { return mFontScale * mTextScale; }
    f32 GetScrollWidth() const { return mViewWidth - 30.0f * GetScale(); }
    inline f32 GetTextHeight(f32 scale);
    inline f32 CalcRowHeight(f32 scale);

public:
    u32 unk0;                                   // at 0x00
    u32 unk4;                                   // at 0x04
    NewsArticle* mArticle;                      // at 0x08
    nw4r::ut::TextWriterBase<wchar_t>* mWriter; // at 0x0C
    StateFunc mState;                           // at 0x10
    f32 unk1C;                                  // at 0x1C
    f32 unk20;                                  // at 0x20
    f32 unk24;                                  // at 0x24
    f32 unk28;                                  // at 0x28
    f32 mLeft;                                  // at 0x2C
    f32 mTop;                                   // at 0x30
    f32 mRight;                                 // at 0x34
    f32 mBottom;                                // at 0x38
    f32 mThumbX;                                // at 0x3C
    f32 mThumbY;                                // at 0x40
    f32 mThumbLeft;                             // at 0x44
    f32 mThumbTop;                              // at 0x48
    f32 mThumbPosX;                             // at 0x4C
    f32 mThumbPosY;                             // at 0x50
    f32 mTextX;                                 // at 0x54
    f32 mTextY;                                 // at 0x58
    f32 mTextScale;                             // at 0x5C
    f32 unk60;                                  // at 0x60
    f32 unk64;                                  // at 0x64
    f32 mTextWidth;                             // at 0x68
    f32 mScrollPos;                             // at 0x6C
    f32 mScrollVelocity;                        // at 0x70
    f32 mScrollTargetVelocity;                  // at 0x74
    f32 mViewWidth;                             // at 0x78
    f32 mThumbScale;                            // at 0x7C
    f32 mFontScale;                             // at 0x80
    bool mHidden;                               // at 0x84
    bool mHover;                                // at 0x85
    bool mPrevHover;                            // at 0x86
    u32 mNumChars;                              // at 0x88
    u32 unk8C;                                  // at 0x8C
    u32 unk90;                                  // at 0x90
    u32 unk94;                                  // at 0x94
    u32 mClipWidth;                             // at 0x98
    s32 mMode;                                  // at 0x9C
    s32 mPhase;                                 // at 0xA0
    s32 mTimer;                                 // at 0xA4
};

#endif
