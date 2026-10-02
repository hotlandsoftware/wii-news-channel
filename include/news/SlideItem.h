#ifndef NEWS_SLIDE_ITEM_H
#define NEWS_SLIDE_ITEM_H

#include <types.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_Color.h>

// An entry of a vertical list that eases towards its target position
// (allocated as an array by the list code at 0x80027674).
class SlideItem {
public:
    typedef void (SlideItem::*StateFunc)();

    SlideItem();
    ~SlideItem();

    void Update(const nw4r::math::VEC2* offset, const f32* scale);
    void SetMode(s32 mode);
    void SetTarget(f32 x, f32 y);
    void StateMove();
    void StateDrop();

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

    BOOL IsState(StateFunc state) { return mState == state; }

public:
    u16 m00;                      // at 0x00
    nw4r::math::VEC2 mPos;        // at 0x04
    nw4r::math::VEC2 mTarget;     // at 0x0C
    f32 m14;                      // at 0x14
    f32 m18;                      // at 0x18
    f32 m1C;                      // at 0x1C
    f32 m20;                      // at 0x20
    u32 m24;                      // at 0x24
    u32 m28;                      // at 0x28
    u32 m2C;                      // at 0x2C
    f32 m30;                      // at 0x30
    f32 m34;                      // at 0x34
    f32 m38;                      // at 0x38
    f32 m3C;                      // at 0x3C
    nw4r::ut::Color mColor;       // at 0x40
    StateFunc mState;             // at 0x44
    u32 m50;                      // at 0x50
    f32 mScale;                   // at 0x54
    f32 m58;                      // at 0x58
    f32 mDistance;                // at 0x5C
    u8 m60;                       // at 0x60
    u8 m61;                       // at 0x61
    u16 mIndex;                   // at 0x62
    s32 mPhase;                   // at 0x64
    s32 mMode;                    // at 0x68
    u32 m6C;                      // at 0x6C
};

// Eases *value towards target (both components), see Ease(). Returns the
// remaining distance.
extern "C" f32 fn_80044534(nw4r::math::VEC2* value, const nw4r::math::VEC2* target, f32 rate,
                           f32 maxStep, f32 minStep);

#endif
