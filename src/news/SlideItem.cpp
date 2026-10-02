#include <news/SlideItem.h>
#include <news/MathUtil.h>
#include <news/System.h>

#pragma explicit_zero_data on
static f32 sStartX = 0.0f;
static f32 sStartY = 0.0f;
static f32 sTargetX = 0.0f;
static f32 sTargetY = 0.0f;
#pragma explicit_zero_data off

SlideItem::SlideItem()
    : m28(0), m2C(0), m30(0.0f), m34(0.0f), m38(0.0f), m3C(0.0f), mColor(0, 0, 0, 255),
      mState(NULL), mScale(1.0f), m58(1.0f), mDistance(0.0f), m60(0), m61(0), mPhase(0), mMode(0),
      m6C(0) {
    m00 = 0;
    mPos.x = sStartX;
    mPos.y = sStartY;
    mTarget.x = sTargetX;
    mTarget.y = sTargetY;
    m14 = 1.0f;
    m18 = 0.0f;
    m1C = 0.0f;
    ChangeState(&SlideItem::StateMove);
}

SlideItem::~SlideItem() {}

void SlideItem::Update(const nw4r::math::VEC2* offset, const f32* scale) {
    f32 margin = 114.0f;
    f32 top = -margin;
    f32 bottom = margin + GetScreenHeight();
    f32 y = offset->y + mTarget.y;
    mScale = *scale;
    if (y < top || y > bottom) {
        mPos.x = mTarget.x;
        mPos.y = mTarget.y;
        if (!IsState(&SlideItem::StateMove)) {
            ChangeState(&SlideItem::StateMove);
        }
    } else if (mState) {
        (this->*mState)();
    }
}

void SlideItem::SetMode(s32 mode) {
    if (mMode != mode) {
        mMode = mode;
        ChangeState(&SlideItem::StateDrop);
    }
}

void SlideItem::SetTarget(f32 x, f32 y) {
    mTarget.x = x;
    mTarget.y = y;
}

void SlideItem::StateMove() {
    switch (mPhase) {
    case 0:
        mPhase++;
        break;
    case -1:
        break;
    default:
        fn_80044534(&mPos, &mTarget, 0.12f, 1000.0f, 0.1f);
        break;
    }
}

void SlideItem::StateDrop() {
    switch (mPhase) {
    case 0:
        mPhase++;
        mDistance = __fabsf(mTarget.x - mPos.x);
        break;
    case -1:
        break;
    default:
        nw4r::math::VEC2 target = mTarget;
        f32 dist = __fabsf(mTarget.x - mPos.x);
        f32 k = 0.0625f * (mIndex & 0xF);
        target.y += (0.0025f + 0.0025f * k) * (dist * dist);
        if (IsNearlyZero(fn_80044534(&mPos, &target, 0.2f - 0.11f * k, 1000.0f, 0.1f))) {
            ChangeState(&SlideItem::StateMove);
        }
        break;
    }
}
