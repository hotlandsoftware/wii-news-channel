#include <news/LayoutScreen.h>
#include <news/ColorTagProcessor.h>
#include <news/System.h>
#include <nw4r/lyt/lyt_arcResourceAccessor.h>
#include <nw4r/lyt/lyt_drawInfo.h>
#include <nw4r/lyt/lyt_layout.h>
#include <nw4r/lyt/lyt_material.h>
#include <nw4r/lyt/lyt_textBox.h>
#include <nw4r/math/math_triangular.h>
#include <nw4r/ut/ut_TextWriterBase.h>
#include <revolution/gx.h>
#include <revolution/mtx.h>



using namespace nw4r;

extern "C" {
int sprintf(char* s, const char* format, ...);
const char* fn_8003F79C();
void fn_800409EC(void* p);
}

static const GXColor cBaseTop = {0, 0, 0, 255};
static const GXColor cBaseBottom = {200, 140, 140, 255};
static const GXColor cBaseTopBlue = {0, 0, 0, 255};
static const GXColor cBaseBottomBlue = {133, 230, 255, 255};
static const GXColor cBaseTopMono = {0, 0, 0, 255};
static const GXColor cBaseBottomMono = {255, 255, 255, 255};
static const GXColor cBaseTopNavyAlt = {0, 0, 0, 255};
static const GXColor cBaseBottomNavyAlt = {0, 86, 167, 255};
static const GXColor cBaseTopNavy = {0, 0, 0, 255};
static const GXColor cBaseBottomNavy = {98, 35, 35, 255};
static const GXColor cBaseTopSelect = {140, 0, 0, 255};
static const GXColor cBaseBottomSelect = {255, 255, 255, 255};
static const GXColor cText = {216, 216, 216, 255};
static const GXColor cTextHover = {0, 0, 0, 255};
static const GXColor cTextSelect = {216, 216, 216, 255};
static const GXColor cIcon = {216, 216, 216, 255};
static const GXColor cIconHover = {0, 0, 0, 255};
static const GXColor cIconSelect = {216, 216, 216, 255};
static const GXColor cTagRed = {140, 0, 0, 255};
static const GXColor cTagBlue = {0, 0, 140, 255};

static const GXColor cHoverTop[10] = {
    {216, 216, 216, 255}, {255, 120, 0, 255}, {248, 190, 0, 255}, {0, 162, 222, 255},
    {71, 197, 40, 255},   {216, 216, 216, 255}, {216, 216, 216, 255}, {216, 216, 216, 255},
    {255, 255, 255, 255}, {255, 255, 255, 255},
};

