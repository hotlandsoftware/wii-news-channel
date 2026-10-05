#define NW4R_UT_COLOR_DEFAULT_WHITE
#define NW4R_UT_COLOR_WORD_COPY
#include <news/ArticleText.h>
#include <news/Common.h>
#include <news/Draw2D.h>
#include <news/MathUtil.h>
#include <news/NewsArticle.h>
#include <news/NewsData.h>
#include <news/System.h>
#include <nw4r/ut/ut_CharWriter.h>
#include <nw4r/ut/ut_Font.h>
#include <nw4r/ut/ut_TextWriterBase.h>
#include <revolution/mem.h>
#include <wchar.h>

using namespace nw4r;

void* operator new[](u32 size, MEMAllocator* allocator);

void SetupTexGX();  // d_s_news.cpp: sets up GX for Draw2D_Texture

extern "C" ArticleText* lbl_80357574;  // the shared caption text
extern "C" f32 lbl_803575CC;           // extra line spacing
extern "C" s32 lbl_80356970;           // text size setting

// Picture size limits per text size setting.
extern "C" const f32 lbl_801922F8[];  // max width
extern "C" const f32 lbl_80192320[];  // max width of a small picture
extern "C" const f32 lbl_80192348[];  // max height

// Characters that may not start a line (Japanese).
extern const wchar_t gPunctuationTable[];

static ut::Color sTextColor(0, 0, 0, 255);
static ut::Color sSelectColor(178, 0, 0, 255);

#pragma explicit_zero_data on
static f32 sStartX = 0.0f;
static f32 sStartY = 0.0f;
#pragma explicit_zero_data reset

static wchar_t sNoBreakBeforeChars[] = {
    0x3001, 0x3002, 0xFF0C, 0xFF0E, 0xFF09, 0x3015, 0xFF3D, 0xFF5D, 0x3009, 0x300B, 0x300D,
    0x300F, 0x3011, 0xFF1E, 0x226B, 0x2019, 0x201D, 0x309D, 0x309E, 0x30FD, 0x30FE, 0x3005,
    0xFF1F, 0xFF01, 0xFF1A, 0xFF1B, 0x30FB, 0xFF0D, 0x2015, 0x2026, 0x2192, 0x2190, 0x2191,
    0x2193, 0x30FC, 0x002D, 0x3041, 0x3043, 0x3045, 0x3047, 0x3049, 0x3083, 0x3085, 0x3087,
    0x308E, 0x30A1, 0x30A3, 0x30A5, 0x30A7, 0x30A9, 0x30E3, 0x30E5, 0x30E7, 0x30EE, 0x002C,
    0x002E, 0x0029, 0x005D, 0x007D, 0x003E, 0x0027, 0x0022, 0x003F, 0x0021, 0x003A, 0x003B,
    0x0000,
};

static wchar_t sNoBreakAfterChars[] = {
    0xFF08, 0x3014, 0xFF3B, 0xFF5B, 0x3008, 0x300A, 0x300C, 0x300E, 0x3010, 0xFF1C,
    0x226A, 0x2018, 0x201C, 0x0028, 0x005B, 0x007B, 0x003C, 0x0060, 0x0000,
};

static wchar_t sUnitChars[] = {
    0x3349, 0x3314, 0x3322, 0x334D, 0x3318, 0x3327, 0x3303, 0x3336, 0x3351, 0x3357,
    0x330D, 0x3326, 0x3323, 0x332B, 0x334A, 0x333B, 0x339C, 0x339D, 0x339E, 0x338E,
    0x338F, 0x33C4, 0x33A1, 0x982D, 0x7FBD, 0x672C, 0x5339, 0x56DE, 0x500B, 0x4EBA,
    0x5E74, 0x6708, 0x65E5, 0x6642, 0x5206, 0x79D2, 0x5186, 0x6B73, 0x624D, 0x92AD,
    0xFF20, 0x2103, 0xFF05, 0xFF27, 0xFF47, 0xFF2D, 0xFF4D, 0xFF04, 0x0040, 0x0025,
    0x0047, 0x0067, 0x004D, 0x006D, 0x0024, 0x0000,
};

static wchar_t sSpaceChars[] = {0x0020, 0x3000, 0x000D, 0x000A, 0x0009, 0x0000};

