#include <news/Ticker.h>
#include <news/Common.h>
#include <news/Draw2D.h>
#include <news/MathUtil.h>
#include <news/NewsArticle.h>
#include <news/System.h>
#include <nw4r/ut/ut_Font.h>
#include <wchar.h>

using namespace nw4r;

static f32 sOrigin[2] = {0.0f, 0.0f};

inline f32 Ticker::GetTextHeight(f32 scale) {
    return scale * (mFontScale * mWriter->GetFont()->GetHeight());
}

inline f32 Ticker::CalcRowHeight(f32 scale) {
    f32 height = 1.55f * GetTextHeight(scale);
    if (mArticle->GetTexture()) {
        f32 withThumb = 80.0f + 0.55f * GetTextHeight(scale);
        height = (withThumb < height) ? height : withThumb;
    }
    return height;
}

Ticker::Ticker() {
    unk0 = 0;
    unk4 = 0;
    mArticle = NULL;
    mWriter = NULL;
    mState = NULL;
    unk1C = 0.0f;
    unk20 = 0.0f;
    unk24 = 0.0f;
    unk28 = 0.0f;
    mLeft = 0.0f;
    mTop = 0.0f;
    mRight = 0.0f;
    mBottom = 0.0f;
    mThumbX = 0.0f;
    mThumbY = 0.0f;
    mThumbLeft = 0.0f;
    mThumbTop = 0.0f;
    mThumbPosX = 0.0f;
    mThumbPosY = 0.0f;
    mTextScale = 0.0f;
    unk60 = 0.0f;
    unk64 = 0.0f;
    mTextWidth = 0.0f;
    mScrollPos = 0.0f;
    mScrollVelocity = 0.0f;
    mScrollTargetVelocity = 0.0f;
    mViewWidth = 0.0f;
    mThumbScale = 1.0f;

    f32 fontScale;
    if (gLargeFont) {
        if (gLanguage == 0) {
            fontScale = 1.2f;
        } else {
            fontScale = 1.0f;
        }
    } else {
        fontScale = gDefaultFontScale;
    }
    mFontScale = fontScale;

    mHidden = false;
    mHover = false;
    mPrevHover = false;
    mNumChars = 0;
    unk8C = 0;
    unk90 = 0;
    unk94 = 0;
    mClipWidth = 0;
    mPhase = 0;
    mTimer = 0;
}

Ticker::~Ticker() {}

BOOL Ticker::Init(NewsArticle* article, ut::TextWriterBase<wchar_t>* writer, f32 thumbX, f32 thumbY,
                  f32 arg3, f32 arg4) {
    if (IsErrorState()) {
        return FALSE;
    }

    mArticle = article;
    mWriter = writer;
    unk24 = arg3;
    unk28 = arg4;
    mThumbX = thumbX;
    mThumbY = thumbY;
    mNumChars = article->mText->size >> 1;
    unk8C = article->unk58;
    ChangeState(&Ticker::StateWait);
    return TRUE;
}

void Ticker::ResetState() {
    ChangeState(&Ticker::StateWait);
}

static inline void SetTevColorWhite(u8 alpha) {
    GXColor color = {255, 255, 255, alpha};
    GXSetTevColor(GX_TEVREG0, color);
}

