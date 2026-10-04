#include <news/PaneLayout.h>
#include <news/LanguageSelect.h>
#include <news/Common.h>
#include <news/Draw2D.h>
#include <news/MathUtil.h>
#include <news/NewsArticle.h>
#include <news/PaneButton.h>
#include <news/System.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_Font.h>

using namespace nw4r;

// Not yet decompiled: layouts, input and globals in other files.
extern u8 lbl_801EE270[];          // layout resource accessor
extern const wchar_t* lbl_801B26BC[]; // language names
extern u8 gSelectedNewsLanguage;            // selected language
extern s32 lbl_80357598;
extern u8 lbl_803575BA;
extern u8 lbl_803575BB;
extern s32 lbl_801EDFD0[4];
extern f32 lbl_801EDFA0[6];
extern f32 lbl_801EDFB8[6];
extern f32 lbl_803575D8;
extern GXColor lbl_80357600;
extern f32 gPointerScroll[];         // pointer movement

static ut::Color sFillColor(255, 255, 255, 64);
static ut::Color sTextColor(0, 0, 0, 255);

static inline void PressButton(Layout* layout, const char* name) {
    PaneButton* button = layout->FindButton(name);
    button->mToggle = true;
}

static inline void DisableButton(PaneButton* button) {
    button->mDisabled = true;
    button->Press();
}

LanguageSelect::LanguageSelect(u32 arc)
    : mLayout1(NULL),
      mLayout2(NULL),
      mBackButton(NULL),
      mUpButton(NULL),
      mDownButton(NULL),
      mItems(NULL),
      mSelected(NULL),
      unk20(0),
      mDraw(NULL),
      mState(NULL),
      mScroll(NULL) {
    mListX = 0.5f * GetScreenWidth();
    mScrollY = 0.0f;
    unkB0 = 0.0f;
    unkB4 = 0.0f;
    unkB8 = 0.0f;
    unkBC = 0.0f;
    unkC0 = 0.0f;
    unkC4 = 0.0f;
    mListTop = 130.0f;
    mScrollTarget = 0.0f;
    mRowHeight = 60.0f;
    mDragMax = 0.0f;
    mDragMin = 0.0f;
    mDragVelocity = 0.0f;
    mViewTop = 130.0f;
    mViewBottom = 393.0f;
    mMaxScroll = 0;
    mScrollIdx = 0;
    mStep = 0;
    mScrollStep = 0;
    mNumItems = 0;
    mUpPressed = false;
    mDownPressed = false;
    mDragHeld = false;
    mYesPressed = false;
    mNoPressed = false;
    mDragging = false;
    unk106 = false;
    mBackPressed = false;

    if (IsErrorState()) {
        return;
    }

    Layout* layout = new Layout((void*)arc, "set_language1.brlyt", (PaneButtonColors*)lbl_801EE270, false);
    mLayout1 = layout;
    if (mLayout1 == NULL) {
        gAllocFailed = true;
        return;
    }
    layout = new Layout((void*)arc, "set_language2.brlyt", (PaneButtonColors*)lbl_801EE270, false);
    mLayout2 = layout;
    if (mLayout2 == NULL) {
        gAllocFailed = true;
        return;
    }

    mBackButton = mLayout1->FindButton("back");
    if (mBackButton == NULL) {
        gFatalError = true;
        return;
    }
    DisableButton(mBackButton);

    mUpButton = mLayout1->FindButton("up");
    if (mUpButton == NULL) {
        gFatalError = true;
        return;
    }
    mDownButton = mLayout1->FindButton("down");
    if (mDownButton == NULL) {
        gFatalError = true;
        return;
    }

    mWriter.SetFont(*gSysFont);
    mWriter.SetCharSpace(0.0f);
    mWriter.SetScale(1.0f);

    f32 width = GetContentRight() - GetSideMargin();
    f32 rowHeight = mRowHeight;
    f32 screenWidth = GetScreenWidth();
    const u8* language = gNewsData->mHeader->languages;
    f32 y = 0.5f * rowHeight;
    f32 scale = 1.0f;

    for (mNumItems = 0; language[mNumItems] != 0xFF;) {
        if (++mNumItems >= 16) {
            mNumItems = 16;
            break;
        }
    }

    mItems = new Item[mNumItems];
    if (mItems == NULL) {
        gAllocFailed = true;
        return;
    }

    Item* item = mItems;
    for (s32 i = 0; i < mNumItems; i++, item++) {
        item->language = language[i];
        item->name = lbl_801B26BC[item->language];
        item->scaleX = scale;
        item->scaleY = scale;
        item->width = width;
        item->height = rowHeight;
        item->rect.left = 0.0f;
        item->rect.right = screenWidth;
        item->unk0 = 0;
        item->unk4 = 0;
        item->hover = false;
        item->prevHover = false;
        item->hidden = false;
        item->x = mListX;
        item->y = y;
        y += rowHeight;
    }
    mMaxScroll = mNumItems - 2;

    ChangeState(&LanguageSelect::StateIdle);
    ChangeScroll(&LanguageSelect::ScrollIdle);
}

