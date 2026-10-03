// In this file's NW4R headers VEC3 has no destructor (no temporaries are
// destroyed in UpdatePane).
#define NW4R_MATH_VEC3_NO_DTOR
#include <news/PaneButton.h>
#include <news/System.h>
#include <nw4r/lyt/lyt_material.h>
#include <nw4r/lyt/lyt_textBox.h>
#include <nw4r/math/math_triangular.h>
#include <stdio.h>

using namespace nw4r;

const char* GetLanguageSuffix();

static PaneButtonColors sColors[4] = {
    {
        ut::Color(0, 0, 0, 255),
        ut::Color(255, 255, 255, 255),
        ut::Color(0, 0, 0, 255),
        ut::Color(216, 216, 216, 255),
        ut::Color(0, 0, 0, 255),
        ut::Color(255, 255, 255, 255),
        ut::Color(193, 0, 0, 255),
        ut::Color(255, 255, 255, 255),
        ut::Color(0, 0, 0, 255),
    },
    {
        ut::Color(0, 0, 0, 255),
        ut::Color(255, 255, 255, 255),
        ut::Color(0, 0, 0, 255),
        ut::Color(255, 120, 0, 255),
        ut::Color(0, 0, 0, 255),
        ut::Color(255, 255, 255, 255),
        ut::Color(193, 0, 0, 255),
        ut::Color(255, 255, 255, 255),
        ut::Color(0, 0, 0, 255),
    },
    {
        ut::Color(0, 0, 0, 255),
        ut::Color(255, 255, 255, 255),
        ut::Color(0, 0, 0, 255),
        ut::Color(248, 190, 0, 255),
        ut::Color(0, 0, 0, 255),
        ut::Color(255, 255, 255, 255),
        ut::Color(193, 0, 0, 255),
        ut::Color(255, 255, 255, 255),
        ut::Color(0, 0, 0, 255),
    },
    {
        ut::Color(0, 0, 0, 255),
        ut::Color(255, 255, 255, 255),
        ut::Color(0, 0, 0, 255),
        ut::Color(89, 221, 255, 255),
        ut::Color(0, 0, 0, 255),
        ut::Color(255, 255, 255, 255),
        ut::Color(193, 0, 0, 255),
        ut::Color(255, 255, 255, 255),
        ut::Color(0, 0, 0, 255),
    },
};

static void SetTagProcessorRecursive(lyt::Pane* pane, ut::TagProcessorBase<wchar_t>* tagProcessor) {
    lyt::TextBox* textBox = ut::DynamicCast<lyt::TextBox*>(pane);
    if (textBox != NULL) {
        textBox->SetTagProcessor(tagProcessor);
    }

    lyt::PaneList& list = pane->GetChildList();
    lyt::PaneList::Iterator it;
    for (it = list.GetBeginIter(); it != list.GetEndIter(); it++) {
        SetTagProcessorRecursive(&*it, tagProcessor);
    }
}

static void SetTextColorRecursive(lyt::Pane* pane, const GXColor& color) {
    if (pane->GetUserData()[0] == 'F' || pane->GetUserData()[0] == 'f') {
        return;
    }

    lyt::TextBox* textBox = ut::DynamicCast<lyt::TextBox*>(pane);
    if (textBox != NULL) {
        textBox->SetTextColor(color, color);
    }

    lyt::PaneList& list = pane->GetChildList();
    for (lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); it++) {
        SetTextColorRecursive(&*it, color);
    }
}

static void SetMaterialColorRecursive(lyt::Pane* pane, const ut::Color& color, int reg) {
    if (pane->GetUserData()[0] == 'F' || pane->GetUserData()[0] == 'f') {
        return;
    }

    lyt::Material* material = pane->FindMaterialByName(pane->GetName(), true);
    if (material != NULL) {
        switch (reg) {
        case 0:
            material->SetColorElement(lyt::ANIMTARGET_MATCOLOR_TEV0R, color.r);
            material->SetColorElement(lyt::ANIMTARGET_MATCOLOR_TEV0G, color.g);
            material->SetColorElement(lyt::ANIMTARGET_MATCOLOR_TEV0B, color.b);
            break;
        case 1:
            material->SetColorElement(lyt::ANIMTARGET_MATCOLOR_TEV1R, color.r);
            material->SetColorElement(lyt::ANIMTARGET_MATCOLOR_TEV1G, color.g);
            material->SetColorElement(lyt::ANIMTARGET_MATCOLOR_TEV1B, color.b);
            break;
        }
    }

    lyt::PaneList& list = pane->GetChildList();
    for (lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); it++) {
        SetMaterialColorRecursive(&*it, color, reg);
    }
}

