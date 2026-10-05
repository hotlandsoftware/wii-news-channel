#ifndef NEWS_SCROLLER_H
#define NEWS_SCROLLER_H

#include <types.h>

// Scrolls a line that is wider than its view: waits, scrolls to the end,
// then eases back to the start.
class Scroller {
public:
    typedef void (Scroller::*StateFunc)();

    enum Mode {
        MODE_WAIT,
        MODE_SCROLL,
        MODE_RETURN,
    };

    Scroller();
    void Reset();
    void Update();

    void StateWait();
    void StateScroll();
    void StateReturn();

private:
    void ChangeState(StateFunc state) {
        if (mState) {
            mPhase = -1;
            (this->*mState)();
        }
        mState = state;
        mPhase = 0;
        if (mState) {
            (this->*mState)();
        }
    }

public:
    StateFunc mState;     // at 0x00
    PC_PMF_PAD(mState)
    s32 mPhase;           // at 0x0C
    s32 mMode;            // at 0x10
    BOOL mActive;         // at 0x14
    s32 mTimer;           // at 0x18
    f32 mViewWidth;       // at 0x1C
    f32 mContentWidth;    // at 0x20
    f32 mPos;             // at 0x24
    f32 mVelocity;        // at 0x28
    f32 mTargetVelocity;  // at 0x2C
};

#endif