LanguageSelect::Item::~Item() {}

LanguageSelect::~LanguageSelect() {
    if (mItems != NULL) {
        delete[] mItems;
    }
    if (mLayout2 != NULL) {
        delete mLayout2;
    }
    if (mLayout1 != NULL) {
        delete mLayout1;
    }
}

void LanguageSelect::Start() {
    mLayout1->Reset();
    mLayout2->Reset();
    ChangeState(&LanguageSelect::StateList);
}

void LanguageSelect::Draw() {
    f32 scale = 832.0f / 608.0f;
    math::VEC3 pos(0.0f, 0.0f, 0.0f);
    Draw2D_SetupGX();
    Draw2D_SetOrtho();
    Draw2D_Tex(gCommonTpl, 0, &pos, scale, 1.0f);
    if (mDraw) {
        (this->*mDraw)();
    }
}

void LanguageSelect::Update(bool arg) {
    unk106 = arg;
    mUpPressed = false;
    mDownPressed = false;
    mDragHeld = false;
    mYesPressed = false;
    mNoPressed = false;
    mBackPressed = false;
    mLayout1->Calc();
    mLayout2->Calc();
    if (mState) {
        (this->*mState)(NULL);
    }
}

void LanguageSelect::DrawList() {
    f32 offsetY = mScrollY + mListTop;
    Item* item = mItems;
    ut::Color color = ut::Color::WHITE;
    math::VEC3 quad[4];

    Draw2D_SetScissor(0, 133, GetScreenWidth(), 323);
    quad[0].z = quad[1].z = quad[2].z = quad[3].z = 0.0f;
    for (s32 i = 0; i < mNumItems; i++, item++) {
        if (!item->hidden) {
            quad[0].x = quad[1].x = item->rect.left;
            quad[2].x = quad[3].x = item->rect.right;
            quad[0].y = quad[3].y = item->rect.top;
            quad[1].y = quad[2].y = item->rect.bottom;
            color = item->hover ? static_cast<ut::Color&>(gHighlightColor) : sFillColor;
            Draw2D_FillQuad(quad, &color);
        }
    }

    mWriter.SetDrawFlag(0x111);
    mWriter.SetupGX();
    mWriter.SetTextColor(ut::Color(static_cast<GXColor&>(sTextColor)));
    item = mItems;
    for (s32 i = 0; i < mNumItems; i++, item++) {
        if (!item->hidden) {
            mWriter.SetScale(item->scaleX, item->scaleY);
            mWriter.SetCursor(item->x, item->y + offsetY);
            mWriter.Print(item->name);
        }
    }

    Draw2D_SetScissor(0, 0, GetScreenWidth(), 456);
    mLayout1->Draw();
}

void LanguageSelect::DrawConfirm() {
    math::VEC2 pos(mListX, 150.0f);
    math::VEC3 quad[4];
    quad[0].z = quad[1].z = quad[2].z = quad[3].z = 0.0f;
    quad[0].x = quad[1].x = 0.0f;
    quad[2].x = quad[3].x = GetScreenWidth();
    quad[0].y = quad[3].y = pos.y - 20.0f;
    quad[1].y = quad[2].y = 20.0f + pos.y;
    Draw2D_FillQuad(quad, &sFillColor);

    Draw2D_SetupGX();
    Draw2D_SetOrtho();
    mWriter.SetDrawFlag(0x111);
    mWriter.SetupGX();
    mWriter.SetTextColor(ut::Color(static_cast<GXColor&>(sTextColor)));
    mWriter.SetScale(mSelected->scaleX, mSelected->scaleY);
    mWriter.SetCursor(pos.x, pos.y);
    mWriter.Print(mSelected->name);
    mLayout2->Draw();
}

