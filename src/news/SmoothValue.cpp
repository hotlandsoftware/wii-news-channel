#include <news/SmoothValue.h>

SmoothValue::SmoothValue() : mValue(0.0f), mTarget(0.0f), mStep(0.0f) {}

void SmoothValue::Update() {
    if (mValue > mTarget) {
        mValue -= mStep;
        if (mValue < mTarget) {
            mValue = mTarget;
        }
    } else if (mValue < mTarget) {
        mValue += mStep;
        if (mValue > mTarget) {
            mValue = mTarget;
        }
    }
}