static const wchar_t* sNoBreakBefore = sNoBreakBeforeChars;
static const wchar_t* sNoBreakAfter = sNoBreakAfterChars;
static const wchar_t* sUnits = sUnitChars;
static const wchar_t* sSpaces = sSpaceChars;

static inline bool IsInList(const wchar_t* list, wchar_t c) {
    for (; *list != 0; list++) {
        if (*list == c) {
            return true;
        }
    }
    return false;
}

static inline bool IsAlpha(wchar_t c) {
    if (c >= 0xFF21 && c <= 0xFF3A) {
        return true;
    }
    if (c >= 0xFF41 && c <= 0xFF5A) {
        return true;
    }
    if (c >= L'A' && c <= L'Z') {
        return true;
    }
    if (c >= L'a' && c <= L'z') {
        return true;
    }
    return false;
}

static inline bool IsDigit(wchar_t c) {
    if (c >= L'0' && c <= L'9') {
        return true;
    }
    if (c >= 0xFF10 && c <= 0xFF19) {
        return true;
    }
    return false;
}

static inline bool IsNoBreakBefore(wchar_t c) {
    for (const wchar_t* s = sNoBreakBefore; *s != 0; s++) {
        if (*s == c) {
            return true;
        }
    }
    for (s32 i = 0; i < 39; i++) {
        if (c == gPunctuationTable[i]) {
            return true;
        }
    }
    return false;
}

static inline void Deselect(TextChar* c) {
    c->mColor = sTextColor;
    c->mSelected = false;
}

static inline void Select(TextChar* c) {
    c->mColor = sSelectColor;
    c->mSelected = true;
}

ArticleText::ArticleText(MEMAllocator* allocator, ut::CharWriter* writer, s32 maxChars,
                         const math::VEC2& size, f32 scale)
    : mAllocator(allocator), mWriter(writer), mFont(writer->GetFont()), mChars(NULL),
      mPicture(NULL), mText(NULL), mPicLabel(NULL), mSub(NULL), mSize(size) {
    mPicPos.x = 0.0f;
    mPicPos.y = 0.0f;
    mPicTarget.x = 0.0f;
    mPicTarget.y = 0.0f;
    mPicSize.x = 1.0f;
    mPicSize.y = 1.0f;
    mSubPos.x = 0.0f;
    mSubPos.y = 0.0f;
    mScale = scale;
    mHeight = 0.0f;
    mRevealRate = 1.0f;
    mPicScale = 1.0f;
    mPicTargetScale = 1.0f;
    mFontScale = 1.0f;
    mLineHeight = 0.0f;
    mBaseLineHeight = 0.0f;
    mTextTop = 0.0f;
    mLabelScale = 1.0f;
    mUnk80 = 0.0f;
    mMaxChars = maxChars;
    mLength = 0;
    mCount = 0;
    mFirstVisible = 0;
    mLastVisible = 0;
    mLastFull = 0;
    mNumLines = 0;
    mRevealPos = 0;
    mRevealWait = 0;
    mIndentFirst = true;
    mPicWrapped = false;
    mIsCaption = false;
    mTruncated = false;

    if (IsErrorState()) {
        return;
    }

    mChars = new (mAllocator) TextChar[mMaxChars];
    if (mChars == NULL) {
        gAllocFailed = true;
        return;
    }
}

ArticleText::~ArticleText() {
    if (mChars != NULL) {
        TextChar* c = mChars;
        for (s32 i = 0; i < mMaxChars; i++, c++) {
            c->~TextChar();
        }
    }
}