void LanguageSelect::StateIdle(Item* item) {
    switch (mStep) {
    case 0:
        mStep++;
        mDraw = NULL;
        break;
    case -1:
        break;
    default:
        break;
    }
}

void LanguageSelect::StateList(Item* item) {
    switch (mStep) {
    case 0:
        mStep++;
        mDraw = &LanguageSelect::DrawList;
        mScrollIdx = 0;
        mScrollTarget = 0.0f;
        mScrollY = 0.0f;
        mActiveLayout = mLayout1;
        ChangeScroll(&LanguageSelect::ScrollIdle);
        break;
    case -1:
        break;
    default:
        CheckInput();
        switch (mStep) {
        case 1:
            if (mBackPressed) {
                PlaySE(0x22);
                lbl_80357598 = 0;
                mStep++;
                break;
            }
            if (mScroll) {
                (this->*mScroll)();
            }
            if (UpdateItems()) {
                return;
            }
            break;
        }
        break;
    }
}

void LanguageSelect::StateConfirm(Item* item) {
    switch (mStep) {
    case 0:
        mStep++;
        mDraw = &LanguageSelect::DrawConfirm;
        mActiveLayout = mLayout2;
        mSelected = item;
        if (item == NULL) {
            mSelected = mItems;
        }
        mLayout2->Reset();
        ChangeScroll(&LanguageSelect::ScrollIdle);
        break;
    case -1:
        break;
    default:
        CheckInput();
        switch (mStep) {
        case 1:
            if (mYesPressed) {
                PlaySE(0x21);
                mStep = 2;
                PressButton(mLayout2, "yes");
            } else if (mNoPressed) {
                PlaySE(0x22);
                mStep = 3;
                mTimer = 12;
                PressButton(mLayout2, "no");
            }
            break;
        case 2:
            gSelectedNewsLanguage = mSelected->language;
            lbl_80357598 = 0;
            ChangeState(&LanguageSelect::StateIdle);
            break;
        case 3:
        default:
            if (mTimer != 0) {
                mTimer--;
            } else {
                ChangeState(&LanguageSelect::StateList);
            }
            break;
        }
        break;
    }
}

void LanguageSelect::CheckInput() {
    UpdateLayoutButtons(mActiveLayout, 0x23);
    if (gHoldAll & 0x400) {
        mDragHeld = true;
    } else {
        if (CheckButtonHold("down", 0x800) >= 0) {
            mDownPressed = true;
        } else if (gRepeatFastAll & 4) {
            mDownPressed = true;
            mDownButton->SetPressed(true);
        }
        if (CheckButtonHold("up", 0x800) >= 0) {
            mUpPressed = true;
        } else if (gRepeatFastAll & 8) {
            mUpPressed = true;
            mUpButton->SetPressed(true);
        }
    }
    if (CheckButtonTrig("yes", 0x800) >= 0) {
        mYesPressed = true;
    }
    if (CheckButtonTrig("no", 0x800) >= 0) {
        mNoPressed = true;
    }
    if (CheckButtonTrig("back", 0x800) >= 0) {
        mBackPressed = true;
    }
}

void LanguageSelect::ScrollIdle() {
    switch (mScrollStep) {
    case 0:
        mScrollStep++;
        break;
    case -1:
        break;
    default:
        if (mMaxScroll > 1 && mDragHeld) {
            ChangeScroll(&LanguageSelect::ScrollDrag);
            return;
        }

        if (mDownPressed) {
            if (mMaxScroll > 1 && mScrollIdx < mMaxScroll - 1) {
                mScrollIdx++;
                if (mScrollIdx >= mMaxScroll) {
                    mScrollIdx = mMaxScroll - 1;
                }
                PlaySE(0x4A);
            }
        } else if (mUpPressed) {
            if (mScrollIdx > 0) {
                if (--mScrollIdx < 0) {
                    mScrollIdx = 0;
                }
                PlaySE(0x4A);
            }
        }

        if (mScrollIdx == 0) {
            DisableButton(mUpButton);
        } else {
            mUpButton->mDisabled = false;
        }
        if (mMaxScroll <= 0 || mScrollIdx == mMaxScroll - 1) {
            DisableButton(mDownButton);
        } else {
            mDownButton->mDisabled = false;
        }
        mScrollTarget = -(mRowHeight * mScrollIdx);
        break;
    }
    Ease(&mScrollY, mScrollTarget, 0.1f, 20.0f, 5.0f);
}

