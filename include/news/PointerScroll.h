#ifndef NEWS_POINTER_SCROLL_H
#define NEWS_POINTER_SCROLL_H

#include <types.h>

// Scrolling by holding B and pointing above or below the centre of the
// screen: per channel, a repeat timer that fires faster the further the
// pointer is from the centre line (y = 228, 20 px dead zone).
class PointerScroll {
public:
    PointerScroll();
    ~PointerScroll() {}

    void Init() {
        for (s32 i = 0; i < 4; i++) {
            mDelta[i] = 0.0f;
            mTimer[i] = 0;
            mDir[i] = 0;
        }
        mActive = false;
    }

    void Reset();
    void UpdateChannel(s32 chan);
    void Update();

    f32 mDelta[4]; // at 0x00: 228 - pointer y (0 inside the dead zone)
    s32 mTimer[4]; // at 0x10: frames until the next step
    s32 mDir[4];   // at 0x20: step this frame: 0 none, 1 up, 2 down
    bool mActive;  // at 0x30: B was held on some channel
};

#endif