bool ArticleText::IsNoBreak(const wchar_t* p, const wchar_t* start) {
    wchar_t next = p[1];
    const wchar_t* prev = NULL;

    if (next == 0) {
        return false;
    }

    if (gLanguage != 0) {
        wchar_t c = p[0];
        s32 i = 0;
        while (sSpaces[i] != 0) {
            wchar_t sp = sSpaces[i];
            if (sp == c) {
                return false;
            }
            i++;
        }
        return true;
    }

    wchar_t c = p[0];
    switch (c) {
    case 0x3000:
    case ' ':
        return false;
    }

    if (next == L'\n') {
        return true;
    }

    if (p != start) {
        prev = p - 1;
    }

    if (IsInList(sNoBreakAfter, c)) {
        return true;
    }

    if (IsNoBreakBefore(next)) {
        return true;
    }

    if (IsAlpha(c) && IsAlpha(next)) {
        return true;
    }

    if (IsDigit(c)) {
        if (IsDigit(next)) {
            return true;
        }
        if (IsInList(sUnits, next)) {
            return true;
        }
        switch (next) {
        case 0xFF23:
        case 'c':
        case 'C':
        case 0xFF43:
            switch (p[2]) {
            case 0x33A1:
            case 'm':
            case 'M':
            case 0xFF4D:
            case 0xFF2D:
                return true;
            }
        case 'K':
        case 'k':
        case 0xFF2B:
        case 0xFF4B:
            switch (p[2]) {
            case 0x33A1:
            case 'g':
            case 'M':
            case 'G':
            case 'm':
            case 0xFF47:
            case 0xFF2D:
            case 0xFF27:
            case 0xFF4D:
                return true;
            }
            break;
        }
    }

    switch (c) {
    case ',':
    case '.':
    case 0xFF0E:
    case 0xFF0C:
        if (prev != NULL && IsDigit(*prev) && IsDigit(next)) {
            return true;
        }
        return false;
    case '$':
    case '\\':
    case 0xFF04:
    case 0x20AC:
    case 0xFFE0:
    case 0xFFE1:
    case 0xFFE5:
        return IsDigit(next);
    case '@':
    case 0xFF20:
        if (IsDigit(next) || IsAlpha(next)) {
            return true;
        }
        return false;
    case 0x2026:
        return next == 0x2026;
    case 0x2025:
        return next == 0x2025;
    case 0x2015:
        return next == 0x2015;
    }
    return false;
}

bool ArticleText::Set(const wchar_t* text, NewsPicture* picture, const math::VEC2* start,
                      const math::VEC2* picPos, const f32* picScale, const math::VEC2* size,
                      bool indent, f32 scale, f32 fontScale, bool smallPicture, bool isCaption) {
    if (text == NULL) {
        return false;
    }

    mIsCaption = isCaption;
    if (isCaption && scale > 0.7f) {
        scale = 0.7f;
    }

    mFontScale = fontScale;
    SetSize(size);
    mScale = scale;
    mNumLines = 0;
    mText = text;
    mPicture = picture != NULL ? picture->texture : NULL;

    s32 len = wcslen(mText) + 1;
    mIndentFirst = indent;
    const wchar_t* p = mText;
    s32 word = 0;
    mLength = len;
    mCount = len;
    mRevealPos = len;
    mSmallPicture = smallPicture;

    TextChar* c = mChars;
    for (s32 i = 0; i < mMaxChars; i++, c++) {
        c->mNext = NULL;
        c->mPrev = NULL;
        c->mChar = 0;
        c->mHidden = false;
    }

    c = mChars;
    for (; *p != 0; c++, p++) {
        c->mChar = *p;
        c->mScale = mFontScale;
        Deselect(c);
        c->mScaleX = 1.0f;
        c->mWordIndex = word;
        if (IsNoBreak(p, mText)) {
            c->mNext = c + 1;
            c[1].mPrev = c;
        } else {
            word++;
        }

        if (c->mChar == 0xA0) {
            c->mChar = L' ';
        }

        if (c->mChar == L'\n') {
            c->mWidth = 0.0f;
        } else {
            c->mWidth = mFont->GetCharWidth(c->mChar);
        }

        c->mHeight = mFont->GetHeight();
        c->mScaledWidth = c->mWidth * c->mScale;
        c->mScaledHeight = c->mScale * (c->mHeight + lbl_803575CC);

        if (c->mPrev == NULL) {
            c->mWordChar = c->mChar;
        } else {
            c->mWordChar = c->mPrev->mWordChar;
        }
    }

    c->mChar = 0;
    c->mScale = mFontScale;
    Deselect(c);
    c->mScaleX = 1.0f;
    c->mWordIndex = word + 1;

    for (c = mChars; c->mChar != 0; c++) {
        if (c->mPrev == NULL) {
            f32 space = gCharSpaceScale;
            f32 width = c->mScaledWidth;
            for (TextChar* n = c->mNext; n != NULL; n = n->mNext) {
                width += space + n->mScaledWidth;
            }
            f32 scaleX = 1.0f;
            c->mWordWidth = width * scaleX;
        }
    }

    mPicLabel = NULL;
    mSub = NULL;
    if (picture != NULL) {
        mPicLabel = picture->caption;
        if (picture->credit != NULL) {
            mSub = lbl_80357574;
            mSub->Set(picture->credit, NULL, start, picPos, picScale, size, false, 0.7f * scale,
                      0.8f, false, true);
        }
    }

    Layout(scale);

    f32 y = start->y;
    if (mCount != 0) {
        if (mPicture != NULL) {
            mPicTarget.y = y;
        }
        c = mChars;
        for (u32 i = 0; i < mCount; i++, c++) {
            c->mTarget.y = y;
        }
    }

    Snap();

    mPicPos.x = picPos->x;
    mPicPos.y = picPos->y;
    mPicScale = *picScale / mScale;
    mPicTargetScale = 1.0f;
    Layout(scale);
    return true;
}

