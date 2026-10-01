#include <news/Scroller.h>
#include <news/MathUtil.h>

Scroller::Scroller() : mState(NULL), mPhase(0) {
    mActive = FALSE;
    mViewWidth = 0.0f;
    mContentWidth = 0.0f;
    mVelocity = 0.0f;
    mTargetVelocity = 0.0f;
    ChangeState(&Scroller::StateWait);
}

void Scroller::Reset() {
    mActive = FALSE;
    mPos = 0.0f;
    ChangeState(&Scroller::StateWait);
}

void Scroller::Update() {
    if (mState) {
        (this->*mState)();
    }
}

void Scroller::StateWait() {
    switch (mPhase) {
    case 0:
        mPhase++;
        mMode = MODE_WAIT;
        mVelocity = mTargetVelocity = 0.0f;
        break;
    case -1:
        break;
    default:
        if (mContentWidth > mViewWidth && mActive) {
            ChangeState(&Scroller::StateScroll);
        }
        break;
    }
}

void Scroller::StateScroll() {
    switch (mPhase) {
    case 0:
        mPhase++;
        mMode = MODE_SCROLL;
        mTimer = 20;
        mVelocity = mTargetVelocity = 0.0f;
        break;
    case -1:
        break;
    default:
        if (!mActive) {
            ChangeState(&Scroller::StateReturn);
        } else {
            Chase(&mVelocity, mTargetVelocity, 0.2f);
            mPos += mVelocity;
            switch (mPhase) {
            case 1:
                if (mTimer != 0) {
                    mTimer--;
                } else {
                    mPhase++;
                    mTargetVelocity = -2.0f;
                }
                break;
            case 2:
                if (mPos < mViewWidth - mContentWidth) {
                    mPhase++;
                    mTimer = 60;
                    mTargetVelocity = 0.0f;
                }
                break;
            }
        }
        break;
    }
}

void Scroller::StateReturn() {
    switch (mPhase) {
    case 0:
        mPhase++;
        mMode = MODE_RETURN;
        mVelocity = mTargetVelocity = 0.0f;
        break;
    case -1:
        break;
    default:
        if (IsNearlyZero(Ease(&mPos, 0.0f, 0.1f, 16.0f, 0.5f))) {
            mPos = 0.0f;
            if (mActive) {
                ChangeState(&Scroller::StateScroll);
            } else {
                ChangeState(&Scroller::StateWait);
            }
        }
        break;
    }
}
