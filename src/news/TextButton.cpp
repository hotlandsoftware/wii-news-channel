#include <news/TextButton.h>
#include <news/Common.h>
#include <news/Draw2D.h>
#include <news/System.h>
#include <wchar.h>

using namespace nw4r;

static ut::Color sGrayColor(80, 80, 80, 255);
static ut::Color sWhiteColor(255, 255, 255, 255);
static ut::Color sDisabledColor(100, 100, 180, 255);
static ut::Color sHoverTextColor(255, 255, 255, 255);
static ut::Color sTextColor(0, 0, 0, 255);
static ut::Color sShadowColor(0, 86, 104, 255);
static ut::Color sHoverColor(18, 162, 192, 255);

TextButton::TextButton(const wchar_t* text, const math::VEC2& size, s32 id, bool enabled, s32 align,
                       f32 scale)
    : mText(NULL),
      mRect(0.0f, 0.0f, size.x, size.y),
      mHoverColor(sHoverColor),
      mDisabledColor(sDisabledColor),
      mTextColor(sTextColor),
      mShadowColor(sShadowColor),
      mCenter(0.0f, 0.0f),
      mSize(size),
      mTextPos(0.0f, 0.0f),
      mScaleX(scale),
      mScaleY(scale),
      mCharSpace(0.0f),
      mMargin(0.0f),
      mHover(false),
      mPrevHover(false),
      mEnabled(enabled),
      mId(id),
      mAlign(align) {
    for (int i = 0; i < 4; i++) {
        mPressed[i] = false;
    }

    mText = new wchar_t[wcslen(text) + 1];
    if (mText == NULL) {
        gAllocFailed = true;
        return;
    }
    wcscpy(mText, text);

    u32 flag;
    switch (mAlign) {
    case ALIGN_LEFT:
        mMargin = 5.0f;
        flag = 0x00;
        break;
    case ALIGN_RIGHT:
        mMargin = 5.0f;
        flag = 0x22;
        break;
    case ALIGN_CENTER:
    default:
        flag = 0x11;
        break;
    }

    mWriter.SetFont(*gSysFont);
    mWriter.SetDrawFlag(flag | 0x100);
    mWriter.SetScale(mScaleX, mScaleY);
    mWriter.SetCharSpace(mCharSpace);

    f32 width = mWriter.CalcStringWidth(mText);
    f32 maxWidth = mRect.GetWidth() - 10.0f;
    if (maxWidth < width) {
        f32 fit = scale * (maxWidth / width);
        mScaleX *= fit;
        mCharSpace *= fit;
        mWriter.SetScale(mScaleX, mScaleY);
        mWriter.SetCharSpace(mCharSpace);
    }
}

TextButton::~TextButton() {
    if (mText != NULL) {
        delete[] mText;
    }
}

void TextButton::Draw(f32 alpha) {
    u8 a = 255.0f * alpha;
    mHoverColor.a = a;
    mDisabledColor.a = a;
    mTextColor.a = a;
    mShadowColor.a = a;

    Draw2D_SetupGX();
    Draw2D_SetOrtho();

    math::VEC3 quad[4];
    quad[0].x = quad[1].x = mRect.left;
    quad[2].x = quad[3].x = mRect.right;
    quad[0].y = quad[3].y = mRect.top;
    quad[1].y = quad[2].y = mRect.bottom;
    quad[0].z = quad[1].z = quad[2].z = quad[3].z = 0.0f;

    if (mDisabled) {
        Draw2D_FillQuad(quad, &mDisabledColor);
    } else if (mHover) {
        Draw2D_FillQuad(quad, &mHoverColor);
    }

    mWriter.SetupGX();
    if (mHover) {
        mWriter.SetCursor(1.0f + mTextPos.x, 1.0f + mTextPos.y);
        mWriter.SetTextColor(mShadowColor);
        mWriter.Print(mText);
    }
    mWriter.SetCursor(mTextPos.x, mTextPos.y);
    mWriter.SetTextColor(mTextColor);
    mWriter.Print(mText);
}

void TextButton::Update(const math::VEC2& pos) {
    mPrevHover = mHover;
    mHover = false;
    SetPosition(pos);

    for (int i = 0; i < 4; i++) {
        mPressed[i] = false;
    }

    if (!mDisabled && mEnabled) {
        for (s32 chan = 0; chan < 4; chan++) {
            if (IsPointerValid(chan)) {
                f32 x = gCursorX[chan][0];
                f32 y = gCursorY[chan][0];
                // Pointer must be inside the content area (between header and footer)
                f32 minY = 63.0f;
                f32 maxY = 393.0f;
                if (y > minY && y < maxY && x > mRect.left && x < mRect.right && y > mRect.top &&
                    y < mRect.bottom) {
                    mHover = true;
                    if (!mPrevHover) {
                        PlaySE(0x2A);
                    }
                    if (gTrig[chan] & 0x800) {
                        mPressed[chan] = true;
                    }
                }
            }
        }

        if (mHover) {
            mTextColor = sHoverTextColor;
        } else {
            mTextColor = sTextColor;
        }
    }
}

void TextButton::SetPosition(const math::VEC2& pos) {
    f32 halfW = 0.5f * mSize.x;
    f32 halfH = 0.5f * mSize.y;

    mCenter.x = pos.x;
    mCenter.y = pos.y;
    mRect.left = pos.x - halfW;
    mRect.top = pos.y - halfH;
    mRect.right = pos.x + halfW;
    mRect.bottom = pos.y + halfH;
    mTextPos.y = pos.y;

    switch (mAlign) {
    case ALIGN_LEFT:
        mTextPos.x = mRect.left + mMargin;
        break;
    case ALIGN_RIGHT:
        mTextPos.x = mRect.right - mMargin;
        break;
    case ALIGN_CENTER:
    default:
        mTextPos.x = mCenter.x;
        break;
    }
}

s32 TextButton::GetPressedChan() const {
    for (s32 chan = 0; chan < 4; chan++) {
        if (mPressed[chan]) {
            return chan;
        }
    }
    return -1;
}