#define DRAW_CHAR(c, yOfs)                                                                         \
    {                                                                                              \
        f32 sy = c->mScale * mScale;                                                               \
        mWriter->SetScale(mScale * (c->mScale * c->mScaleX), sy);                                  \
        f32 cy = yOfs * sy; \
        mWriter->SetCursor(pos->x + c->mPos.x, cy + (pos->y + c->mPos.y)); \
        color.r = c->mColor.r;                                                                     \
        color.g = c->mColor.g;                                                                     \
        color.b = c->mColor.b;                                                                     \
        color.a = a;                                                                               \
        mWriter->SetTextColor(color);                                                              \
    }

void ArticleText::Draw(const math::VEC2* pos, bool clip, f32 alpha, f32 zoom) {
    if (mCount == 0) {
        return;
    }

    GXColor black;
    black.r = 0;
    black.g = 0;
    black.b = 0;
    u8 a = 255.0f * alpha;
    black.a = a;
    mWriter->SetTextColor(black);
    mWriter->SetupGX();

    TextChar* c;
    s32 i = mFirstVisible;
    ut::Color color;
    c = &mChars[i];

    if (gNewsData->mHeader->unk2C[0] == 0) {
        f32 yOfs = 0.0f;
        if (clip) {
            for (; i < mLastFull; i++, c++) {
                if (!c->mHidden && c->mChar != L'\n') {
                    DRAW_CHAR(c, yOfs);
                    mWriter->Print(c->mChar);
                }
            }
            if (!c->mHidden) {
                DRAW_CHAR(c, yOfs);
                if (mTruncated) {
                    mWriter->MoveCursorY(mWriter->GetFontDescent() + 2.0f * mWriter->GetScaleH());
                    mWriter->Print(0x2026);
                } else {
                    mWriter->Print(c->mChar);
                }
            }
        } else {
            for (; i <= mLastVisible; i++, c++) {
                if (!c->mHidden && c->mChar != L'\n') {
                    DRAW_CHAR(c, yOfs);
                    mWriter->Print(c->mChar);
                }
            }
        }
    } else {
        f32 yOfs = -(f32)mFont->GetDescent();
        if (clip) {
            for (i = mFirstVisible; i < mLastFull; i++, c++) {
                if (!c->mHidden && c->mChar != L'\n') {
                    DRAW_CHAR(c, yOfs);
                    mWriter->Print(c->mChar);
                }
            }
            if (!c->mHidden) {
                DRAW_CHAR(c, yOfs);
                if (mTruncated) {
                    mWriter->MoveCursorY(mWriter->GetFontDescent() + 2.0f * mWriter->GetScaleH());
                    mWriter->Print(0x2026);
                } else {
                    mWriter->Print(c->mChar);
                }
            }
        } else {
            for (i = mFirstVisible; i <= mLastVisible; i++, c++) {
                if (!c->mHidden && c->mChar != L'\n') {
                    DRAW_CHAR(c, yOfs);
                    mWriter->Print(c->mChar);
                }
            }
        }
    }

    if (mPicture != NULL) {
        f32 grow = zoom - 1.0f;
        math::VEC3 picPos;
        picPos.x = pos->x + mPicPos.x - 0.5f * (grow * mPicScale * mPicture->width);
        picPos.y = pos->y + mPicPos.y - 0.5f * (grow * mPicScale * mPicture->height);
        picPos.z = 0.0f;
        f32 picScale = zoom * mPicScale;
        SetupTexGX();
        GXSetTevColor(GX_TEVREG0, (GXColor){255, 255, 255, a});
        Draw2D_Texture(mPicture, &picPos, picScale);

        if (mPicLabel != NULL) {
            f32 labelScale = zoom * (mFontScale * mLabelScale);
            ut::TextWriterBase<wchar_t> writer;
            writer.SetFont(*gSysFont);
            writer.SetDrawFlag(0x22);
            writer.SetupGX();
            writer.SetTextColor(black);
            writer.SetScale(labelScale);
            writer.SetCharSpace(0.0f);
            writer.SetCursor(picPos.x + picScale * mPicture->width,
                             picPos.y + picScale * mPicture->height);
            f32 width = writer.CalcStringWidth(mPicLabel);
            if (width > picScale * mPicture->width) {
                writer.SetScale(labelScale * ((picScale * mPicture->width) / width), labelScale);
            }
            writer.Print(mPicLabel);
        }
    }

    TextChar* u = &mChars[mFirstVisible];
    math::VEC3 line[2];
    line[0].z = line[1].z = line[2].z = line[3].z = 0.0f;
    f32 lineOfs = gNewsData->mHeader->unk2C[0] == 0 ? 2.0f : 0.0f;
    SetupTexGX();
    for (s32 j = mFirstVisible; j <= mLastVisible; j++, u++) {
        if (!u->mHidden && u->mSelected && u->mChar != L'\n') {
            color.r = u->mColor.r;
            color.g = u->mColor.g;
            color.b = u->mColor.b;
            color.a = a;
            line[0].x = u->mLeft;
            line[1].x = u->mRight;
            f32 y = u->mBottom - lineOfs;
            line[1].y = y;
            line[0].y = y;
            Draw2D_Line(line[0], line[1], 12, color, color);
        }
    }

    if (mSub != NULL) {
        math::VEC2 subPos(pos->x + mSubPos.x, pos->y + mSubPos.y);
        mSub->Draw(&subPos, false, alpha, 1.0f);
    }
}

