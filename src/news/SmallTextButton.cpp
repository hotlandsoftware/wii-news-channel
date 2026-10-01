#include <news/TextButton.h>
#include <news/Common.h>
#include <news/Draw2D.h>
#include <news/System.h>

using namespace nw4r;

SmallTextButton::SmallTextButton(const wchar_t* text, const math::VEC2& size, s32 id, bool enabled,
                                 s32 align, f32 scale)
    : TextButton(text, size, id, enabled, align, scale) {}

SmallTextButton::~SmallTextButton() {}

void SmallTextButton::Draw(f32 alpha) {
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
    f32 y = mTextPos.y - 2.0f;
    if (mHover) {
        mWriter.SetCursor(1.0f + mTextPos.x, 1.0f + y);
        mWriter.SetTextColor(mShadowColor);
        mWriter.Print(mText);
    }
    mWriter.SetCursor(mTextPos.x, y);
    mWriter.SetTextColor(mTextColor);
    mWriter.Print(mText);
}