void LanguageSelect::ScrollDrag() {
    switch (mScrollStep) {
    case -1:
        lbl_801EDFD0[0] = 1;
        lbl_801EDFD0[1] = 1;
        lbl_801EDFD0[2] = 1;
        lbl_801EDFD0[3] = 1;
        lbl_803575BA = false;
        lbl_803575BB = false;
        mDragging = false;
        if (mDragVelocity < 0.0f) {
            mScrollIdx = (-mScrollY + mRowHeight) / mRowHeight;
        } else {
            mScrollIdx = -mScrollY / mRowHeight;
        }
        if (mScrollIdx > mMaxScroll - 1) {
            mScrollIdx = mMaxScroll - 1;
        } else if (mScrollIdx < 0) {
            mScrollIdx = 0;
        }
        mScrollTarget = -(mRowHeight * mScrollIdx);
        return;
    case 0:
        mScrollStep++;
        mDragMax = 0.0f;
        mDragVelocity = 0.0f;
        mDragMin = -(mRowHeight * (mMaxScroll - 1));
        PlaySE(0x16);
        mDragging = true;
        {
        f32 y = lbl_803575D8;
        lbl_801EDFA0[1] = 138.0f;
        lbl_801EDFB8[1] = y;
        lbl_80357600.a = 255;
        }
    default:
        break;
    }

    if (!mDragHeld) {
        ChangeScroll(&LanguageSelect::ScrollIdle);
        return;
    }

    for (s32 i = 0; i < 4; i++) {
        lbl_801EDFD0[i] = 1;
        if (gHold[i] & 0x400) {
            lbl_801EDFD0[i] = 5;
            f32 v = gPointerScroll[i];
            mDragVelocity = 0.1f * v;
            break;
        }
    }

    mScrollY += mDragVelocity;
    if (mScrollY > mDragMax) {
        mScrollY = mDragMax;
    } else if (mScrollY < mDragMin) {
        mScrollY = mDragMin;
    }

    if (mScrollY >= mDragMax) {
        DisableButton(mUpButton);
        mDownButton->mDisabled = false;
        lbl_803575BA = false;
        lbl_803575BB = true;
    } else if (mScrollY <= mDragMin) {
        mUpButton->mDisabled = false;
        DisableButton(mDownButton);
        lbl_803575BA = true;
        lbl_803575BB = false;
    } else {
        mUpButton->mDisabled = false;
        mDownButton->mDisabled = false;
        lbl_803575BA = true;
        lbl_803575BB = true;
    }
}

BOOL LanguageSelect::UpdateItems() {
    f32 margin = 20.0f;
    f32 offsetY = (mScrollY + mListTop) - margin;
    Item* item = mItems;
    for (s32 i = 0; i < mNumItems; i++, item++) {
        item->prevHover = item->hover;
        item->hover = false;
        item->rect.top = item->y + offsetY;
        item->rect.bottom = 40.0f + item->rect.top;
        item->hidden = item->rect.bottom < 0.0f || item->rect.top > mViewBottom;
    }

    item = mItems;
    for (s32 i = 0; i < mNumItems; i++, item++) {
        if (HitTest(item) >= 0) {
            PlaySE(0x48);
            ChangeState(&LanguageSelect::StateConfirm, item);
            return TRUE;
        }
    }
    return FALSE;
}

s32 LanguageSelect::HitTest(Item* item) {
    for (s32 chan = 0; chan < 4; chan++) {
        if (!IsPointerValid(chan)) {
            continue;
        }
        f32 x = gCursorX[chan][0];
        f32 y = gCursorY[chan][0];
        if (y > mViewTop && y < mViewBottom && x > item->rect.left && x < item->rect.right &&
            y > item->rect.top && y < item->rect.bottom)
        {
            item->hover = true;
            if (!item->prevHover) {
                PlaySE(0x49);
            }
            if (gTrig[chan] & 0x800) {
                return chan;
            }
        }
    }
    return -1;
}

void LanguageSelect::SetBackEnabled(BOOL enabled) {
    if (enabled) {
        mBackButton->mDisabled = false;
    } else {
        DisableButton(mBackButton);
    }
}

void LanguageSelect::Reset() {
    ChangeState(&LanguageSelect::StateIdle);
    ChangeScroll(&LanguageSelect::ScrollIdle);
}
