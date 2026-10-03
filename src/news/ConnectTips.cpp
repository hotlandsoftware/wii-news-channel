#include <news/Connect.h>
#include <nw4r/lyt/lyt_pane.h>
#include <nw4r/lyt/lyt_textBox.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

using namespace nw4r;

const char* GetLanguageSuffix();

ConnectTips::ConnectTips(lyt::Pane* pane) {
    char name[128];
    sprintf(name, "%s%s", pane->GetName(), GetLanguageSuffix());
    mRoot = pane->FindPaneByName(name, true);
    mTip = NULL;
    mTextBox = NULL;
    mNumPages = 0;
    mPage = 0;
    memset(mText, 0, sizeof(mText));
    mLength = 0;
    mPos = 0;
    for (lyt::PaneList::Iterator it = pane->GetChildList().GetBeginIter();
         it != pane->GetChildList().GetEndIter(); it++) {
        it->SetVisible(false);
    }
    mRoot->SetVisible(true);
}

ConnectTips::~ConnectTips() {}

s32 ConnectTips::GetNumTips() {
    s32 n = 0;
    for (lyt::PaneList::Iterator it = mRoot->GetChildList().GetBeginIter();
         it != mRoot->GetChildList().GetEndIter(); it++) {
        n++;
    }
    return n;
}

void ConnectTips::SetTip(s32 index) {
    s32 i = 0;
    lyt::PaneList& tips = mRoot->GetChildList();
    for (lyt::PaneList::Iterator it = tips.GetBeginIter(); it != tips.GetEndIter(); it++, i++) {
        if (i == index) {
            it->SetVisible(true);
            mTip = &*it;
        } else {
            it->SetVisible(false);
        }
    }
    mNumPages = 0;
    lyt::PaneList& pages = mTip->GetChildList();
    for (lyt::PaneList::Iterator it = pages.GetBeginIter(); it != pages.GetEndIter(); it++) {
        mNumPages++;
    }
    mPage = 0;
    ShowPage();
}

BOOL ConnectTips::TypeText() {
    BOOL pause = FALSE;
    if (mPos >= mLength) {
        return FALSE;
    }
    while (mPos < mLength) {
        wchar_t c = mText[mPos];
        mPos++;
        switch (c) {
        case L'!':
        case L'.':
        case L'?':
        case 0x2026:
        case 0x3002:
        case 0xFF01:
        case 0xFF1F:
            switch (mText[mPos]) {
            case L'\n':
                pause = TRUE;
                break;
            }
            break;
        }
        if (c > 0x1F) {
            break;
        }
    }
    wchar_t buf[0x200];
    u32 i;
    for (i = 0; i < mPos; i++) {
        buf[i] = mText[i];
    }
    buf[mPos] = 0;
    mTextBox->SetString(buf, 0);
    return pause;
}

BOOL ConnectTips::IsTextDone() {
    return mPos == mLength;
}

void ConnectTips::NextPage() {
    if (mPage < mNumPages - 1) {
        mPage++;
        ShowPage();
    }
}

BOOL ConnectTips::IsLastPage() {
    return mPage == mNumPages - 1;
}

void ConnectTips::ShowPage() {
    s32 i = 0;
    lyt::PaneList& pages = mTip->GetChildList();
    for (lyt::PaneList::Iterator it = pages.GetBeginIter(); it != pages.GetEndIter(); it++, i++) {
        if (i == mPage) {
            lyt::Pane* pane = &*it;
            it->SetVisible(true);
            mTextBox = ut::DynamicCast<lyt::TextBox*>(pane);
        } else {
            it->SetVisible(false);
        }
    }
    wcsncpy(mText, mTextBox->GetStringBuffer(), 0x200);
    mText[0x1FF] = 0;
    mLength = wcslen(mText);
    mPos = 0;
    mTextBox->SetString(L"", 0);
}