void Ticker::Draw(const math::VEC2& pos, const f32& offsetX, const f32& scale, const f32& alpha) {
    u8 a = 255.0f * alpha;
    f32 s = mFontScale * scale;
    ut::Color highlight = gHighlightColor;
    highlight.a = a;

    if (mHover) {
        math::VEC3 quad[4];
        quad[0].x = quad[1].x = mLeft + offsetX;
        quad[0].y = quad[3].y = mTop - 5.0f;
        quad[1].y = quad[2].y = mBottom;
        quad[2].x = quad[3].x = mRight;
        quad[0].z = quad[1].z = quad[2].z = quad[3].z = 0.0f;
        Draw2D_SetupGX();
        GXSetZMode(FALSE, GX_LEQUAL, FALSE);
        Draw2D_FillQuad(quad, &highlight);
    }

    u32 drawFlag = mWriter->GetDrawFlag();
    u32 margin = 30.0f * s;
    f32 height = 1.55f * GetTextHeight(scale);
    f32 centerY = 0.5f * height + (pos.y + mTextY);
    ut::Color white(255, 255, 255, a);
    math::VEC3 textPos(0.0f, 0.0f, 0.0f);
    const ut::Font* font = mWriter->GetFont();

    mWriter->SetDrawFlag(0x100);
    mWriter->SetupGX();
    mWriter->SetCharSpace(s * gCharSpaceScale);
    mWriter->SetScale(s);

    Draw2D_SetupGX();
    GXSetZMode(FALSE, GX_LEQUAL, FALSE);
    textPos.x = pos.x + mTextX;
    textPos.y = centerY;
    GXSetTevColor(GX_TEVREG0, white);
    Draw2D_Icon(mArticle->GetCategoryIcon(), &textPos, s, s, 0x100);

    Draw2D_SetScissor(textPos.x, 0, mClipWidth - margin, gRenderMode.efbHeight);
    textPos.x += mScrollPos;
    textPos.y = centerY - 0.5f * mWriter->GetFontHeight();
    mWriter->SetupGX();
    if (mArticle->mFlags & 1) {
        mWriter->SetTextColor(ut::Color(80, 80, 80, a));
    } else {
        mWriter->SetTextColor(ut::Color(0, 0, 0, a));
    }
    textPos.y += s;
    mWriter->SetCursor(textPos.x, textPos.y);

    f32 yOffset = s * font->GetAscent();
    f32 charSpace = mWriter->GetCharSpace();
    yOffset -= yOffset;
    u32 i = 0;
    const wchar_t* text;
    if (mMode == MODE_WAIT) {
        text = mArticle->mShortHeadline;
    } else {
        text = mArticle->mHeadline;
    }

    for (; *text != 0; text++, i++) {
        if (i >= mNumChars) {
            charSpace = s * gCharSpaceScale;
            mWriter->SetCharSpace(charSpace);
            mWriter->SetScale(s);
            mWriter->SetCursorY(textPos.y + yOffset);
        }
        if (*text == 0xA0) {
            mWriter->Print(L' ');
        } else {
            mWriter->Print(*text);
        }
        mWriter->SetCursorX(charSpace + mWriter->GetCursorX());
    }

    Draw2D_SetScissor(0, 0, GetScreenWidth(), 456);
    mWriter->SetDrawFlag(drawFlag);

    if (mArticle->GetTexture()) {
        f32 halfHeight = 0.5f * (mThumbScale * mArticle->GetTexture()->height);
        f32 rowHeight = CalcRowHeight(scale);
        math::VEC3 thumbPos(pos.x + mThumbPosX, mTop + (0.5f * rowHeight - halfHeight), 0.0f);
        Draw2D_SetupGX();
        GXSetZMode(FALSE, GX_LEQUAL, FALSE);
        SetTevColorWhite(a);
        Draw2D_Texture(mArticle->GetTexture(), &thumbPos, mThumbScale);
    }
}

void Ticker::DrawSeparator(const f32& offsetX, const f32& alpha) {
    u8 a = 255.0f * alpha;
    f32 width = 10.0f + (GetScreenWidth() - GetSideMargin() * 2);
    ut::Color color = gSeparatorColor;
    color.a = a;
    math::VEC3 line[2];
    line[0].x = mLeft + offsetX;
    line[1].x = line[0].x + width;
    line[0].y = line[1].y = mBottom;
    line[0].z = line[1].z = 0.0f;
    Draw2D_Line(line[0], line[1], 6, color, color);
}