void ArticleText::Update(const math::VEC2* pos, bool clip, f32 scroll) {
    mTruncated = false;
    if (mSub != NULL) {
        math::VEC2 subPos(pos->x + mSubPos.x, pos->y + mSubPos.y);
        mSub->Update(&subPos, false, 0.0f);
    }

    if (mCount == 0) {
        return;
    }

    if (mRevealWait != 0) {
        mRevealWait--;
    }

    for (s32 n = gLanguage == 0 ? 1 : 2; n > 0; n--) {
        if (mRevealPos < mLength && mRevealWait == 0) {
            mRevealWait = 0;
            mChars[mRevealPos].mHidden = false;
            mRevealPos++;
        }
    }

    if (!IsNearlyZero(scroll)) {
        mRevealRate = 1.0f;
    } else {
        Ease(&mRevealRate, 0.2f, 0.1f, 1.0f, 0.01f);
    }

    f32 height = mFont->GetHeight();
    TextChar* c = mChars;
    f32 bottom = clip ? 398.0f : 456.0f;

    Ease(&mPicPos, &mPicTarget, 0.2f, 100.0f, 0.01f);
    Ease(&mPicScale, mPicTargetScale, 0.2f, 1.0f, 0.005f);
    Ease(&mSubPos, &mSubTarget, 0.2f, 100.0f, 0.01f);

    mFirstVisible = mCount;
    mLastVisible = 0;
    s32 i;
    bool overflow = false;
    mLastFull = 0;
    for (i = 0; c->mChar != 0; c++) {
        c->Update(pos, &mRevealRate);
        f32 top = pos->y + c->mPos.y;
        f32 lineBottom = top + mLineHeight;
        if (i < mFirstVisible && lineBottom >= 0.0f) {
            mFirstVisible = i;
        }
        if (top < bottom) {
            mLastVisible = i;
        }
        if (lineBottom < bottom) {
            mLastFull = i;
        } else {
            overflow = true;
        }
        i++;
        f32 x = c->mLeft = pos->x + c->mPos.x;
        f32 y = c->mTop = pos->y + c->mPos.y;
        c->mRight = x + mScale * (c->mWidth * (c->mScale * c->mScaleX));
        f32 h = c->mScaledHeight * mScale;
        c->mBottom = h + y;
    }

    if (clip) {
        s32 dotsWidth = mFont->GetCharWidth(0x2026);
        if (mLastFull < 4) {
            if (overflow) {
                mTruncated = true;
            }
        } else {
            if (mLastFull < mCount - 2) {
                mTruncated = true;
                TextChar* t = &mChars[mLastFull];
                while (t->mChar == L' ' || t->mChar == 0x3000) {
                    t--;
                    mLastFull--;
                }
                if (mFont->GetCharWidth(t->mChar) < dotsWidth) {
                    mLastFull--;
                }
            }
            if (mLastFull < 0) {
                mLastFull = 0;
            }
        }
    }
}

