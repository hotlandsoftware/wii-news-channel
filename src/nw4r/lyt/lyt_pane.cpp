#include <nw4r/lyt/lyt_pane.h>

#include <nw4r/lyt/lyt_common.h>
#include <nw4r/lyt/lyt_layout.h>
#include <nw4r/lyt/lyt_material.h>

#include <revolution/gx.h>
#include <revolution/mtx.h>
#include <string.h>

#define DEG_TO_RAD(deg) ((deg) * (3.1415927f / 180.0f))

namespace {

using namespace nw4r;

void ReverseYAxis(math::MTX34* pMtx) {
    pMtx->m[0][1] = -pMtx->m[0][1];
    pMtx->m[1][1] = -pMtx->m[1][1];
    pMtx->m[2][1] = -pMtx->m[2][1];
}

} // namespace

namespace nw4r {
namespace lyt {

NW4R_UT_GET_RUNTIME_TYPEINFO(Pane);

Pane::Pane(const res::Pane* pBlock) {
    Init();

    mBasePosition = pBlock->basePosition;
    SetName(pBlock->name);
    SetUserData(pBlock->userData);

    mTranslate = pBlock->translate;
    mRotate = pBlock->rotate;
    mScale = pBlock->scale;
    mSize = pBlock->size;
    mAlpha = pBlock->alpha;
    mGlbAlpha = mAlpha;
    mFlag = pBlock->flag;
}

void Pane::Init() {
    mpParent = NULL;
    mpMaterial = NULL;
    mbUserAllocated = false;
}

Pane::~Pane() {
    for (PaneList::Iterator it = mChildList.GetBeginIter(); it != mChildList.GetEndIter();) {
        PaneList::Iterator currIt = it++;
        mChildList.Erase(currIt);

        if (!currIt->IsUserAllocated()) {
            currIt->~Pane();
            Layout::FreeMemory(&*currIt);
        }
    }

    UnbindAnimationSelf(NULL);

    if (mpMaterial && !mpMaterial->IsUserAllocated()) {
        mpMaterial->~Material();
        Layout::FreeMemory(mpMaterial);
    }
}

void Pane::SetName(const char* name) {
    strncpy(mName, name, sizeof(mName));
}

void Pane::SetUserData(const char* userData) {
    strncpy(mUserData, userData, sizeof(mUserData));
}

void Pane::AppendChild(Pane* pChild) {
    InsertChild(mChildList.GetEndIter(), pChild);
}

void Pane::InsertChild(PaneList::Iterator next, Pane* pChild) {
    mChildList.Insert(next, pChild);
    pChild->mpParent = this;
}

#pragma ppc_iro_level 0

ut::Rect Pane::GetPaneRect(const DrawInfo& drawInfo) const {
    ut::Rect ret(0.0f, 0.0f, 0.0f, 0.0f);
    math::VEC2 basePt = GetVtxPos();

    ret.left = basePt.x;
    ret.top = basePt.y;
    ret.right = ret.left + mSize.width;
    ret.bottom = ret.top + mSize.height;

    if (drawInfo.IsYAxisUp()) {
        ret.top = -ret.top;
        ret.bottom = -ret.bottom;
    }

    return ret;
}

#pragma ppc_iro_level reset

ut::Color Pane::GetVtxColor(u32) const {
    return ut::Color(0xFFFFFFFF);
}

void Pane::SetVtxColor(u32, ut::Color) {}

u8 Pane::GetColorElement(u32 idx) const {
    switch (idx) {
    case ANIMTARGET_PANE_COLOR_ALPHA:
        return mAlpha;

    default:
        return GetVtxColorElement(idx);
    }
}

void Pane::SetColorElement(u32 idx, u8 value) {
    switch (idx) {
    case ANIMTARGET_PANE_COLOR_ALPHA:
        mAlpha = value;
        break;

    default:
        SetVtxColorElement(idx, value);
        break;
    }
}

u8 Pane::GetVtxColorElement(u32) const {
    return 0xFF;
}

void Pane::SetVtxColorElement(u32, u8) {}

Pane* Pane::FindPaneByName(const char* findName, bool bRecursive) {
    if (detail::EqualsPaneName(mName, findName)) {
        return this;
    }

    if (bRecursive) {
        for (PaneList::Iterator it = mChildList.GetBeginIter(); it != mChildList.GetEndIter(); it++) {
            if (Pane* pane = it->FindPaneByName(findName, true)) {
                return pane;
            }
        }
    }

    return NULL;
}

Material* Pane::FindMaterialByName(const char* findName, bool bRecursive) {
    if (mpMaterial && detail::EqualsMaterialName(mpMaterial->GetName(), findName)) {
        return mpMaterial;
    }

    if (bRecursive) {
        for (PaneList::Iterator it = mChildList.GetBeginIter(); it != mChildList.GetEndIter(); it++) {
            if (Material* pMaterial = it->FindMaterialByName(findName, true)) {
                return pMaterial;
            }
        }
    }

    return NULL;
}

void Pane::CalculateMtx(const DrawInfo& drawInfo) {
    const f32 invAlpha = 1.0f / 255.0f;

    if (!detail::TestBit(mFlag, VISIBLE) && !drawInfo.IsInvisiblePaneCalculateMtx()) {
        return;
    }

    {
        math::MTX34 mtx1;
        math::MTX34 mtx2;
        math::MTX34 rotateMtx;

        {
            math::VEC2 scale(mScale);

            if (drawInfo.IsLocationAdjust() && detail::TestBit(mFlag, LOCATION_ADJUST)) {
                scale.x *= drawInfo.GetLocationAdjustScale().x;
                scale.y *= drawInfo.GetLocationAdjustScale().y;
            }

            PSMTXScale(mtx2.mtx, scale.x, scale.y, 1.0f);
        }

        PSMTXRotRad(rotateMtx.mtx, 'x', DEG_TO_RAD(mRotate.x));
        PSMTXConcat(rotateMtx.mtx, mtx2.mtx, mtx1.mtx);

        PSMTXRotRad(rotateMtx.mtx, 'y', DEG_TO_RAD(mRotate.y));
        PSMTXConcat(rotateMtx.mtx, mtx1.mtx, mtx2.mtx);

        PSMTXRotRad(rotateMtx.mtx, 'z', DEG_TO_RAD(mRotate.z));
        PSMTXConcat(rotateMtx.mtx, mtx2.mtx, mtx1.mtx);

        PSMTXTransApply(mtx1.mtx, mMtx.mtx, mTranslate.x, mTranslate.y, mTranslate.z);
    }

    if (mpParent) {
        math::MTX34Mult(&mGlbMtx, &mpParent->mGlbMtx, &mMtx);
    } else if (drawInfo.IsMultipleViewMtxOnDraw()) {
        mGlbMtx = mMtx;
    } else {
        math::MTX34Mult(&mGlbMtx, &drawInfo.GetViewMtx(), &mMtx);
    }

    if (drawInfo.IsInfluencedAlpha() && mpParent) {
        mGlbAlpha = mAlpha * drawInfo.GetGlobalAlpha();
    } else {
        mGlbAlpha = mAlpha;
    }

    f32 crGlobalAlpha = drawInfo.GetGlobalAlpha();
    bool bCrInfluenced = drawInfo.IsInfluencedAlpha();

    bool bModDrawInfo = detail::TestBit(mFlag, INFLUENCED_ALPHA) && mAlpha != 0xFF;

    if (bModDrawInfo) {
        DrawInfo& mtDrawInfo = const_cast<DrawInfo&>(drawInfo);
        mtDrawInfo.SetGlobalAlpha(crGlobalAlpha * mAlpha * invAlpha);
        mtDrawInfo.SetInfluencedAlpha(true);
    }

    CalculateMtxChild(drawInfo);

    if (bModDrawInfo) {
        DrawInfo& mtDrawInfo = const_cast<DrawInfo&>(drawInfo);
        mtDrawInfo.SetGlobalAlpha(crGlobalAlpha);
        mtDrawInfo.SetInfluencedAlpha(bCrInfluenced);
    }
}

void Pane::CalculateMtxChild(const DrawInfo& drawInfo) {
    for (PaneList::Iterator it = mChildList.GetBeginIter(); it != mChildList.GetEndIter(); it++) {
        it->CalculateMtx(drawInfo);
    }
}

void Pane::Draw(const DrawInfo& drawInfo) {
    if (detail::TestBit(mFlag, VISIBLE)) {
        DrawSelf(drawInfo);

        for (PaneList::Iterator it = mChildList.GetBeginIter(); it != mChildList.GetEndIter(); it++) {
            it->Draw(drawInfo);
        }
    }
}

// No debug drawing in this revision.
void Pane::DrawSelf(const DrawInfo&) {}

void Pane::Animate(u32 option) {
    AnimateSelf(option);

    if (detail::TestBit(mFlag, VISIBLE) || !(option & ANIMOPTION_SKIP_INVISIBLE)) {
        for (PaneList::Iterator it = mChildList.GetBeginIter(); it != mChildList.GetEndIter(); it++) {
            it->Animate(option);
        }
    }
}

void Pane::AnimateSelf(u32 option) {
    for (AnimationLinkList::Iterator it = mAnimList.GetBeginIter(); it != mAnimList.GetEndIter(); it++) {
        if (it->IsEnable()) {
            AnimTransform* animTrans = it->GetAnimTransform();
            animTrans->Animate(it->GetIndex(), this);
        }
    }

    if ((detail::TestBit(mFlag, VISIBLE) || !(option & ANIMOPTION_SKIP_INVISIBLE)) && mpMaterial) {
        mpMaterial->Animate();
    }
}

void Pane::BindAnimation(AnimTransform* pAnimTrans, bool bRecursive) {
    pAnimTrans->Bind(this, bRecursive);
}

void Pane::UnbindAnimation(AnimTransform* pAnimTrans, bool bRecursive) {
    UnbindAnimationSelf(pAnimTrans);

    if (bRecursive) {
        for (PaneList::Iterator it = mChildList.GetBeginIter(); it != mChildList.GetEndIter(); it++) {
            it->UnbindAnimation(pAnimTrans, bRecursive);
        }
    }
}

void Pane::UnbindAllAnimation(bool bRecursive) {
    UnbindAnimation(NULL, bRecursive);
}

void Pane::UnbindAnimationSelf(AnimTransform* pAnimTrans) {
    if (mpMaterial) {
        mpMaterial->UnbindAnimation(pAnimTrans);
    }

    for (AnimationLinkList::Iterator it = mAnimList.GetBeginIter(); it != mAnimList.GetEndIter();) {
        AnimationLinkList::Iterator currIt = it++;

        if (pAnimTrans == NULL || currIt->GetAnimTransform() == pAnimTrans) {
            mAnimList.Erase(currIt);
            currIt->Reset();
        }
    }
}

void Pane::AddAnimationLink(AnimationLink* pAnimationLink) {
    mAnimList.PushBack(pAnimationLink);
}

AnimationLink* Pane::FindAnimationLink(AnimTransform* pAnimTrans) {
    if (AnimationLink* ret = detail::FindAnimationLink(&mAnimList, pAnimTrans)) {
        return ret;
    }

    if (mpMaterial) {
        if (AnimationLink* ret = mpMaterial->FindAnimationLink(pAnimTrans)) {
            return ret;
        }
    }

    return NULL;
}

void Pane::SetAnimationEnable(AnimTransform* pAnimTrans, bool bEnable, bool bRecursive) {
    if (AnimationLink* pAnimLink = detail::FindAnimationLink(&mAnimList, pAnimTrans)) {
        pAnimLink->SetEnable(bEnable);
    }

    if (mpMaterial) {
        mpMaterial->SetAnimationEnable(pAnimTrans, bEnable);
    }

    if (bRecursive) {
        for (PaneList::Iterator it = mChildList.GetBeginIter(); it != mChildList.GetEndIter(); it++) {
            it->SetAnimationEnable(pAnimTrans, bEnable, bRecursive);
        }
    }
}

void Pane::LoadMtx(const DrawInfo& drawInfo) {
    math::MTX34 mtx;
    MtxPtr mtxPtr = NULL;

    if (drawInfo.IsMultipleViewMtxOnDraw()) {
        math::MTX34Mult(&mtx, &drawInfo.GetViewMtx(), &mGlbMtx);

        if (drawInfo.IsYAxisUp()) {
            ReverseYAxis(&mtx);
        }

        mtxPtr = mtx.mtx;
    } else if (drawInfo.IsYAxisUp()) {
        math::MTX34Copy(&mtx, &mGlbMtx);
        ReverseYAxis(&mtx);
        mtxPtr = mtx.mtx;
    } else {
        mtxPtr = mGlbMtx.mtx;
    }

    GXLoadPosMtxImm(mtxPtr, 0);
    GXSetCurrentMtx(0);
}

math::VEC2 Pane::GetVtxPos() const {
    math::VEC2 basePt(0.0f, 0.0f);

    switch (mBasePosition % 3) {
    default:
    case 0:
        basePt.x = 0.0f;
        break;

    case 1:
        basePt.x = -mSize.width / 2.0f;
        break;

    case 2:
        basePt.x = -mSize.width;
        break;
    }

    switch (mBasePosition / 3) {
    default:
    case 0:
        basePt.y = 0.0f;
        break;

    case 1:
        basePt.y = -mSize.height / 2.0f;
        break;

    case 2:
        basePt.y = -mSize.height;
        break;
    }

    return basePt;
}

Material* Pane::GetMaterial() const {
    return mpMaterial;
}

} // namespace lyt
} // namespace nw4r