void Ticker::Layout(math::VEC2& pos) {
    f32 minY = 0.0f;
    f32 maxY = 456.0f;
    f32 height = CalcRowHeight(mTextScale);

    unk4 = 0;
    unk0 = 0;
    mLeft = (pos.x + mTextX) - 5.0f;
    mTop = pos.y + mTextY;
    mRight = mLeft + (10.0f + (GetScreenWidth() - GetSideMargin() * 2));
    mBottom = mTop + height;
    mHidden = !(mBottom > minY && mTop < maxY);

    if (!mHidden) {
        f32 s = mFontScale * mTextScale;
        f32 space = gCharSpaceScale;
        mWriter->SetCharSpace(s * space);
        mWriter->SetScale(s);
        mTextWidth = CalcTextWidth(mArticle->mHeadline);
        if (mArticle->GetTexture()) {
            mClipWidth = mThumbLeft - unk1C;
        } else {
            mClipWidth = GetContentRight() - GetSideMargin();
        }
        mViewWidth = mClipWidth;
        if (!mHidden) {
            TruncateText();
        }
    }
}

u32 Ticker::UpdateHover() {
    f32 minY;
    f32 maxY = 393.0f;
    mPrevHover = mHover;
    mHover = false;
    u32 hits = 0;

    if (!mHidden) {
        minY = 63.0f;
        for (s32 chan = 0; chan < 4; chan++) {
            if (IsPointerValid(chan)) {
                f32 x = gCursorX[chan][0];
                f32 y = gCursorY[chan][0];
                if (y > minY && y < maxY && x > mLeft && x < mRight && y > mTop && y < mBottom) {
                    hits |= 1 << chan;
                    if (!mHover) {
                        mHover = true;
                        if (!mPrevHover) {
                            PlaySE(0x26);
                        }
                    }
                }
            }
        }
    }
    return hits;
}

void Ticker::Update() {
    if (mHidden) {
        if (!IsState(&Ticker::StateWait)) {
            ChangeState(&Ticker::StateWait);
        }
    } else if (mState) {
        (this->*mState)();
    }
}

void Ticker::StateWait() {
    switch (mPhase) {
    case 0:
        mPhase++;
        mMode = MODE_WAIT;
        mScrollPos = 0.0f;
        mScrollVelocity = 0.0f;
        mScrollTargetVelocity = 0.0f;
        break;
    case -1:
        break;
    default:
        if (mTextWidth > GetScrollWidth() && mHover) {
            ChangeState(&Ticker::StateScroll);
        }
        break;
    }
}

void Ticker::StateScroll() {
    switch (mPhase) {
    case 0:
        mPhase++;
        mMode = MODE_SCROLL;
        mTimer = 20;
        mScrollTargetVelocity = mScrollVelocity = 0.0f;
        break;
    case -1:
        break;
    default:
        if (!mHover) {
            ChangeState(&Ticker::StateReturn);
        } else {
            Chase(&mScrollVelocity, mScrollTargetVelocity, 0.2f);
            mScrollPos += mScrollVelocity;
            switch (mPhase) {
            case 1:
                if (mTimer != 0) {
                    mTimer--;
                } else {
                    mPhase++;
                    mScrollTargetVelocity = -2.0f;
                }
                break;
            case 2:
                if (mScrollPos < GetScrollWidth() - mTextWidth) {
                    mPhase++;
                    mTimer = 60;
                    mScrollTargetVelocity = 0.0f;
                }
                break;
            }
        }
        break;
    }
}

void Ticker::StateReturn() {
    switch (mPhase) {
    case 0:
        mPhase++;
        mMode = MODE_RETURN;
        mScrollTargetVelocity = mScrollVelocity = 0.0f;
        break;
    case -1:
        break;
    default:
        if (IsNearlyZero(Ease(&mScrollPos, 0.0f, 0.1f, 16.0f, 0.5f))) {
            mScrollPos = 0.0f;
            if (mHover) {
                ChangeState(&Ticker::StateScroll);
            } else {
                ChangeState(&Ticker::StateWait);
            }
        }
        break;
    }
}

