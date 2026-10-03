#include <news/ArticleText.h>
#include <news/MathUtil.h>
#include <news/System.h>

// Eases *value towards target (both components), see Ease(). Returns the
// remaining distance.
extern "C" f32 fn_80044534(nw4r::math::VEC2* value, const nw4r::math::VEC2* target, f32 rate,
                           f32 maxStep, f32 minStep);

#pragma explicit_zero_data on
static f32 sStartX = 0.0f;
static f32 sStartY = 0.0f;
static f32 sTargetX = 0.0f;
static f32 sTargetY = 0.0f;
#pragma explicit_zero_data off

TextChar::TextChar()
    : mPrev(NULL), mNext(NULL), mLeft(0.0f), mTop(0.0f), mRight(0.0f), mBottom(0.0f),
      mColor(0, 0, 0, 255), mState(NULL), mRate(1.0f), mScaleX(1.0f), mUnk5C(0.0f), mHidden(false),
      mSelected(false), mStateFrame(0), mLine(0), mWordIndex(0) {
    mChar = 0;
    mPos.x = sStartX;
    mPos.y = sStartY;
    mTarget.x = sTargetX;
    mTarget.y = sTargetY;
    mScale = 1.0f;
    mWidth = 0.0f;
    mHeight = 0.0f;
    ChangeState(&TextChar::StateMove);
}

TextChar::~TextChar() {}

void TextChar::Update(const nw4r::math::VEC2* offset, const f32* scale) {
    f32 margin = 114.0f;
    f32 top = -margin;
    f32 bottom = margin + GetScreenHeight();
    f32 y = offset->y + mTarget.y;
    mRate = *scale;
    if (y < top || y > bottom) {
        mPos.x = mTarget.x;
        mPos.y = mTarget.y;
        if (!IsState(&TextChar::StateMove)) {
            ChangeState(&TextChar::StateMove);
        }
    } else if (mState) {
        (this->*mState)();
    }
}

void TextChar::SetLine(s32 line) {
    if (mLine != line) {
        mLine = line;
        ChangeState(&TextChar::StateDrop);
    }
}

void TextChar::SetTarget(f32 x, f32 y) {
    mTarget.x = x;
    mTarget.y = y;
}

void TextChar::StateMove() {
    switch (mStateFrame) {
    case 0:
        mStateFrame++;
        break;
    case -1:
        break;
    default:
        fn_80044534(&mPos, &mTarget, 0.12f, 1000.0f, 0.1f);
        break;
    }
}

void TextChar::StateDrop() {
    switch (mStateFrame) {
    case 0:
        mStateFrame++;
        mUnk5C = __fabsf(mTarget.x - mPos.x);
        break;
    case -1:
        break;
    default:
        nw4r::math::VEC2 target = mTarget;
        f32 dist = __fabsf(mTarget.x - mPos.x);
        f32 k = 0.0625f * (mWordChar & 0xF);
        target.y += (0.0025f + 0.0025f * k) * (dist * dist);
        if (IsNearlyZero(fn_80044534(&mPos, &target, 0.2f - 0.11f * k, 1000.0f, 0.1f))) {
            ChangeState(&TextChar::StateMove);
        }
        break;
    }
}
