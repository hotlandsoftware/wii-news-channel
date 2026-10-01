#include <news/HeadlineList.h>
#include <news/Common.h>
#include <news/Draw2D.h>
#include <news/MathUtil.h>
#include <news/Message.h>
#include <news/NewsData.h>
#include <news/System.h>
#include <news/TextButton.h>
#include <news/Ticker.h>
#include <revolution/os.h>
#include <wchar.h>

using namespace nw4r;

static math::VEC2 sItemOffset(2.0f, 12.0f);

inline const wchar_t* GetLocalizedMsg(const wchar_t** table) {
    switch (gLanguage) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
        return table[gLanguage];
    default:
        return table[1];
    }
}

inline const wchar_t* GetChooseLanguageMsg() {
    return GetLocalizedMsg(gMsgChooseLanguage);
}

inline void HeadlineList::UpdatePosition() {
    mBasePos.x = mPos.x;
    mBasePos.y = mPos.y;
    f32 y = mBasePos.y + sItemOffset.y;
    f32 x = mBasePos.x + sItemOffset.x;
    mOrigin.y = y;
    mOrigin.x = x;
}

inline void HeadlineList::LayoutItems() {
    if (mNumItems != 0) {
        Ticker* item = mItems;
        math::VEC2 pos(0.0f, 0.0f);
        for (s32 i = 0; i < mNumItems; i++, item++) {
            f32 height = item->SetLayout(pos, mScale);
            item->Dummy();
            pos.y += height;
        }
    }
}

inline void HeadlineList::UpdateTotalHeight() {
    Ticker* item = mItems;
    math::VEC2 pos(0.0f, 0.0f);
    if (mNumItems != 0) {
        for (s32 i = 0; i < mNumItems; i++, item++) {
            item->SetLayout(pos, mScale);
        }
        mTotalHeight = pos.y;
    } else {
        mTotalHeight = pos.y;
    }
}

inline void HeadlineList::ResetItemsInline() {
    if (mNumItems != 0) {
        s32 i = 0;
        Ticker* item = mItems;
        for (; i < mNumItems; i++, item++) {
            item->ResetState();
        }
    }
}

inline void HeadlineList::Refresh() {
    mLanguagePressed = false;
    UpdateTotalHeight();
    mScrollTarget = GetOffset(mIndex);
    mScroll = mScrollTarget;
    LayoutItems();
    ResetItemsInline();
}

inline void HeadlineList::RecalcTotalHeight() {
    s32 i;
    Ticker* item = mItems;
    math::VEC2 pos(0.0f, 0.0f);
    if (mNumItems != 0) {
        for (i = 0; i < mNumItems; i++, item++) {
            item->SetLayout(pos, mScale);
        }
        mTotalHeight = pos.y;
    } else {
        mTotalHeight = pos.y;
    }
}

inline void HeadlineList::JumpToIndex() {
    RecalcTotalHeight();
    f32 target = GetOffset(mIndex);
    mScrollTarget = target;
    mScroll = target;
}

inline s32 HeadlineList::GetMaxIndexInline() {
    if (mMode != MODE_SECTION && mNumSectionRows < 3) {
        return mNumItems + 2;
    }
    return mNumIndices - 2;
}

inline bool HeadlineList::IsScrolledTo(s32 index) {
    return mScroll >= GetOffset(index);
}

inline s32 HeadlineList::FindIndex() {
    s32 i = 0;
    do {
        if (IsScrolledTo(i)) {
            return i;
        }
        i++;
    } while (i < mNumIndices);
    return i;
}

inline void HeadlineList::SnapInline() {
    s32 i = FindIndex();
    s32 max = GetMaxIndexInline();
    if (mScrollVel <= 0.0f) {
        mIndex = i;
    } else {
        mIndex = i - 1;
    }
    if (mIndex > max) {
        mIndex = max;
    }
    if (mIndex < 0) {
        mIndex = 0;
    }
    mScrollTarget = GetOffset(mIndex);
}