void Ticker::SetLayout(math::VEC2& pos, f32 scale) {
    if (mArticle->GetTexture()) {
        mThumbLeft = unk24 - 80.0f;
        mThumbTop = pos.y;
        const NewsTexture* tex = mArticle->GetTexture();
        u16 w = tex->width;
        if (w < mArticle->GetTexture()->height) {
            mThumbScale = 80.0f / mArticle->GetTexture()->height;
            mThumbPosX = mThumbLeft + 0.5f * (80.0f - mThumbScale * mArticle->GetTexture()->width);
            mThumbPosY = mThumbTop;
        } else {
            mThumbScale = 80.0f / mArticle->GetTexture()->width;
            mThumbPosY = mThumbTop + 0.5f * (80.0f - mThumbScale * mArticle->GetTexture()->height);
            mThumbPosX = mThumbLeft;
        }
    }

    mTextScale = scale;
    unk60 = pos.y;
    mTextX = pos.x;
    mTextY = pos.y;
    unk64 = unk60 + CalcRowHeight(mTextScale);
    pos.y = 5.0f + unk64;
}

void Ticker::Dummy() {}

void Ticker::GetOrigin(math::VEC2& out) {
    out.x = sOrigin[0];
    out.y = mTextY;
}

void Ticker::TruncateText() {
    const ut::Font* font = mWriter->GetFont();
    f32 s = mFontScale * mTextScale;
    f32 s2 = mFontScale * mTextScale;
    f32 space = s * gCharSpaceScale;
    const wchar_t* src = mArticle->mHeadline;
    wchar_t* dst = mArticle->mShortHeadline;
    u32 i = 0;
    f32 width = 0.0f;
    f32 maxWidth = mViewWidth - 30.0f * s;

    mWriter->SetCharSpace(space);
    mWriter->SetScale(s);

    if (gLanguage == 0) {
        while (*src != 0) {
            *dst = *src++;
            f32 w;
            if (i < mNumChars) {
                w = s * font->GetCharWidth(*dst);
            } else {
                w = s2 * font->GetCharWidth(*dst);
            }
            width += w;
            *++dst = 0;
            if (width > maxWidth) {
                f32 ellipsis;
                if (i < mNumChars) {
                    ellipsis = s * font->GetCharWidth(0x2026);
                } else {
                    ellipsis = s * font->GetCharWidth(0x2026);
                }
                dst[-1] = 0;
                dst -= 2;
                f32 removed = s * font->GetCharWidth(*dst);
                while (removed < ellipsis) {
                    removed += space + s * font->GetCharWidth(*--dst);
                }
                dst[0] = 0x2026;
                dst[1] = 0;
                return;
            }
            width += space;
            i++;
        }
    } else {
        while (*src != 0) {
            *dst = *src++;
            f32 w;
            if (i < mNumChars) {
                w = s * font->GetCharWidth(*dst);
            } else {
                w = s2 * font->GetCharWidth(*dst);
            }
            width += w;
            *++dst = 0;
            if (width > maxWidth) {
                f32 dots;
                if (i < mNumChars) {
                    dots = 2.0f * space + s * (3.0f * font->GetCharWidth('.'));
                } else {
                    dots = 2.0f * space + s * (3.0f * font->GetCharWidth('.'));
                }
                dst[-1] = 0;
                dst -= 2;
                f32 removed = s * font->GetCharWidth(*dst);
                while (removed < dots) {
                    removed += space + s * font->GetCharWidth(*--dst);
                }
                dst[0] = 0;
                wcscat(mArticle->mShortHeadline, L"...");
                return;
            }
            width += space;
            i++;
        }
    }
}

f32 Ticker::CalcTextWidth(const wchar_t* str) {
    const ut::Font* font = mWriter->GetFont();
    f32 scale = mWriter->GetScaleH();
    f32 space = mWriter->GetCharSpace();
    u32 len = wcslen(str);
    f32 width = 0.0f;
    u32 i = 0;

    for (; *str != 0; str++, i++) {
        if (i >= mNumChars) {
            width += scale * font->GetCharWidth(*str);
        } else {
            width = width + scale * font->GetCharWidth(*str);
        }
    }

    if (space) {
        if (len >= mNumChars) {
            u32 n = mNumChars - 1;
            len -= n;
            width += space * n;
        }
        width += space * (len - 1);
    }
    return width;
}