void ArticleText::HideAll() {
    TextChar* c = mChars;
    mRevealPos = 0;
    mRevealWait = 0;
    for (u32 i = 0; i < mLength; i++, c++) {
        c->mHidden = true;
    }
}

static inline bool NeedsLineBreak(TextChar* last, const f32* x, f32 right, const f32& scale) {
    switch (last->mChar) {
    case '\n':
        return true;
    }
    wchar_t next = last[1].mChar;
    if (next != 0) {
        if (last[1].mPrev != NULL) {
            return false;
        }
        switch (next) {
        case 0x3000:
        case ' ':
            return false;
        }
        if (*x + scale * last[1].mWordWidth > right) {
            return true;
        }
    }
    return false;
}

bool ArticleText::LayoutPicture(const math::VEC2* pos, f32 scale) {
    if (mPicture == NULL) {
        return false;
    }

    math::VEC2 size;
    f32 texHeight;
    f32 texWidth;
    f32 aspect;
    f32 maxAspect;
    f32 maxHeight;
    f32 labelHeight;
    maxHeight = lbl_80192348[lbl_80356970];
    size.x = mSmallPicture ? lbl_80192320[lbl_80356970] : lbl_801922F8[lbl_80356970];
    maxAspect = maxHeight / size.x;
    texWidth = mPicture->width;
    texHeight = mPicture->height;
    size.y = maxHeight;
    aspect = texHeight / texWidth;

    if (mPicLabel != NULL) {
        ut::TextWriterBase<wchar_t> writer;
        writer.SetFont(*gSysFont);
        writer.SetScale(mFontScale * mLabelScale);
        writer.SetCharSpace(0.0f);
        labelHeight = 10.0f + writer.CalcStringHeight(mPicLabel);
    } else {
        labelHeight = 10.0f;
    }

    mPicTargetScale = aspect > maxAspect ? size.y / texHeight : size.x / texWidth;
    texHeight *= mPicTargetScale;
    if (size.y > texHeight) {
        size.y = texHeight;
    }
    texWidth *= mPicTargetScale;

    mPicSize = size;
    f32 left = mRight - size.x;
    mSubTarget.x = left;
    mPicTarget.y = mCursor.y;
    mWrapRight = pos->x + left - mIndent;
    size.y = size.y + labelHeight;
    mPicBottom = mCursor.y + size.y;
    mPicTarget.x = left + 0.5f * (size.x - texWidth);
    mSubTarget.y = mPicBottom;

    if (mSub != NULL) {
        mSub->SetSize(&mPicSize);
        mSub->Layout(0.7f * scale);
        mPicBottom = 10.0f + (mSubTarget.y + mSub->mLineHeight * mSub->mNumLines);
    }

    f32 space = scale * gCharSpaceScale;
    f32 right = mWrapRight;
    f32 width = right - mLeft;
    TextChar* c = mChars;
    while (c->mChar != 0) {
        if (mCursor.y > mPicBottom) {
            return false;
        }

        f32 wordWidth = scale * c->mWordWidth;
        if (mCursor.x == mLeft && wordWidth > width) {
            mPicWrapped = true;
            if (mSub != NULL) {
                mTextTop = mPicBottom;
                mPicLines = (s32)((mPicBottom - pos->y) / mLineHeight) + 1;
            } else {
                mPicLines = (s32)(size.y / mLineHeight) + 1;
                mTextTop = pos->y + mLineHeight * mPicLines;
            }
            return true;
        }

        TextChar* last;
        do {
            if (c->mNext == NULL) {
                last = c;
            }
            c = c->mNext;
        } while (c != NULL);

        mCursor.x += wordWidth + space;
        if (NeedsLineBreak(last, &mCursor.x, right, scale)) {
            mCursor.x = mLeft;
            mNumLines++;
            mCursor.y += 1.25f * (last->mScaledHeight * scale);
        }
        c = last + 1;
    }
    return false;
}

void ArticleText::Layout(f32 scale) {
    math::VEC2 origin(0.0f, 0.0f);
    Layout(&origin, scale);
}