// The constructor's refresh uses variants of the helpers above; their
// declaration order and inline depth set the stack slots and registers.
inline void HeadlineList::CalcTotalHeight() {
    RecalcTotalHeight();
}

inline void HeadlineList::RelayoutItems() {
    if (mNumItems != 0) {
        s32 i;
        Ticker* item = mItems;
        math::VEC2 pos(0.0f, 0.0f);
        for (i = 0; i < mNumItems; i++, item++) {
            f32 height = item->SetLayout(pos, mScale);
            item->Dummy();
            pos.y += height;
        }
    }
}

inline void HeadlineList::ClearItemStates() {
    if (mNumItems != 0) {
        Ticker* item = mItems;
        for (s32 i = 0; i < mNumItems; i++, item++) {
            item->ResetState();
        }
    }
}

HeadlineList::HeadlineList(Category* category, ut::TextWriterBase<wchar_t>* writer,
                           math::VEC2& pos, math::VEC2& size, s32 pageIndex)
    : mCategory(category),
      mWriter(writer),
      mItems(NULL),
      mVisibleItems(NULL),
      mSectionButton(NULL),
      mTopButton(NULL),
      mLanguageButton(NULL),
      mPos(pos),
      mSize(size),
      mScrollX(0.0f),
      mScroll(0.0f),
      mScrollXTarget(0.0f),
      mScrollTarget(0.0f),
      mScale(gTextScale),
      mTotalHeight(0.0f),
      mScrollRate(1.0f),
      mMode(MODE_TOP_EMPTY),
      mPageIndex(pageIndex),
      mNumItems(category->mNumArticles),
      mNumSectionRows(0),
      mSelected(0),
      mIndex(0),
      mNumIndices(0),
      mLanguagePressed(false) {
    s32 i;
    Ticker* item;
    if (IsErrorState()) {
        return;
    }

    UpdatePosition();

    if (mPageIndex == 0) {
        if (mNumItems == 0) {
            mMode = MODE_TOP_EMPTY;
        } else {
            mMode = MODE_TOP;
        }
        mNumSectionRows = GetSectionRowCount();
    } else {
        mMode = MODE_SECTION;
    }

    switch (mMode) {
    case MODE_TOP_EMPTY:
        mNumIndices = mNumSectionRows + 1;
        break;
    case MODE_TOP:
        mNumIndices = mNumItems + mNumSectionRows + 2;
        break;
    case MODE_SECTION:
    default:
        mNumIndices = mNumItems + 1;
        break;
    }

    if (mNumItems != 0) {
        mItems = new Ticker[mNumItems];
        if (mItems == NULL) {
            gAllocFailed = true;
            return;
        }
    }

    mSectionButton = new IconTextButton(GetMsgToSectionSelect(), math::VEC2(180.0f, 56.0f), 0,
                                        IsTopPage(), 0, 0.7f, 0);
    if (mSectionButton == NULL) {
        gAllocFailed = true;
        return;
    }
    if (IsErrorState()) {
        return;
    }

    mTopButton = new IconTextButton(GetMsgToTop(), math::VEC2(180.0f, 60.0f), 0, IsTopPage(),
                                    0, 0.7f, 2);
    if (mTopButton == NULL) {
        gAllocFailed = true;
        return;
    }
    if (IsErrorState()) {
        return;
    }

    if (mMode != MODE_SECTION && gLanguageSelectable) {
        const wchar_t* label = GetChooseLanguageMsg();
        mLanguageButton = new SmallTextButton(label, math::VEC2(300.0f, 60.0f), 0, true, 0, 0.7f);
        if (mLanguageButton == NULL) {
            gAllocFailed = true;
            return;
        }
        if (IsErrorState()) {
            return;
        }
    }

    item = mItems;
    if (mNumItems != 0) {
        NewsArticle** article = mCategory->mArticles;
        for (i = 0; i < mNumItems; i++, item++, article++) {
            item->Init(*article, mWriter, mOrigin.x, mOrigin.y, mSize.x, mSize.y);
        }
    }

    s32 time = gNewsData->mHeader->mTimestamp;
    if (gLanguage == 0) {
        time += 9 * 60;
    } else {
        time = gCurrentTime - time;
        if (time < 0) {
            time = 0;
        }
    }

    switch (gLanguage) {
    case 1:
        if (gUpdateMsgType == 1) {
            FormatElapsedA_EN(time, mTitle, 256);
        } else {
            FormatElapsedB_EN(time, mTitle, 256);
        }
        break;
    case 2:
        FormatElapsedB_DE(time, mTitle, 256);
        break;
    case 3:
        if (gUpdateMsgType == 1) {
            FormatElapsedA_FR(time, mTitle, 256);
        } else {
            FormatElapsedB_FR(time, mTitle, 256);
        }
        break;
    case 4:
        if (gUpdateMsgType == 1) {
            FormatElapsedA_ES(time, mTitle, 256);
        } else {
            FormatElapsedB_ES(time, mTitle, 256);
        }
        break;
    case 5:
        FormatElapsedB_IT(time, mTitle, 256);
        break;
    case 6:
        FormatElapsedB_NL(time, mTitle, 256);
        break;
    case 0: {
        OSCalendarTime cal;
        MinutesToCalendarTime(time, &cal);
        // "%d月%d日(%ls) %d時のニュース"
        swprintf(mTitle, 256, L"%d\x6708%d\x65e5(%ls) %d\x6642\x306e\x30cb\x30e5\x30fc\x30b9",
                 cal.mon + 1, cal.mday, gMsgWeekday[gLanguage][cal.wday], cal.hour);
        break;
    }
    default:
        FormatElapsedB_EN(time, mTitle, 256);
        break;
    }

    LayoutItems();
    mLanguagePressed = false;
    CalcTotalHeight();
    mScrollTarget = GetOffset(mIndex);
    mScroll = mScrollTarget;
    RelayoutItems();
    ClearItemStates();
}