static const GXColor cHoverBottom[10] = {
    {255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
    {255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
    {255, 255, 255, 255}, {80, 255, 50, 255},
};

static bool sAltColors;

static void SetTagProcessorRecursive(lyt::Pane* pane, ut::TagProcessorBase<wchar_t>* tagProcessor) {
    lyt::TextBox* textBox = ut::DynamicCast<lyt::TextBox*>(pane);
    if (textBox != NULL) {
        textBox->SetTagProcessor(tagProcessor);
    }

    lyt::PaneList& list = pane->GetChildList();
    for (lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); it++) {
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

static void SetMaterialColorRecursive(lyt::Pane* pane, const GXColor& color, int reg) {
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

ColorTagProcessor::ColorTagProcessor() : mSavedColor(0xFFFFFFFF) {}

ColorTagProcessor::~ColorTagProcessor() {}

ut::Operation ColorTagProcessor::Process(u16 code, ut::PrintContext<wchar_t>* context) {
    switch (code) {
    case 1:
        mSavedColor = context->writer->GetTextColor();
        context->writer->SetTextColor(cTagRed);
        return ut::OPERATION_NO_CHAR_SPACE;
    case 2:
        mSavedColor = context->writer->GetTextColor();
        context->writer->SetTextColor(cTagBlue);
        return ut::OPERATION_NO_CHAR_SPACE;
    case 9:
        context->writer->SetTextColor(mSavedColor);
        return ut::OPERATION_NO_CHAR_SPACE;
    default:
        return ut::TagProcessorBase<wchar_t>::Process(code, context);
    }
}

ut::Operation ColorTagProcessor::CalcRect(ut::Rect* pRect, u16 code, ut::PrintContext<wchar_t>* context) {
    switch (code) {
    case 1:
    case 2:
    case 9:
        return ut::OPERATION_NO_CHAR_SPACE;
    default:
        return ut::TagProcessorBase<wchar_t>::CalcRect(pRect, code, context);
    }
}

LayoutScreenItem::LayoutScreenItem(lyt::Pane* pane, const lyt::DrawInfo* drawInfo,
                               ut::TagProcessorBase<wchar_t>* tagProcessor, int noScale)
    : mPane(pane), mDrawInfo(drawInfo), mRect(0.0f, 0.0f, 0.0f, 0.0f), mTextColor(0xFFFFFFFF) {
    char name[32];

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
    if (rectPane != mPane) {
        math::VEC3 trans = rectPane->GetTranslate();
        mRect.left += trans.x;
        mRect.right += trans.x;
        mRect.top += trans.y;
        mRect.bottom += trans.y;
    }

    mUnk44 = false;
    mUnk45 = false;
    mToggle = false;
    mUnk47 = mInactive = false;
    mHidden = false;
    mDisabled = false;
    mUnk4B = false;
    mLinkA = NULL;
    mLinkB = NULL;
    mFixed = false;
    mColorSet = 0;

    for (int i = 0; i < 8; i++) {
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
                int n = mPane->GetUserData()[i] - '0';
                if (n >= 0 && n <= 9) {
                    mColorSet = n;
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
        sprintf(name, "%s%s", iconPane->GetName(), fn_8003F79C());
        mIconPane = iconPane->FindPaneByName(name, true);
        if (mIconPane == NULL) {
            mIconPane = iconPane;
        } else {
            lyt::PaneList& list = iconPane->GetChildList();
            for (lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); it++) {
                if (&*it != mIconPane) {
                    it->SetVisible(false);
                }
            }
        }
    }

    sprintf(name, "%sT", mPane->GetName());
    lyt::Pane* textPane = mPane->FindPaneByName(name, true);
    if (textPane == NULL) {
        mTextPane = NULL;
    } else {
        sprintf(name, "%s%s", textPane->GetName(), fn_8003F79C());
        mTextPane = textPane->FindPaneByName(name, true);
        lyt::PaneList& list = textPane->GetChildList();
        for (lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); it++) {
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

    sprintf(name, "%sM", mPane->GetName());
    lyt::Pane* markPane = mPane->FindPaneByName(name, true);
    if (markPane == NULL) {
        mMarkPane = NULL;
    } else {
        sprintf(name, "%s%s", markPane->GetName(), fn_8003F79C());
        mMarkPane = markPane->FindPaneByName(name, true);
        lyt::PaneList& list = markPane->GetChildList();
        for (lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); it++) {
            if (&*it != mMarkPane) {
                it->SetVisible(false);
            }
        }
    }

    if (mFixed && mMarkPane != NULL) {
        SetTagProcessorRecursive(mMarkPane, tagProcessor);
    }

    if (gWidescreen && !noScale) {
        f32 scale = 1.0f / (gWidescreen ? 1.3684211f : 1.0f);
        if (mTextPane != NULL) {
            math::VEC2 s = mTextPane->GetScale();
            s.x *= scale;
            mTextPane->SetScale(s);
        }
        if (mIconPane != NULL) {
            math::VEC2 s = mIconPane->GetScale();
            s.x *= scale;
            mIconPane->SetScale(s);
        }
        if (mFrame0Pane != NULL) {
            math::VEC2 s = mFrame0Pane->GetScale();
            s.x *= scale;
            mFrame0Pane->SetScale(s);
        }
        if (mFrame1Pane != NULL) {
            math::VEC2 s = mFrame1Pane->GetScale();
            s.x *= scale;
            mFrame1Pane->SetScale(s);
        }
        if (mMarkPane != NULL) {
            math::VEC2 s = mMarkPane->GetScale();
            s.x *= scale;
            mMarkPane->SetScale(s);
        }
    }

    lyt::TextBox* textBox = ut::DynamicCast<lyt::TextBox*>(mBasePane);
    if (textBox != NULL) {
        if (gWidescreen) {
            math::VEC2 s = textBox->GetScale();
            s.x *= 1.0f / (gWidescreen ? 1.3684211f : 1.0f);
            textBox->SetScale(s);
        }
        textBox->SetVisible(true);
        SetTagProcessorRecursive(textBox, tagProcessor);
    }

    mMarkIndex = -1;
    mMarkSubIndex = -1;
    mUnk84 = NULL;
    mUnk88 = NULL;
    mUnk8C = 0;
    mUnk90 = 0;
    mUnk94 = 0;
    Reset();
}

void LayoutScreenItem::Update() {
    if (mHidden) {
        return;
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
            int frame = mPressFrame > 16 ? 16 : mPressFrame;
            f32 rad = 1.5708f * (16 - frame) / 16.0f;
            offset = (mUnk4B ? 4.0f : 18.0f) * (1.0f - math::SinRad(rad));
        }
    } else {
        offset = 0.0f;
    }

    mPane->SetTranslate(mBasePos + math::VEC3(0.0f, mOffsetY - offset, 0.0f));

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
            mPressFrame--;
        }
        if (mPressFrame == 0) {
            mPressed = false;
        }
    } else if (mPressFrame > 1) {
        mPressFrame--;
    }

    if (mPressFrame > 10) {
        mHoverFrame = 12;
    }
}

static GXColor BlendColor(const GXColor& from, const GXColor& to, int t, int max);

#pragma dont_inline on
void LayoutScreenItem::Draw() {
    lyt::Pane* textPane = NULL;

    if (mHidden) {
        return;
    }

    if (!mFixed) {
        SetMaterialAlphaRecursive(mPane, mAlpha);
    }

    if (mTextPane != NULL) {
        lyt::PaneList& list = mTextPane->GetChildList();
        textPane = mTextPane;
        int i = 0;
        for (lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); it++) {
            if (mTextIndex == i) {
                textPane = &*it;
                break;
            }
            i++;
        }
        textPane->SetVisible(true);

        for (lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); it++) {
            if (&*it != textPane) {
                (*it).SetVisible(false);
            }
        }
    }

    if (mMarkPane != NULL) {
        lyt::Pane* markPane = GetMarkPane(mMarkIndex);
        lyt::PaneList& list = mMarkPane->GetChildList();
        for (lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); it++) {
            (*it).SetVisible(false);
        }
        if (markPane != NULL) {
            lyt::PaneList& subList = markPane->GetChildList();
            int i = 0;
            markPane->SetVisible(true);
            for (lyt::PaneList::Iterator it = subList.GetBeginIter(); it != subList.GetEndIter(); it++) {
                (*it).SetVisible(mMarkSubIndex == i);
                i++;
            }
        }
    }

    if (!mFixed) {
        GXColor top;
        GXColor bottom;
        GXColor baseTop;
        GXColor baseBottom;

        int baseAlpha;
        int iconAlpha;
        int textAlpha;
        if (mColorSet == 8 || mColorSet == 9) {
            baseAlpha = 128;
            iconAlpha = 255;
            textAlpha = 255;
        } else {
            baseAlpha = mAlpha * mFadeAlpha / 255;
            iconAlpha = mAlpha * (mFadeAlpha + 128 > 255 ? 255 : mFadeAlpha + 128) / 255;
            textAlpha = iconAlpha;
        }

        if (mBasePane != mPane) {
            if (mColorSet == 5) {
                baseTop = cBaseTopMono;
                baseBottom = cBaseBottomMono;
            } else if (mColorSet == 8 || mColorSet == 9) {
                if (sAltColors) {
                    baseTop = cBaseTopNavyAlt;
                    baseBottom = cBaseBottomNavyAlt;
                } else {
                    baseTop = cBaseTopNavy;
                    baseBottom = cBaseBottomNavy;
                }
            } else if (sAltColors || mColorSet == 7) {
                baseTop = cBaseTopBlue;
                baseBottom = cBaseBottomBlue;
            } else {
                baseTop = cBaseTop;
                baseBottom = cBaseBottom;
            }

            if (mSelected) {
                top = cBaseTopSelect;
                bottom = cBaseBottomSelect;
            } else if (mToggle && mPressFrame > 0) {
                top = cHoverTop[mColorSet];
                bottom = cHoverBottom[mColorSet];
            } else if (mHoverFrame > 0) {
                top = BlendColor(baseTop, cHoverTop[mColorSet], mHoverFrame, 12);
                bottom = BlendColor(baseBottom, cHoverBottom[mColorSet], mHoverFrame, 12);
            } else if (mPressFrame > 0) {
                top = cHoverTop[mColorSet];
                bottom = cHoverBottom[mColorSet];
            } else {
                top = baseTop;
                bottom = baseBottom;
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
        }

        if (mIconPane != NULL && mIconPane->GetUserData()[0] != 'F') {
            GXColor iconColor;
            if (mSelected) {
                iconColor = cIconSelect;
            } else if (mDisabled) {
                iconAlpha = 0;
            } else if (mToggle && mPressFrame > 0) {
                iconColor = cIconHover;
            } else if (mHoverFrame > 0) {
                iconColor = BlendColor(cIcon, cIconHover, mHoverFrame, 12);
            } else if (mPressFrame > 0) {
                iconColor = cIconHover;
            } else {
                iconColor = cIcon;
            }

            SetMaterialColorRecursive(mIconPane, iconColor, 0);
            SetMaterialColorRecursive(mIconPane, iconColor, 1);
            SetMaterialAlphaRecursive(mIconPane, iconAlpha);
        }

        if (textPane != NULL) {
            GXColor textColor;
            if (mSelected) {
                textColor = cTextSelect;
            } else if (mDisabled) {
                textAlpha = 0;
            } else if (mToggle && mPressFrame > 0) {
                textColor = cTextHover;
            } else if (mHoverFrame > 0) {
                textColor = BlendColor(cText, cTextHover, mHoverFrame, 12);
            } else if (mPressFrame > 0) {
                textColor = cTextHover;
            } else {
                textColor = cText;
            }

            textColor.a = textAlpha;
            SetTextColorRecursive(textPane, textColor);
            mTextColor = textColor;
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

    mPane->Draw(*mDrawInfo);
}
#pragma dont_inline reset

static GXColor BlendColor(const GXColor& from, const GXColor& to, int t, int max) {
    GXColor color;
    color.r = (from.r * (max - t) + to.r * t) / max;
    color.g = (from.g * (max - t) + to.g * t) / max;
    color.b = (from.b * (max - t) + to.b * t) / max;
    return color;
}

void LayoutScreenItem::SetHover() {
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

lyt::Pane* LayoutScreenItem::GetMarkPane(int index) {
    lyt::PaneList& list = mMarkPane->GetChildList();
    int i = 0;
    for (lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); it++) {
        if (index == i) {
            return &*it;
        }
        i++;
    }
    return NULL;
}

LayoutScreen::LayoutScreen(void* archive, const char* layoutName, int noScale) {
    mResAccessor = new lyt::ArcResourceAccessor();
    mResAccessor->Attach(archive, "arc");

    mLayout = new lyt::Layout();
    void* res = mResAccessor->GetResource(0, layoutName, NULL);
    mLayout->Build(res, mResAccessor);

    mDrawInfo = new lyt::DrawInfo();
    mDrawInfo->SetViewRect(mLayout->GetLayoutRect());
    math::MTX34 viewMtx;
    PSMTXIdentity(viewMtx.mtx);
    mDrawInfo->SetViewMtx(viewMtx);

    mTagProcessor = new ColorTagProcessor();

    mItemCount = 0;
    lyt::PaneList& list = mLayout->GetRootPane()->GetChildList();
    for (lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); it++) {
        if (mItemCount >= 64) {
            break;
        }
        mItems[mItemCount++] = new LayoutScreenItem(&*it, mDrawInfo, mTagProcessor, noScale);
    }

    Reset();
}

LayoutScreen::~LayoutScreen() {
    for (int i = 0; i < mItemCount; i++) {
        LayoutScreenItem* item = mItems[i];
        if (item != NULL) {
            if (item->mUnk88 != NULL) {
                fn_800409EC(item->mUnk88);
            }
            if (item->mUnk84 != NULL) {
                fn_800409EC(item->mUnk84);
            }
            delete item;
        }
    }

    delete mTagProcessor;
    delete mDrawInfo;
    delete mLayout;
    mResAccessor->Detach();
    delete mResAccessor;
}

void LayoutScreen::Reset() {
    for (int i = 0; i < mItemCount; i++) {
        mItems[i]->Reset();
    }
    mFadeOut = false;
    mFadeLength = 15;
    mFadeFrame = 0;
}

void LayoutScreen::Calc() {
    for (int i = 0; i < mItemCount; i++) {
        mItems[i]->Update();
    }

    if (mFadeOut) {
        if (mFadeFrame < mFadeLength) {
            mFadeFrame++;
        }
    } else if (mFadeFrame > 0) {
        mFadeFrame--;
    }

    f32 height;
    if (mItemCount > 0) {
        height = __fabsf(mItems[0]->mRect.top - mItems[0]->mRect.bottom);
    } else {
        height = 0.0f;
    }

    f32 step = height * mFadeFrame / mFadeLength;
    for (int i = 0; i < mItemCount; i++) {
        LayoutScreenItem* item = mItems[i];
        f32 offset = step * (item->mPane->GetTranslate().y > 0.0f ? 1 : -1);
        if (!item->mInactive) {
            item->mOffsetY = offset;
        }
    }

    if (mAlphaFadeOut) {
        if (mAlphaFadeFrame < mAlphaFadeLength) {
            mAlphaFadeFrame++;
        }
    } else if (mAlphaFadeFrame > 0) {
        mAlphaFadeFrame--;
    }

    int alpha = 255 - mAlphaFadeFrame * 255 / mAlphaFadeLength;
    for (int i = 0; i < mItemCount; i++) {
        mItems[i]->mAlpha = alpha;
    }
}

void LayoutScreen::Draw() {
    f32 near = 0.0f;
    f32 far = 1.0f;
    Mtx44 projMtx;

    mLayout->CalculateMtx(*mDrawInfo);
    if (mLayout->GetOriginType() == 1) {
        near = -near;
        far = -far;
    }

    C_MTXOrtho(projMtx, mLayout->GetLayoutRect().top, mLayout->GetLayoutRect().bottom,
               mLayout->GetLayoutRect().left, mLayout->GetLayoutRect().right, near, far);
    GXSetProjection(projMtx, GX_ORTHOGRAPHIC);
    GXSetNumChans(1);
    GXSetCullMode(GX_CULL_NONE);
    GXSetZMode(GX_FALSE, GX_NEVER, GX_FALSE);

    for (int i = 0; i < mItemCount; i++) {
        mItems[i]->Draw();
    }
}