void ArticleText::Layout(const math::VEC2* pos, f32 scale) {
    mCursor.x = sStartX;
    mCursor.y = sStartY;
    mLeft = 0.0f;
    mRight = 0.0f;
    mWrapRight = 0.0f;
    mIndent = 0.0f;
    mPicBottom = 0.0f;

    if (mCount == 0) {
        return;
    }

    if (mIsCaption && scale > 0.7f) {
        scale = 0.7f;
    }

    mBaseLineHeight = 1.25f * (((f32)mFont->GetHeight() + lbl_803575CC) * mFontScale);
    mPicLines = 0;
    mPicWrapped = false;
    mLineHeight = mBaseLineHeight * scale;
    mTextTop = pos->y;

    if (mIsCaption) {
        mIndent = mFont->GetWidth();
        f32 x = pos->x;
        mCursor.x = mLeft = x;
        mRight = x + mSize.x;
        mCursor.y = pos->y;
    } else {
        if (mIndentFirst) {
            mIndent = 0.5f * ((1.0f / scale) * mFont->GetWidth());
        } else {
            mIndent = 0.5f * (scale * mFont->GetWidth());
        }
        mCursor.x = mLeft = pos->x + mIndent;
        mRight = mLeft + (mSize.x - 2.0f * mIndent);
        mCursor.y = pos->y;
    }

    bool wrapped = false;
    mNumLines = 0;
    LayoutPicture(pos, scale);
    if (mPicture != NULL && mPicWrapped) {
        wrapped = true;
    }

    const f32* top = wrapped ? &mTextTop : &pos->y;
    mCursor.x = mLeft;
    f32 right = mRight;
    mCursor.y = *top;
    mNumLines = mPicLines;
    s32 count = 0;
    f32 scaleX;
    f32 bottom = mCursor.y + mLineHeight;

    TextChar* c = mChars;
    while (c->mChar != 0) {
        if (mPicture != NULL && !mPicWrapped) {
            right = mCursor.y <= mPicBottom ? mWrapRight : mRight;
        }

        f32 width = right - mLeft;
        f32 wordWidth = scale * c->mWordWidth;
        if (mCursor.x == mLeft && wordWidth > width) {
            scaleX = width / wordWidth;
            c = PlaceWord(c, scale, scaleX, mCursor, count);
            switch (c[1].mChar) {
            case ' ':
            case '\n':
            case 0x3000:
                break;
            default:
                mCursor.x = mLeft;
                mCursor.y += mLineHeight;
                bottom = mCursor.y + mLineHeight;
                mNumLines++;
                break;
            }
        } else {
            scaleX = 1.0f;
            c = PlaceWord(c, scale, scaleX, mCursor, count);
            if (NeedsLineBreak(c, &mCursor.x, right, scale)) {
                mCursor.x = mLeft;
                mCursor.y += mLineHeight;
                bottom = mCursor.y + mLineHeight;
                mNumLines++;
            }
        }
        c++;
    }

    mHeight = bottom;
    mNumLines++;
    if (mPicture != NULL) {
        mHeight = bottom < mPicBottom ? mPicBottom : bottom;
        s32 lines = (mHeight - pos->y) / mLineHeight;
        if (lines <= 0) {
            lines = 1;
        }
        if (lines > mNumLines) {
            mNumLines = lines;
        }
    }
}

TextChar* ArticleText::PlaceWord(TextChar* c, const f32& scale, const f32& scaleX,
                                 math::VEC2& cursor, s32& count) {
    f32 k = gCharSpaceScale;
    f32 s = scale * scaleX;
    f32 space = s * k;
    c->mScaleX = scaleX;
    c->SetTarget(cursor.x, cursor.y);
    c->SetLine(mNumLines);
    cursor.x += c->mScaledWidth * s;
    count++;
    if (c->mNext == NULL) {
        return c;
    }
    while (c->mNext != NULL) {
        TextChar* n = c->mNext;
        c = n;
        f32 sx = scaleX;
        cursor.x += space;
        n->mScaleX = sx;
        n->SetTarget(cursor.x, cursor.y);
        n->SetLine(mNumLines);
        cursor.x += n->mScaledWidth * s;
        count++;
    }
    return c;
}