HeadlineList::~HeadlineList() {
    if (mLanguageButton != NULL) {
        delete mLanguageButton;
        mLanguageButton = NULL;
    }
    if (mTopButton != NULL) {
        delete mTopButton;
        mTopButton = NULL;
    }
    if (mSectionButton != NULL) {
        delete mSectionButton;
        mSectionButton = NULL;
    }
    if (mItems != NULL) {
        delete[] mItems;
    }
}

void HeadlineList::SetScale(const f32& scale) {
    mScale = scale;
    Refresh();
}

void HeadlineList::ResetItems() {
    ResetItemsInline();
}

void HeadlineList::Draw(f32 alpha, f32 headerAlpha, const f32& offsetX) {
    math::VEC3 line[2];
    ut::Rect rect;
    ut::Color lineColor = gSeparatorColor;
    math::VEC2 basePos(offsetX + (mOrigin.x + mScrollX), mOrigin.y + mScroll);
    u8 a = 255.0f * alpha;
    u8 headerA = 255.0f * headerAlpha;
    f32 contentW = GetContentWidth();
    ut::Color barColor(48, 72, 54, a);
    f32 barH = 45.0f;
    f32 halfBarH = 0.5f * barH;
    line[1].z = 0.0f;
    line[0].z = 0.0f;
    ut::Color white(255, 255, 255, a);
    lineColor.a = a;

    u32 drawFlag = mWriter->GetDrawFlag();
    f32 left = mPos.x - GetSideMargin();
    f32 headerY = basePos.y - 161.0f;
    rect.left = 0.0f;
    rect.top = 0.0f;
    rect.right = 0.0f;
    rect.bottom = 0.0f;

    Draw2D_SetupGX();
    Draw2D_SetOrtho();

    if (mMode != MODE_SECTION) {
        ut::Color shade(255, 255, 255, 128.0f * alpha);
        ut::Color texColor;
        texColor.r = 255;
        texColor.g = 255;
        texColor.b = 255;
        texColor.a = 96.0f * alpha;
        rect.left = line[0].x = basePos.x + offsetX - 5.0f;
        rect.right = line[1].x = line[0].x + contentW;
        rect.top = line[0].y = line[1].y = headerY;
        rect.bottom = 46.0f + headerY;
        Draw2D_FillRect(&rect, &shade);
        rect.top = rect.bottom;
        rect.bottom = 22.0f + rect.bottom;
        Draw2D_FillRect(&rect, &barColor);
        Draw2D_SetupGX();
        GXSetTevColor(GX_TEVREG0, texColor);
        Draw2D_TexRect(gCommonTpl, 4, &rect, 0.0f, 0);
        Draw2D_SetupGX();
        Draw2D_Line(line[0], line[1], 12, barColor, barColor);

        f32 maxW = GetContentRight() - GetSideMargin();
        ut::TextWriterBase<wchar_t> writer;
        writer.SetFont(*gHeaderFont);
        writer.SetDrawFlag(0x111);
        writer.SetupGX();
        f32 x = line[0].x + 0.5f * contentW;
        f32 y = 23.0f + line[0].y;
        writer.SetTextColor(ut::Color(0, 0, 0, a));
        writer.SetScale(1.0f);
        writer.SetCharSpace(0.0f);
        writer.SetCursor(x, y);
        f32 width = writer.CalcStringWidth(mTitle);
        if (width > maxW) {
            writer.SetScale(0.8f * (maxW / width));
        }
        writer.Print(mTitle);

        y = 11.0f + rect.top;
        writer.SetFont(*gSysFont);
        writer.SetupGX();
        writer.SetTextColor(ut::Color(255, 255, 255, a));
        writer.SetScale(0.5f);
        writer.SetCharSpace(0.0f);
        writer.SetCursor(x, y);
        writer.Print(gMsgNewsChannel[gLanguage]);
    }

    Draw2D_SetupGX();

    if (mMode != MODE_TOP_EMPTY) {
        ut::Color barWhite(255, 255, 255, headerA);
        ut::Color barBlue(18, 162, 192, headerA);
        rect.left = 0.0f;
        rect.right = GetScreenWidth();
        rect.top = mSectionButton->GetRect().top;
        rect.bottom = 45.0f + rect.top;
        GXSetTevColor(GX_TEVREG0, barWhite);
        Draw2D_TexRect(gCommonTpl, 1, &rect, 0.0f, 0);
        rect.top = rect.bottom;
        rect.bottom = 11.0f + rect.bottom;
        GXSetTevColor(GX_TEVREG0, barBlue);
        Draw2D_TexRect(gCommonTpl, 1, &rect, 0.0f, 0);
    }

    if (mMode != MODE_SECTION) {
        rect.left = left;
        f32 w;
        if (mMode != MODE_TOP) {
            w = GetScreenWidth();
        } else {
            w = GetContentWidth() - mSectionButton->GetWidth();
        }
        rect.right = rect.left + w;
        rect.top = mTopButton->GetRect().top;
        rect.bottom = 56.0f + rect.top;
        GXSetTevColor(GX_TEVREG0, barColor);
        Draw2D_TexRect(gCommonTpl, 2, &rect, 0.0f, 0);
        GXSetTevColor(GX_TEVREG0, white);
        Draw2D_TexRectTiled(gCommonTpl, 3, &rect, 0.0f, 0);
    }

    mWriter->SetFont(*gSysFont);
    mWriter->SetDrawFlag(0x100);
    mWriter->SetupGX();

    math::VEC2 textPos(GetSideMargin(), basePos.y - 61.0f);
    f32 maxW = mSectionButton->GetRect().left - 10.0f - basePos.x;

    if (mMode != MODE_TOP_EMPTY) {
        mWriter->SetTextColor(ut::Color(0, 0, 0, headerA));
        mWriter->SetScale(1.0f);
        mWriter->SetCharSpace(0.0f);
        f32 width = mWriter->CalcStringWidth(mCategory->mName);
        if (maxW < width) {
            mWriter->SetScale(maxW / width, 1.0f);
        }
        mWriter->SetCursor(textPos.x, halfBarH + mSectionButton->GetRect().top);
        mWriter->Print(mCategory->mName);
    }

    if (mMode != MODE_SECTION) {
        mWriter->SetTextColor(ut::Color(255, 255, 255, a));
        mWriter->SetScale(0.8f);
        mWriter->SetCharSpace(0.0f);
        f32 width = mWriter->CalcStringWidth(GetMsgSectionSelect());
        if (maxW < width) {
            mWriter->SetScale(maxW / width, 1.0f);
        }
        mWriter->SetCursor(basePos.x, 28.0f + mTopButton->GetRect().top);
        mWriter->Print(GetMsgSectionSelect());

        Draw2D_SetupGX();
        if (mNumItems != 0) {
            line[0].x = basePos.x + offsetX - 5.0f;
            line[1].x = line[0].x + contentW;
            line[0].y = line[1].y = 56.0f + textPos.y;
            Draw2D_Line(line[0], line[1], 6, lineColor, lineColor);
        }
        if (mLanguageButton != NULL) {
            line[0].x = 0.0f;
            line[1].x = GetScreenWidth();
            line[0].y = line[1].y = mLanguageButton->GetRect().top - 5.0f;
            Draw2D_Line(line[0], line[1], 12, lineColor, lineColor);
        }
        if (mNumItems != 0) {
            mSectionButton->Draw(alpha);
            mTopButton->Draw(alpha);
        }
        if (mLanguageButton != NULL) {
            mLanguageButton->Draw(alpha);
        }
    }

    Draw2D_SetupGX();
    for (Ticker* item = mVisibleItems; item != NULL; item = item->GetNext()) {
        item->Draw(basePos, offsetX, mScale, alpha);
    }

    Draw2D_SetupGX();
    GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
    for (Ticker* item = mVisibleItems; item != NULL; item = item->GetNext()) {
        item->DrawSeparator(offsetX, alpha);
    }

    mWriter->SetDrawFlag(drawFlag);
    mWriter->SetFont(*gArticleFont);
}

