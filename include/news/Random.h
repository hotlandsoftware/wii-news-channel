#ifndef NEWS_RANDOM_H
#define NEWS_RANDOM_H

#include <types.h>
#include <revolution/os/OSTime.h>

// Linear congruential generator, also stirred with the time base.
extern u32 gRandSeed;

inline u32 Random() {
    gRandSeed = gRandSeed * 1664525 + 1013904223 + (u32)OSGetTime();
    return gRandSeed;
}

// Returns a float in [0, max).
inline f32 RandomF(f32 max) {
    u32 bits = (Random() >> 9) | 0x3F800000;
    return (*(f32*)&bits - 1.0f) * max;
}

#endif
