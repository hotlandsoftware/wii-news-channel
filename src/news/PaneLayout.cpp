#include <news/PaneLayout.h>
#include <news/ColorTagProcessor.h>
#include <news/PaneButton.h>
#include <news/System.h>
#include <nw4r/lyt/lyt_arcResourceAccessor.h>
#include <nw4r/lyt/lyt_drawInfo.h>
#include <nw4r/lyt/lyt_layout.h>
#include <revolution/gx.h>
#include <revolution/mtx.h>
#include <string.h>

using namespace nw4r;

static inline f32 GetAspect() { return gWidescreen ? 832.0f / 608.0f : 1.0f; }

Layout::Layout(void* arc, const char* name, PaneButtonColors* colors, bool influencedAlpha) {
    mFadeLength = 0;
    mFadeFrame = 0;
    mAlpha = 255;

    mResAccessor = new lyt::ArcResourceAccessor();
    mResAccessor->Attach(arc, "arc");

    mLayout = new lyt::Layout();
    void* res = mResAccessor->GetResource(0, name, NULL);
    mLayout->Build(res, mResAccessor);

    mDrawInfo = new lyt::DrawInfo();
    mDrawInfo->SetInfluencedAlpha(influencedAlpha);
    mDrawInfo->SetLocationAdjust(true);
    mDrawInfo->SetLocationAdjustScale(math::VEC2(1.0f / GetAspect(), 1.0f));
    mDrawInfo->SetViewRect(mLayout->GetLayoutRect());
    math::MTX34 viewMtx;
    PSMTXIdentity(viewMtx.mtx);
    mDrawInfo->SetViewMtx(viewMtx);

    mTagProcessor = new ColorTagProcessor();

    lyt::PaneList& list = mLayout->GetRootPane()->GetChildList();
    mButtonCount = 0;
    for (lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); it++) {
        if (mButtonCount >= MAX_BUTTONS) {
            break;
        }
        mButtons[mButtonCount++] = new PaneButton(&*it, mDrawInfo, colors, mTagProcessor);
    }

    Reset();
}

Layout::~Layout() {
    for (int i = 0; i < mButtonCount; i++) {
        delete mButtons[i];
    }

    delete mTagProcessor;
    delete mDrawInfo;
    delete mLayout;
    mResAccessor->Detach();
    delete mResAccessor;
}

void Layout::Reset() {
    for (int i = 0; i < mButtonCount; i++) {
        mButtons[i]->Reset();
    }
    mSlideOut = false;
    mSlideLength = 15;
    mSlideFrame = 0;
}

inline void Layout::SetSlide(f32 step) {
    for (int i = 0; i < mButtonCount; i++) {
        mButtons[i]->SetSlide(step);
    }
}

void Layout::Calc() {
    for (int i = 0; i < mButtonCount; i++) {
        mButtons[i]->UpdateFrame();
    }

    if (mSlideOut) {
        if (mSlideFrame < mSlideLength) {
            mSlideFrame++;
        }
    } else if (mSlideFrame > 0) {
        mSlideFrame--;
    }

    f32 height;
    if (mButtonCount > 0) {
        height = __fabsf(mButtons[0]->mRect.top - mButtons[0]->mRect.bottom);
    } else {
        height = 0.0f;
    }

    SetSlide(height * mSlideFrame / mSlideLength);

    if (mFadeOut) {
        if (mFadeFrame < mFadeLength) {
            mFadeFrame++;
        }
    } else if (mFadeFrame > 0) {
        mFadeFrame--;
    }

    mAlpha = 255 - mFadeFrame * 255 / mFadeLength;
    SetButtonAlpha(mAlpha);
}

void Layout::SetButtonAlpha(s32 alpha) {
    for (int i = 0; i < mButtonCount; i++) {
        mButtons[i]->SetBaseAlpha(alpha);
    }
}

inline void Layout::UpdatePanes() {
    for (int i = 0; i < mButtonCount; i++) {
        mButtons[i]->UpdatePane();
    }
}

void Layout::Draw() {
    f32 near = 0.0f;
    f32 far = 1.0f;
    Mtx44 projMtx;

    if (mLayout->GetOriginType() == 1) {
        near = -near;
        far = -far;
    }

    ut::Rect rect = mLayout->GetLayoutRect();
    C_MTXOrtho(projMtx, rect.top, rect.bottom, rect.left, rect.right, near, far);
    GXSetProjection(projMtx, GX_ORTHOGRAPHIC);
    GXSetNumChans(1);
    GXSetCullMode(GX_CULL_NONE);
    GXSetZMode(GX_FALSE, GX_NEVER, GX_FALSE);

    UpdatePanes();

    mLayout->CalculateMtx(*mDrawInfo);

    for (int i = 0; i < mButtonCount; i++) {
        mButtons[i]->Draw();
    }
}

PaneButton* Layout::HitTest(f32 x, f32 y) {
    if (mFadeOut && mFadeFrame >= mFadeLength) {
        return NULL;
    }

    math::VEC3 pos(x, y, 0.0f);
    Mtx inv;
    PSMTXInverse(mDrawInfo->GetViewMtx(), inv);
    PSMTXMultVec(inv, pos, pos);

    for (int i = mButtonCount - 1; i >= 0; i--) {
        if (mButtons[i]->HitTest(pos.x, pos.y)) {
            return mButtons[i];
        }
    }
    return NULL;
}

static inline bool IsNamed(PaneButton* button, const char* name) {
    return strcmp(button->mPane->GetName(), name) == 0;
}

PaneButton* Layout::FindButton(const char* name) {
    for (int i = 0; i < mButtonCount; i++) {
        if (IsNamed(mButtons[i], name)) {
            return mButtons[i];
        }
    }
    return NULL;
}

void Layout::SlideIn(s32 frames) {
    mSlideOut = false;
    if (frames != mSlideLength) {
        mSlideFrame = mSlideFrame * frames / mSlideLength;
        mSlideLength = frames;
    }
}

void Layout::SlideOut(s32 frames) {
    mSlideOut = true;
    if (frames != mSlideLength) {
        mSlideFrame = mSlideFrame * frames / mSlideLength;
        mSlideLength = frames;
    }
}

void Layout::FadeIn(s32 frames) {
    mFadeOut = false;
    if (frames != mFadeLength) {
        mFadeFrame = mFadeFrame * frames / mFadeLength;
        mFadeLength = frames;
    }
}

void Layout::SetBlend(s32 alpha, s32 blend, s32 blendMax) {
    for (int i = 0; i < mButtonCount; i++) {
        mButtons[i]->SetBlend(alpha, blend, blendMax);
    }
}

void Layout::SetAlpha(s32 alpha) {
    for (int i = 0; i < mButtonCount; i++) {
        mButtons[i]->SetAlpha(alpha);
    }
}

void Layout::SetViewMtx(const Mtx mtx) {
    mDrawInfo->SetViewMtx(*(const math::MTX34*)mtx);
}