s32 HeadlineList::Update(const f32& offsetX, f32 scale, f32 scaleDelta,
                         const bool* dragging) {
    mLanguagePressed = false;

    if (mMode != MODE_SECTION) {
        if (mTopButton->GetPressedChan() >= 0) {
            PlaySE(0x29);
            mIndex = 0;
            mScrollTarget = GetOffset(0);
        }
        if (mSectionButton->GetPressedChan() >= 0) {
            PlaySE(0x28);
            mIndex = GetMaxIndexInline();
            mScrollTarget = GetOffset(mIndex);
        }
        if (mLanguageButton != NULL && mLanguageButton->GetPressedChan() >= 0) {
            PlaySE(0x21);
            mLanguagePressed = true;
        }
    }

    mScale = scale;
    if (!IsNearlyZero(scaleDelta)) {
        mScrollRate = 1.0f;
        JumpToIndex();
    } else {
        Ease(&mScrollRate, 0.2f, 0.1f, 1.0f, 0.01f);
        RecalcTotalHeight();
    }

    bool drag = false;
    if (dragging != NULL) {
        for (s32 i = 0; i < 4; i++) {
            if (dragging[i]) {
                drag = true;
            }
        }
    }

    if (drag) {
        mScroll += mScrollVel;
        if (mScrollVel < 0.0f) {
            f32 limit = GetOffset(GetMaxIndexInline());
            if (mScroll <= limit) {
                mScroll = limit;
                mScrollVel = 0.0f;
            }
        } else if (mScrollVel > 0.0f) {
            f32 limit = GetOffset(0);
            if (mScroll >= limit) {
                mScroll = limit;
                mScrollVel = 0.0f;
            }
        }
        SnapInline();
    } else {
        Ease(&mScroll, mScrollTarget, mScrollRate, 15.0f, 1.0f);
    }

    UpdatePosition();
    math::VEC2 pos(mOrigin.x + mScrollX, mOrigin.y + mScroll);
    mVisibleItems = NULL;

    if (mNumItems != 0) {
        Ticker* item = mItems;
        s32 i;
        mWriter->SetFont(*gSysFont);
        mWriter->SetupGX();
        for (i = 0; i < mNumItems; i++, item++) {
            item->Layout(pos);
        }
        mWriter->SetFont(*gArticleFont);
        mWriter->SetupGX();

        Ticker* last = NULL;
        item = mItems;
        for (i = 0; i < mNumItems; i++, item++) {
            if (!item->mHidden) {
                if (mVisibleItems == NULL) {
                    mVisibleItems = item;
                    last = item;
                } else {
                    last->SetNext(item);
                    item->SetPrev(last);
                    last = item;
                }
            }
        }

        mSelected = -1;
        item = mItems;
        for (i = 0; i < mNumItems; i++, item++) {
            u32 pointed = item->UpdateHover(pos);
            if (!item->mHidden && pointed != 0) {
                for (s32 chan = 0; chan < 4; chan++) {
                    if ((pointed & (1 << chan)) && (gTrig[chan] & 0x800)) {
                        mSelected = i;
                        break;
                    }
                }
            }
            item->Update();
        }
    }

    pos.x += offsetX;
    UpdateButtons(pos);
    return mSelected;
}