static void SetMaterialAlphaRecursive(lyt::Pane* pane, int alpha) {
    lyt::Material* material = pane->FindMaterialByName(pane->GetName(), true);
    if (material != NULL) {
        material->SetColorElement(lyt::ANIMTARGET_MATCOLOR_TEV1A, (u8)alpha);
    }

    lyt::PaneList& list = pane->GetChildList();
    for (lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); it++) {
        SetMaterialAlphaRecursive(&*it, alpha);
    }
}

static ut::Color BlendColor(const ut::Color& from, const ut::Color& to, int t, int max) {
    ut::Color color(ut::Color::WHITE);
    color.r = (from.r * (max - t) + to.r * t) / max;
    color.g = (from.g * (max - t) + to.g * t) / max;
    color.b = (from.b * (max - t) + to.b * t) / max;
    return color;
}

PaneButton::PaneButton(lyt::Pane* pane, const lyt::DrawInfo* drawInfo, PaneButtonColors* colors,
                       ut::TagProcessorBase<wchar_t>* tagProcessor)
    : mColors(colors),
      mPane(pane),
      mDrawInfo(drawInfo),
      mBasePane(NULL),
      mIconPane(NULL),
      mTextPane(NULL),
      mFrame0Pane(NULL),
      mFrame1Pane(NULL),
      mLinkA(NULL),
      mLinkB(NULL),
      mTextColorCallback(NULL),
      mMoveCallback(NULL),
      mTextColor(ut::Color::WHITE),
      mRect(0.0f, 0.0f, 0.0f, 0.0f),
      mColorSet(0),
      mFixed(false),
      mUnk91(false),
      mToggle(false),
      mInactive(false),
      mHidden(false),
      mDisabled(false),
      mFadeFixed(true) {
    char name[128];

    sprintf(name, "%sB", mPane->GetName());
    mBasePane = mPane->FindPaneByName(name, true);
    if (mBasePane == NULL) {
        mBasePane = mPane;
    }

    sprintf(name, "%sR", mPane->GetName());
    lyt::Pane* rectPane = mPane->FindPaneByName(name, true);
    if (rectPane == NULL) {
        rectPane = mBasePane;
    }

    mBasePos = mPane->GetTranslate();
    mRect = rectPane->GetPaneRect(*mDrawInfo);
    mRect.left += mBasePos.x;
    mRect.right += mBasePos.x;
    mRect.top += mBasePos.y;
    mRect.bottom += mBasePos.y;

    if (mBasePane != mPane) {
        math::VEC3 trans = mBasePane->GetTranslate();
        mRect.left += trans.x;
        mRect.right += trans.x;
        mRect.top += trans.y;
        mRect.bottom += trans.y;
    }

    for (s32 i = 0; i < 8; i++) {
        switch (mPane->GetUserData()[i]) {
        case 'D':
        case 'd':
            mDisabled = true;
            break;
        case 'F':
        case 'f':
            mFixed = true;
            break;
        case 'C':
        case 'c':
            i++;
            if (i < 8) {
                s32 set = mPane->GetUserData()[i] - '0';
                if (set >= 0 && set <= 9) {
                    mColorSet = set;
                }
            }
            break;
        }
    }

    sprintf(name, "%sI", mPane->GetName());
    lyt::Pane* iconPane = mPane->FindPaneByName(name, true);
    if (iconPane == NULL) {
        mIconPane = NULL;
    } else {
        sprintf(name, "%s%s", iconPane->GetName(), GetLanguageSuffix());
        mIconPane = iconPane->FindPaneByName(name, true);
        if (mIconPane == NULL) {
            mIconPane = iconPane;
        } else {
            for (lyt::PaneList::Iterator it = iconPane->GetChildList().GetBeginIter();
                 it != iconPane->GetChildList().GetEndIter(); it++) {
                if (&*it != mIconPane) {
                    it->SetVisible(false);
                }
            }
        }
    }

    mTextPos = mPane->GetTranslate();
    mTextSize = mPane->GetSize();

    sprintf(name, "%sT", mPane->GetName());
    lyt::Pane* textPane = mPane->FindPaneByName(name, true);
    if (textPane == NULL) {
        mTextPane = NULL;
    } else {
        mTextPos += textPane->GetTranslate();
        mTextSize = textPane->GetSize();
        mTextPos.x *= gWidescreen ? 1.3684211f : 1.0f;
        mTextSize.width *= gWidescreen ? 1.3684211f : 1.0f;

        sprintf(name, "%s%s", textPane->GetName(), GetLanguageSuffix());
        mTextPane = mPane->FindPaneByName(name, true);
        if (mTextPane != NULL) {
            mTextPos += mTextPane->GetTranslate();
            mTextSize = mTextPane->GetSize();
        }

        for (lyt::PaneList::Iterator it = textPane->GetChildList().GetBeginIter();
             it != textPane->GetChildList().GetEndIter(); it++) {
            if (&*it != mTextPane) {
                it->SetVisible(false);
            }
        }
    }

    if (mFixed && mTextPane != NULL) {
        SetTagProcessorRecursive(mTextPane, tagProcessor);
    }

    sprintf(name, "%sF", mPane->GetName());
    lyt::Pane* framePane = mPane->FindPaneByName(name, true);
    if (framePane == NULL) {
        mFrame0Pane = NULL;
        mFrame1Pane = NULL;
    } else {
        sprintf(name, "%sF0", mPane->GetName());
        mFrame0Pane = framePane->FindPaneByName(name, true);
        sprintf(name, "%sF1", mPane->GetName());
        mFrame1Pane = framePane->FindPaneByName(name, true);
    }

    lyt::TextBox* textBox = ut::DynamicCast<lyt::TextBox*>(mBasePane);
    if (textBox != NULL) {
        textBox->SetVisible(true);
        SetTagProcessorRecursive(textBox, tagProcessor);
    }

    mUnk00 = 0;
    if (colors == NULL) {
        mColors = sColors;
    }

    Reset();
}

