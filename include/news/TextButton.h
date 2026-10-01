#ifndef NEWS_TEXT_BUTTON_H
#define NEWS_TEXT_BUTTON_H

#include <types.h>
#include <nw4r/ut/ut_Color.h>
#include <nw4r/ut/ut_Rect.h>
#include <nw4r/ut/ut_TextWriterBase.h>
#include <nw4r/math/math_types.h>

// A pointer-selectable button with a text label.
class TextButton {
public:
    enum Align {
        ALIGN_CENTER,
        ALIGN_RIGHT,
        ALIGN_LEFT,
    };

    TextButton(const wchar_t* text, const nw4r::math::VEC2& size, s32 id, bool enabled, s32 align,
               f32 scale);
    virtual ~TextButton();
    virtual void Draw(f32 alpha);
    virtual void Update(const nw4r::math::VEC2& pos);

    void SetPosition(const nw4r::math::VEC2& pos);
    s32 GetPressedChan() const;

protected:
    wchar_t* mText;                              // at 0x04
    nw4r::ut::Rect mRect;                        // at 0x08
    nw4r::ut::TextWriterBase<wchar_t> mWriter;   // at 0x18
    nw4r::ut::Color mHoverColor;                 // at 0x78
    nw4r::ut::Color mDisabledColor;              // at 0x7C
    nw4r::ut::Color mTextColor;                  // at 0x80
    nw4r::ut::Color mShadowColor;                // at 0x84
    nw4r::math::VEC2 mCenter;                    // at 0x88
    nw4r::math::VEC2 mSize;                      // at 0x90
    nw4r::math::VEC2 mTextPos;                   // at 0x98
    f32 mScaleX;                                 // at 0xA0
    f32 mScaleY;                                 // at 0xA4
    f32 mCharSpace;                              // at 0xA8
    f32 mMargin;                                 // at 0xAC
    bool mDisabled;                              // at 0xB0
    bool mHover;                                 // at 0xB1
    bool mPrevHover;                             // at 0xB2
    bool mEnabled;                               // at 0xB3
    bool mPressed[4];                            // at 0xB4
    s32 mId;                                     // at 0xB8
    s32 mAlign;                                  // at 0xBC
};

// TextButton drawn on a framed background texture.
class FrameTextButton : public TextButton {
public:
    FrameTextButton(const wchar_t* text, const nw4r::math::VEC2& size, s32 id, bool enabled,
                    s32 align, f32 scale);
    virtual ~FrameTextButton();
    virtual void Draw(f32 alpha);
};

// TextButton with an icon to the left of the label.
class IconTextButton : public TextButton {
public:
    IconTextButton(const wchar_t* text, const nw4r::math::VEC2& size, s32 id, bool enabled,
                   s32 align, f32 scale, u8 iconFlags);
    virtual ~IconTextButton();
    virtual void Draw(f32 alpha);
    virtual void Update(const nw4r::math::VEC2& pos);

private:
    nw4r::math::VEC2 mIconOffset; // at 0xC0
    nw4r::math::VEC2 mIconSize;   // at 0xC8
    nw4r::math::VEC2 mTextOffset; // at 0xD0
    f32 mMaxTextWidth;            // at 0xD8
    u8 mIconFlags;                // at 0xDC
};

// Framed TextButton with the label nudged upwards.
class SmallTextButton : public TextButton {
public:
    SmallTextButton(const wchar_t* text, const nw4r::math::VEC2& size, s32 id, bool enabled,
                    s32 align, f32 scale);
    virtual ~SmallTextButton();
    virtual void Draw(f32 alpha);
};

#endif
