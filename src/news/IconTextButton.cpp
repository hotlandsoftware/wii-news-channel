#include <news/TextButton.h>
#include <news/Common.h>
#include <news/Draw2D.h>
#include <news/System.h>

using namespace nw4r;

IconTextButton::IconTextButton(const wchar_t* text, const math::VEC2& size, s32 id, bool enabled,
                               s32 align, f32 scale, u8 iconFlags)
    : TextButton(text, size, id, enabled, align, scale),
      mIconOffset(0.0f, 0.0f),
      mIconSize(TPL_GetWidth(gCommonTpl, 8), TPL_GetHeight(gCommonTpl, 8)),
      mTextOffset(0.0f, 0.0f),
      mMaxTextWidth(0.0f),
      mIconFlags(iconFlags) {
    if (IsErrorState()) {
        return;
    }

    mSize.x = TPL_GetWidth(gCommonTpl, 7);
    mMaxTextWidth = mSize.x - (18.0f + mIconSize.x);
    mIconOffset.x = 6.0f - 0.5f * mSize.x;
    mIconOffset.y = -(0.5f * mIconSize.y);
    mTextOffset.x = 0.5f * mMaxTextWidth + (6.0f + (mIconOffset.x + mIconSize.x));

    f32 width = mWriter.CalcStringWidth(mText);
    if (mMaxTextWidth < width) {
        f32 fit = mMaxTextWidth / width;
        mScaleX *= fit;
        mCharSpace *= fit;
        mWriter.SetScale(mScaleX, mScaleY);
        mWriter.SetCharSpace(mCharSpace);
    }
}

IconTextButton::~IconTextButton() {}

void IconTextButton::Draw(f32 alpha) {
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
        Draw2D_TexRect(gCommonTpl, 7, &rect, 0.0f, 0);
    } else if (mHover) {
        GXSetTevColor(GX_TEVREG0, mHoverColor);
        Draw2D_TexRect(gCommonTpl, 7, &rect, 0.0f, 0);
        rect.left += 2.0f;
        rect.right -= 2.0f;
        rect.top += 2.0f;
        rect.bottom = 28.0f + rect.top;
        GXSetTevColor(GX_TEVREG0, ut::Color(255, 255, 255, a));
        Draw2D_TexRect(gCommonTpl, 10, &rect, 0.0f, 0);
    } else {
        GXSetTevColor(GX_TEVREG0, ut::Color(255, 255, 255, a));
        Draw2D_TexRect(gCommonTpl, 7, &rect, 0.0f, 0);
    }

    math::VEC3 pos(mCenter.x + mIconOffset.x, mCenter.y + mIconOffset.y, 0.0f);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C1, GX_CC_TEXC, GX_CC_C0, GX_CC_C1);
    GXSetTevColor(GX_TEVREG0, ut::Color(255, 255, 255, a));
    if (mHover) {
        GXSetTevColor(GX_TEVREG1, ut::Color(0, 140, 216, 0));
    } else {
        GXSetTevColor(GX_TEVREG1, ut::Color(108, 108, 108, 0));
    }
    Draw2D_TexPos(gCommonTpl, 8, &pos, 1.0f, 1.0f, mIconFlags);
    Draw2D_SetupGX();

    pos.x = mTextPos.x + mTextOffset.x;
    pos.y = mTextPos.y + mTextOffset.y;
    mWriter.SetupGX();
    if (mHover) {
        mWriter.SetCursor(1.0f + pos.x, 1.0f + pos.y);
        mWriter.SetTextColor(mShadowColor);
        mWriter.Print(mText);
    }
    mWriter.SetCursor(pos.x, pos.y);
    mWriter.SetTextColor(mTextColor);
    mWriter.Print(mText);
}

void IconTextButton::Update(const math::VEC2& pos) {
    TextButton::Update(pos);
}