PaneButton::~PaneButton() {}

void PaneButton::Reset() {
    mOffsetY = 0.0f;
    mAlpha = 255;
    mFadeAlpha = 255;
    mHover = false;
    mHoverFrame = 0;
    mPressed = false;
    mPressFrame = 0;
    mSelIndex = 0;
    mNextSelIndex = 0;
    mSelected = false;
    mToggle = false;
}

void PaneButton::UpdateFrame() {
    if (mHidden) {
        return;
    }

    if (mHover) {
        if (mHoverFrame < 12) {
            mHoverFrame++;
        }
        mHover = false;
    } else if (mHoverFrame > 0) {
        mHoverFrame--;
    }

    if (mPressed) {
        if (mPressFrame > 0) {
            if (--mPressFrame == 0) {
                mSelIndex = mNextSelIndex;
            }
        }
        if (mPressFrame == 0) {
            mPressed = false;
        }
        if (mPressFrame == 9) {
            mHoverFrame = 12;
        }
    } else if (mPressFrame > 1) {
        mPressFrame--;
    }
}

// This file is built with -ipa file, under which a function that gets too big
// from inlining is compiled with no inlining at all. UpdatePane is just below
// that limit here, so force the same result.
#pragma dont_inline on
void PaneButton::UpdatePane() {
    lyt::Pane* textPane = NULL;

    if (mHidden) {
        return;
    }

    if (mFadeFixed) {
        if (!mFixed) {
            SetMaterialAlphaRecursive(mPane, mAlpha);
        }
    } else {
        SetMaterialAlphaRecursive(mPane, mAlpha);
    }

    if (mTextPane != NULL) {
        lyt::PaneList& list = mTextPane->GetChildList();
        lyt::PaneList::Iterator it;

        if (mSelIndex >= 0) {
            textPane = mTextPane;
            int i = 0;
            for (it = list.GetBeginIter(); it != list.GetEndIter(); it++) {
                if (mSelIndex == i) {
                    textPane = &*it;
                    break;
                }
                i++;
            }
            textPane->SetVisible(true);
        }

        for (it = list.GetBeginIter(); it != list.GetEndIter(); it++) {
            if (&*it != textPane) {
                (*it).SetVisible(false);
            }
        }
    }

    if (!mFixed) {
        ut::Color top;
        ut::Color bottom;

        int baseAlpha = mAlpha * mFadeAlpha / 255;
        int iconAlpha = mAlpha * (mFadeAlpha + 128 > 255 ? 255 : mFadeAlpha + 128) / 255;
        int textAlpha = iconAlpha;

        if (mSelected) {
            top = mColors[mColorSet].mBaseTopSelect;
            bottom = mColors[mColorSet].mBaseBottomSelect;
        } else if (mToggle && mPressFrame > 0) {
            top = mColors[mColorSet].mBaseTopHover;
            bottom = mColors[mColorSet].mBaseBottomHover;
        } else if (mHoverFrame > 0) {
            top = BlendColor(mColors[mColorSet].mBaseTop, mColors[mColorSet].mBaseTopHover, mHoverFrame, 12);
            bottom = BlendColor(mColors[mColorSet].mBaseBottom, mColors[mColorSet].mBaseBottomHover, mHoverFrame, 12);
        } else if (mPressFrame > 0) {
            top = mColors[mColorSet].mBaseTopHover;
            bottom = mColors[mColorSet].mBaseBottomHover;
        } else {
            top = mColors[mColorSet].mBaseTop;
            bottom = mColors[mColorSet].mBaseBottom;
        }

        if (mFadeAlpha < 255) {
            {
                GXColor clear = {255, 255, 255, 0};
                top = BlendColor(top, clear, 255 - mFadeAlpha, 255);
            }
            {
                GXColor clear = {255, 255, 255, 0};
                bottom = BlendColor(bottom, clear, 255 - mFadeAlpha, 255);
            }
        }

        SetMaterialColorRecursive(mBasePane, top, 0);
        SetMaterialColorRecursive(mBasePane, bottom, 1);
        SetMaterialAlphaRecursive(mBasePane, baseAlpha);

        if (mIconPane != NULL && mIconPane->GetUserData()[0] != 'F') {
            if (mSelected) {
                top = mColors[mColorSet].mIconTopSelect;
                bottom = mColors[mColorSet].mIconBottomSelect;
            } else if (mDisabled) {
                iconAlpha = 0;
            } else if (mToggle && mPressFrame > 0) {
                top = mColors[mColorSet].mIconTopHover;
                bottom = mColors[mColorSet].mIconBottomHover;
            } else if (mHoverFrame > 0) {
                top = BlendColor(mColors[mColorSet].mIconTop, mColors[mColorSet].mIconTopHover, mHoverFrame, 12);
                bottom = BlendColor(mColors[mColorSet].mIconBottom, mColors[mColorSet].mIconBottomHover, mHoverFrame, 12);
            } else if (mPressFrame > 0) {
                top = mColors[mColorSet].mIconTopHover;
                bottom = mColors[mColorSet].mIconBottomHover;
            } else if (mBlend > 0) {
                top = BlendColor(mColors[mColorSet].mIconTop, mColors[mColorSet].mIconTopBlend, mBlend, mBlendMax);
                bottom = BlendColor(mColors[mColorSet].mIconBottom, mColors[mColorSet].mIconBottomBlend, mBlend, mBlendMax);
            } else {
                top = mColors[mColorSet].mIconTop;
                bottom = mColors[mColorSet].mIconBottom;
            }

            SetMaterialColorRecursive(mIconPane, top, 0);
            SetMaterialColorRecursive(mIconPane, bottom, 1);
            SetMaterialAlphaRecursive(mIconPane, iconAlpha);
        }

        if (textPane != NULL) {
            if (mSelected) {
                top = mColors[mColorSet].mTextSelect;
            } else if (mDisabled) {
                textAlpha = 0;
            } else if (mToggle && mPressFrame > 0) {
                top = mColors[mColorSet].mTextHover;
            } else if (mHoverFrame > 0) {
                top = BlendColor(mColors[mColorSet].mText, mColors[mColorSet].mTextHover, mHoverFrame, 12);
            } else if (mPressFrame > 0) {
                top = mColors[mColorSet].mTextHover;
            } else if (mBlend > 0) {
                top = BlendColor(mColors[mColorSet].mText, mColors[mColorSet].mTextBlend, mBlend, mBlendMax);
            } else {
                top = mColors[mColorSet].mText;
            }

            top.a = textAlpha;
            mTextColor = top;
            SetTextColorRecursive(textPane, top);
            if (mTextColorCallback != NULL) {
                mTextColorCallback(textPane, top);
            }
        }

        if (mFrame0Pane != NULL && mFrame1Pane != NULL) {
            if (mPressFrame > 0) {
                mFrame0Pane->SetVisible(false);
                mFrame1Pane->SetVisible(true);
            } else {
                mFrame0Pane->SetVisible(true);
                mFrame1Pane->SetVisible(false);
            }
        }
    }

    f32 offset;
    if (mPressFrame > 0 && mFrame0Pane == NULL) {
        if (mToggle) {
            offset = 12.0f;
            if (mLinkA != NULL) {
                mLinkA->SetHover();
            }
            if (mLinkB != NULL) {
                mLinkB->SetHover();
            }
        } else {
            int frame = mPressFrame > 14 ? 14 : mPressFrame;
            f32 rad = 1.5708f * (14 - frame) / 14.0f;
            offset = 18.0f * (1.0f - math::SinRad(rad));
        }
    } else {
        offset = 0.0f;
    }

    mPane->SetTranslate(mBasePos + math::VEC3(0.0f, mOffsetY - offset, 0.0f));

    if (mMoveCallback != NULL) {
        mMoveCallback(mCallbackArg);
    }
}