inline void ArticleText::Snap() {
    if (mCount == 0) {
        return;
    }
    mPicPos.x = mPicTarget.x;
    mPicPos.y = mPicTarget.y;
    mPicScale = mPicTargetScale;
    mSubPos.x = mSubTarget.x;
    mSubPos.y = mSubTarget.y;
    for (TextChar* c = mChars; c->mChar != 0; c++) {
        c->mPos.x = c->mTarget.x;
        c->mPos.y = c->mTarget.y;
    }
    if (mSub != NULL) {
        mSub->Snap();
    }
}

void ArticleText::ResetUnk80() {
    mUnk80 = 0.0f;
}

void ArticleText::SetScale(f32 scale) {
    mScale = scale;
    mLabelScale = 0.7f * mScale;
    if (mLabelScale > 0.7f) {
        mLabelScale = 0.7f;
    }
    if (mIsCaption) {
        if (mScale > 0.7f) {
            mScale = 0.7f;
        }
    } else if (mSub != NULL) {
        mSub->SetScale(0.7f * scale);
    }
}

void ArticleText::StartScroll(const math::VEC2* pos, const math::VEC2* from,
                              const math::VEC2* picPos, const f32* picScale) {
    f32 margin = 228.0f;
    f32 top = -margin;
    TextChar* c = mChars;
    f32 bottom = margin + GetScreenHeight();
    for (u32 i = 0; i < mCount; i++, c++) {
        f32 y = pos->y + c->mPos.y;
        if (y < top || y > bottom) {
            c->mTarget.x = c->mPos.x;
            c->mTarget.y = c->mPos.y;
        } else {
            c->mTarget.y = from->y;
        }
    }
    mPicTarget.x = picPos->x;
    mPicTarget.y = picPos->y;
    mPicTargetScale = *picScale / mScale;
}

static inline void GetSelection(TextChar* c, s32 count, s32& first, s32& last) {
    s32 i;
    last = -1;
    first = -1;
    for (i = 0; i < count; i++, c++) {
        if (first < 0 && c->mSelected) {
            first = i;
        }
        if (c->mSelected) {
            last = i;
        }
    }
}

bool ArticleText::Select(const ut::Rect* rect) {
    f32 minX;
    f32 maxX;
    f32 left = rect->left;
    f32 right = rect->right;
    TextChar* c = mChars;
    if (left < right) {
        minX = left;
        maxX = right;
    } else {
        minX = right;
        maxX = left;
    }

    s32 first0, last0;
    GetSelection(c, mCount, first0, last0);

    for (s32 i = 0; i < mCount; i++, c++) {
        f32 top = rect->top;
        Deselect(c);
        if (c->mTop <= top && c->mBottom >= top && c->mTop <= rect->bottom &&
            c->mBottom >= rect->bottom) {
            if ((c->mLeft <= minX && c->mRight >= maxX) || (c->mRight >= minX && c->mLeft <= maxX)) {
                ::Select(c);
            }
        } else if (c->mTop <= rect->top && c->mBottom >= rect->top && c->mRight >= rect->left) {
            ::Select(c);
        } else if (c->mTop <= rect->bottom && c->mBottom >= rect->bottom &&
                   c->mLeft <= rect->right) {
            ::Select(c);
        } else if (c->mTop > rect->top && c->mBottom < rect->bottom) {
            ::Select(c);
        }
    }

    s32 first1, last1;
    GetSelection(mChars, mCount, first1, last1);

    return (first0 >= 0 || last0 >= 0 || first1 >= 0 || last1 >= 0) &&
           (first0 != first1 || last0 != last1);
}

void ArticleText::ClearSelection() {
    TextChar* c = mChars;
    for (s32 i = 0; i < mCount; i++, c++) {
        Deselect(c);
        f32 scale = mFontScale;
        c->mScale = scale;
        c->mScaledWidth = c->mWidth * scale;
        c->mScaledHeight = scale * (c->mHeight + lbl_803575CC);
    }
}

f32 ArticleText::GetTop() {
    if (mPicture != NULL) {
        return mPicPos.y;
    }
    return mChars->mPos.y;
}

bool ArticleText::GetPictureRect(ut::Rect* rect) const {
    if (mPicture != NULL) {
        rect->top = mPicPos.y;
        rect->bottom = mPicPos.y + mPicScale * mPicture->height;
        rect->left = mPicPos.x;
        rect->right = mPicPos.x + mPicScale * mPicture->width;
        return true;
    }
    return false;
}