void HeadlineList::ScrollUp() {
    f32 target = GetOffset(mIndex);
    s32 i = FindIndex();
    while (i > 0) {
        i--;
        target = GetOffset(i);
        if (i == 0) {
            mIndex = i;
            break;
        }
        if (__fabsf(target - mScroll) > 76.0f) {
            mIndex = i;
            break;
        }
    }
    mScrollTarget = target;
}

void HeadlineList::ScrollDown() {
    s32 max = GetMaxIndexInline();
    f32 target = GetOffset(mIndex);
    s32 i = FindIndex();
    while (i < max) {
        i++;
        target = GetOffset(i);
        if (i == max) {
            mIndex = i;
            break;
        }
        if (__fabsf(target - mScroll) > 76.0f) {
            mIndex = i;
            break;
        }
    }
    mScrollTarget = target;
}

f32 HeadlineList::GetOffset(s32 index) {
    if (index == 0) {
        switch (mMode) {
        case MODE_TOP_EMPTY:
        case MODE_TOP:
            return 161.0f;
        case MODE_SECTION:
        default:
            return 61.0f;
        }
    }

    f32 y;
    s32 i = index - 1;
    switch (mMode) {
    case MODE_TOP_EMPTY:
        y = -60 * (1 - i);
        break;
    case MODE_TOP:
        if (i < mNumItems) {
            y = mItems[i].unk60;
        } else {
            s32 last = mNumItems - 1;
            f32 bottom = mItems[last].unk64;
            y = 95.0f + bottom;
            s32 rows = i - mNumItems;
            if (rows > 0) {
                y += rows * 60;
            }
        }
        break;
    case MODE_SECTION:
    default:
        y = mItems[i].unk60;
        break;
    }
    return -y;
}

f32 HeadlineList::GetTopButtonBottom() {
    return mTopButton->GetRect().bottom;
}

void HeadlineList::Snap() {
    SnapInline();
}

void HeadlineList::UpdateButtons(math::VEC2 pos) {
    f32 rowH = 35.0f;
    f32 gridOffset = 155.0f;
    math::VEC2 btnPos(pos.x + (GetContentWidth() - 0.5f * mSectionButton->GetWidth() - 5.0f),
                      pos.y - rowH);
    mSectionButton->Update(btnPos);

    if (mMode != MODE_SECTION) {
        if (mMode == MODE_TOP) {
            s32 last = mNumItems - 1;
            btnPos.y = pos.y + (gridOffset + mItems[last].unk64 - rowH);
        }
        mTopButton->Update(btnPos);
        if (mLanguageButton != NULL) {
            btnPos.x = pos.x + 0.5f * GetContentWidth() - 5.0f;
            btnPos.y = btnPos.y + (50.0f + 60.0f * mNumSectionRows);
            mLanguageButton->Update(btnPos);
        }
    }
}

s32 HeadlineList::GetMaxIndex() {
    return GetMaxIndexInline();
}