#pragma dont_inline reset

void PaneButton::Draw() {
    if (!mHidden) {
        mPane->Draw(*mDrawInfo);
    }
}

bool PaneButton::HitTest(f32 x, f32 y) {
    if (mHidden || mInactive) {
        return false;
    }

    y -= mOffsetY;
    return x >= mRect.left && x < mRect.right && y < mRect.top && y >= mRect.bottom;
}

void PaneButton::SetHover() {
    if (!mFixed && !mDisabled && !mHidden) {
        mHover = true;
        if (mLinkA != NULL) {
            mLinkA->SetHover();
        }
        if (mLinkB != NULL) {
            mLinkB->SetHover();
        }
    }
}

void PaneButton::SetPressed(bool pressed) {
    if (!mFixed && !mDisabled && !mHidden) {
        mPressed = pressed;
        mPressFrame = 18;
    }
}

void PaneButton::Press() {
    if (mFixed) {
        return;
    }

    mPressed = true;
    if (mLinkA != NULL) {
        mLinkA->SetHover();
    }
    if (mLinkB != NULL) {
        mLinkB->SetHover();
    }
}

void PaneButton::SetSelIndex(s32 idx) {
    mSelIndex = idx;
    mNextSelIndex = idx;
}

void PaneButton::SetText(const wchar_t* text) {
    lyt::TextBox* textBox = ut::DynamicCast<lyt::TextBox*>(mBasePane);
    if (textBox == NULL) {
        textBox = ut::DynamicCast<lyt::TextBox*>(mTextPane);
    }

    if (textBox != NULL) {
        int len = 0;
        for (const wchar_t* p = text; *p != 0; p++, len++) {
        }
        textBox->AllocStringBuffer(len + 1);
        textBox->SetString(text);
    }
}

void PaneButton::Hide() {
    mHover = false;
    mHoverFrame = 0;
    mPressed = false;
    mPressFrame = 0;
    mHidden = true;
    mPane->SetTranslate(mBasePos);
}

void PaneButton::SetAlpha(s32 alpha) {
    mPane->SetAlpha(alpha);
}

lyt::Pane* PaneButton::FindPane(const char* name) {
    return mPane->FindPaneByName(name, true);
}
