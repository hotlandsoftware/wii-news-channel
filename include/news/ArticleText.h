#ifndef NEWS_ARTICLE_TEXT_H
#define NEWS_ARTICLE_TEXT_H

#include <types.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_Color.h>
#include <nw4r/ut/ut_Rect.h>

struct MEMAllocator;
struct NewsTexture;
struct NewsPicture;

namespace nw4r {
namespace ut {
class CharWriter;
class Font;
} // namespace ut
} // namespace nw4r

// One laid-out character of an ArticleText (0x70 bytes). Characters that may
// not be separated by a line break are chained into words through
// mPrev/mNext. The class's code is TextChar.cpp (0x8000CA98).
class TextChar {
public:
    typedef void (TextChar::*StateFunc)();

    TextChar();
    ~TextChar();

    void Update(const nw4r::math::VEC2* origin, const f32* rate);
    void SetLine(s32 line);
    void SetTarget(f32 x, f32 y);
    void StateMove();
    void StateDrop();

    void ChangeState(StateFunc state) {
        if (mState) {
            mStateFrame = -1;
            (this->*mState)();
        }
        mState = state;
        mStateFrame = 0;
        if (mState) {
            (this->*mState)();
        }
    }

    BOOL IsState(StateFunc state) { return mState == state; }

    u16 mChar;                   // at 0x00
    nw4r::math::VEC2 mPos;       // at 0x04
    nw4r::math::VEC2 mTarget;    // at 0x0C
    f32 mScale;                  // at 0x14
    f32 mWidth;                  // at 0x18
    f32 mHeight;                 // at 0x1C
    f32 mScaledWidth;            // at 0x20
    f32 mScaledHeight;           // at 0x24
    TextChar* mPrev;             // at 0x28
    TextChar* mNext;             // at 0x2C
    f32 mLeft;                   // at 0x30
    f32 mTop;                    // at 0x34
    f32 mRight;                  // at 0x38
    f32 mBottom;                 // at 0x3C
    nw4r::ut::Color mColor;      // at 0x40
    StateFunc mState;            // at 0x44
    f32 mWordWidth;              // at 0x50
    f32 mRate;                   // at 0x54
    f32 mScaleX;                 // at 0x58
    f32 mUnk5C;                  // at 0x5C
    bool mHidden;                // at 0x60
    bool mSelected;              // at 0x61
    u16 mWordChar;               // at 0x62
    s32 mStateFrame;             // at 0x64
    s32 mLine;                   // at 0x68
    s32 mWordIndex;              // at 0x6C
};


// The body text of an article (optionally with a picture, a caption and a
// credit line): word-wrapped character by character, animated and drawn
// with a CharWriter.
class ArticleText {
public:
    ArticleText(MEMAllocator* allocator, nw4r::ut::CharWriter* writer, s32 maxChars,
                const nw4r::math::VEC2& size, f32 scale);
    ~ArticleText();

    bool IsNoBreak(const wchar_t* p, const wchar_t* start);
    bool Set(const wchar_t* text, NewsPicture* picture, const nw4r::math::VEC2* start,
             const nw4r::math::VEC2* picPos, const f32* picScale, const nw4r::math::VEC2* size,
             bool indent, f32 scale, f32 fontScale, bool smallPicture, bool isCaption);
    void Draw(const nw4r::math::VEC2* pos, bool clip, f32 alpha, f32 zoom);
    void Update(const nw4r::math::VEC2* pos, bool clip, f32 scroll);
    void HideAll();
    bool LayoutPicture(const nw4r::math::VEC2* pos, f32 scale);
    void Layout(f32 scale);
    void Layout(const nw4r::math::VEC2* pos, f32 scale);
    TextChar* PlaceWord(TextChar* c, const f32& scale, const f32& scaleX, nw4r::math::VEC2& cursor,
                        s32& count);
    void Snap();
    void ResetUnk80();
    void SetScale(f32 scale);
    void StartScroll(const nw4r::math::VEC2* pos, const nw4r::math::VEC2* from,
                     const nw4r::math::VEC2* picPos, const f32* picScale);
    bool Select(const nw4r::ut::Rect* rect);
    void ClearSelection();
    f32 GetTop();
    bool GetPictureRect(nw4r::ut::Rect* rect);

    f32 GetHeight() { return mHeight; }
    f32 GetLineHeight() { return mLineHeight; }
    s32 GetNumLines() { return mNumLines; }
    NewsTexture* GetPicture() const { return mPicture; }
    const wchar_t* GetPicLabel() const { return mPicLabel; }

    MEMAllocator* mAllocator;          // at 0x00
    nw4r::ut::CharWriter* mWriter;     // at 0x04
    const nw4r::ut::Font* mFont;       // at 0x08
    TextChar* mChars;                  // at 0x0C
    NewsTexture* mPicture;             // at 0x10
    const wchar_t* mText;              // at 0x14
    const wchar_t* mPicLabel;          // at 0x18
    ArticleText* mSub;                 // at 0x1C
    nw4r::math::VEC2 mSize;            // at 0x20
    nw4r::math::VEC2 mPicPos;          // at 0x28
    nw4r::math::VEC2 mPicTarget;       // at 0x30
    nw4r::math::VEC2 mPicSize;         // at 0x38
    nw4r::math::VEC2 mSubPos;          // at 0x40
    nw4r::math::VEC2 mSubTarget;       // at 0x48
    nw4r::math::VEC2 mCursor;          // at 0x50
    f32 mScale;                        // at 0x58
    f32 mHeight;                       // at 0x5C
    f32 mRevealRate;                   // at 0x60
    f32 mPicScale;                     // at 0x64
    f32 mPicTargetScale;               // at 0x68
    f32 mFontScale;                    // at 0x6C
    f32 mLineHeight;                   // at 0x70
    f32 mBaseLineHeight;               // at 0x74
    f32 mTextTop;                      // at 0x78
    f32 mLabelScale;                   // at 0x7C
    f32 mUnk80;                        // at 0x80
    f32 mLeft;                         // at 0x84
    f32 mRight;                        // at 0x88
    f32 mWrapRight;                    // at 0x8C
    f32 mIndent;                       // at 0x90
    f32 mPicBottom;                    // at 0x94
    s32 mMaxChars;                     // at 0x98
    s32 mLength;                       // at 0x9C
    s32 mCount;                        // at 0xA0
    s32 mFirstVisible;                 // at 0xA4
    s32 mLastVisible;                  // at 0xA8
    s32 mLastFull;                     // at 0xAC
    s32 mNumLines;                     // at 0xB0
    s32 mRevealPos;                    // at 0xB4
    s32 mPicLines;                     // at 0xB8
    s32 mRevealWait;                   // at 0xBC
    bool mIndentFirst;                 // at 0xC0
    bool mSmallPicture;                // at 0xC1
    bool mPicWrapped;                  // at 0xC2
    bool mIsCaption;                   // at 0xC3
    bool mTruncated;                   // at 0xC4
};

#endif
