#include <news/TextButton.h>
#include <news/Common.h>
#include <news/Draw2D.h>
#include <news/System.h>

using namespace nw4r;

FrameTextButton::FrameTextButton(const wchar_t* text, const math::VEC2& size, s32 id, bool enabled,
                                 s32 align, f32 scale)
    : TextButton(text, size, id, enabled, align, scale) {
    if (IsErrorState()) {
        return;
    }

    if (gFitButtonText) {
        f32 maxWidth = mRect.GetWidth() - 48.0f;
        f32 width = mWriter.CalcStringWidth(mText);
        if (maxWidth < width) {
            f32 fit = scale * (maxWidth / width);
            mScaleX *= fit;
            mCharSpace *= fit;
            mWriter.SetScale(mScaleX, mScaleY);
            mWriter.SetCharSpace(mCharSpace);
        }
        mMargin = 24.0f;
    }
}

FrameTextButton::~FrameTextButton() {}

void FrameTextButton::Draw(f32 alpha) {
    u8 a = 255.0f * alpha;
    ut::Rect rect(mRect.left, mRect.top, mRect.right, 56.0f + (2.0f + mRect.top));
    mHoverColor.a = a;
    mDisabledColor.a = a;
    mTextColor.a = a;
    mShadowColor.a = a;

    Draw2D_SetupGX();
    Draw2D_SetOrtho();

    if (mDisabled) {
        GXSetTevColor(GX_TEVREG0, mDisabledColor);
        Draw2D_TexRect(gCommonTpl, 9, &rect, 0.0f, 0);
    } else if (mHover) {
        GXSetTevColor(GX_TEVREG0, mHoverColor);
        Draw2D_TexRect(gCommonTpl, 9, &rect, 0.0f, 0);
        rect.left += 2.0f;
        rect.right -= 2.0f;
        rect.top += 2.0f;
        rect.bottom = 28.0f + rect.top;
        GXSetTevColor(GX_TEVREG0, ut::Color(255, 255, 255, a));
        Draw2D_TexRect(gCommonTpl, 10, &rect, 0.0f, 0);
    } else {
        GXSetTevColor(GX_TEVREG0, ut::Color(255, 255, 255, a));
        Draw2D_TexRect(gCommonTpl, 9, &rect, 0.0f, 0);
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
