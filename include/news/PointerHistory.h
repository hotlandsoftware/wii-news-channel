#ifndef NEWS_POINTER_HISTORY_H
#define NEWS_POINTER_HISTORY_H

#include <types.h>

// Ring buffer of the last pointer positions of every channel
// (PointerHistory.cpp, 0x8004857C). The global instance lives in d_scene.cpp.
class PointerHistory {
public:
    enum {
        NUM_CHANNELS = 4,
        NUM_SAMPLES = 10,
    };

    struct Sample {
        Sample() : x(0.0f), y(0.0f), valid(false) {}
        ~Sample();

        f32 x;      // at 0x0
        f32 y;      // at 0x4
        bool valid; // at 0x8
    };

    PointerHistory();

    void Reset();
    void Update();
    BOOL GetOldest(s32 chan, f32* x, f32* y);

    Sample mSamples[NUM_CHANNELS][NUM_SAMPLES]; // at 0x000
    s32 mIndex;                                 // at 0x1E0
};

extern PointerHistory gPointerHistory;

#endif
