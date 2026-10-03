#include <news/PointerScroll.h>
#include <news/System.h>
#include <nw4r/math/math_triangular.h>

using namespace nw4r;


static inline BOOL IsInDeadZone(f32 d) {
    return d < 20.0f && d > -20.0f;
}

static inline f32 CosRad(f32 rad) {
    return math::CosFIdx(40.743664f * rad);
}

PointerScroll::PointerScroll() {
    Init();
}

void PointerScroll::Reset() {
    Init();
}

void PointerScroll::UpdateChannel(s32 chan) {
    f32* delta = &mDelta[chan];
    s32* timer = &mTimer[chan];
    s32* dir = &mDir[chan];
    f32 d = 228.0f - gCursorY[chan][0];
    *delta = d;
    if (IsInDeadZone(d)) {
        *delta = 0.0f;
        return;
    }
    if (*timer > 0) {
        (*timer)--;
    }
    if (*timer == 0) {
        *dir = *delta > 0.0f ? 1 : 2;
        f32 max = 185.2f;
        f32 t = __fabsf(*delta) - 20.0f;
        if (t > max) {
            t = max;
        }
        *timer = (s32)(23.0f * CosRad(1.5707964f * t / max)) + 1;
    }
}

void PointerScroll::Update() {
    s32* timer = mTimer;
    s32* dir = mDir;
    for (s32 i = 0; i < 4; i++, dir++, timer++) {
        *dir = 0;
        if (gHold[i] & 0x400) {
            mActive = true;
            UpdateChannel(i);
        } else {
            *timer = 0;
        }
    }
}
