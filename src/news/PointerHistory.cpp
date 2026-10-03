#include <news/PointerHistory.h>
#include <news/System.h>

PointerHistory::PointerHistory() {
    mIndex = 0;
}

PointerHistory::Sample::~Sample() {}

void PointerHistory::Reset() {
    mIndex = 0;
    for (s32 i = 0; i < NUM_CHANNELS; i++) {
        for (s32 j = 0; j < NUM_SAMPLES; j++) {
            mSamples[i][j].x = 0.0f;
            mSamples[i][j].y = 0.0f;
            mSamples[i][j].valid = false;
        }
    }
}

void PointerHistory::Update() {
    for (s32 i = 0; i < NUM_CHANNELS; i++) {
        mSamples[i][mIndex].x = gCursorX[i][0];
        mSamples[i][mIndex].y = gCursorY[i][0];
        mSamples[i][mIndex].valid = gPointerValid[i][0] && gKPADLatest[i] >= 0;
    }

    mIndex++;
    if (mIndex >= NUM_SAMPLES) {
        mIndex = 0;
    }
}

BOOL PointerHistory::GetOldest(s32 chan, f32* x, f32* y) {
    s32 idx = mIndex;
    for (s32 i = 0; i < NUM_SAMPLES; i++) {
        Sample* sample = &mSamples[chan][idx];
        if (sample->valid) {
            *x = sample->x;
            *y = sample->y;
            return TRUE;
        }
        idx++;
        if (idx >= NUM_SAMPLES) {
            idx = 0;
        }
    }
    return FALSE;
}
