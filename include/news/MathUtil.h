#ifndef NEWS_MATH_UTIL_H
#define NEWS_MATH_UTIL_H

#include <types.h>

// Moves *value towards target by at most step.
void Chase(f32* value, f32 target, f32 step);

// Moves *value towards target by rate * distance, clamped to [minStep, maxStep]
// (snapping once closer than minStep). Returns the remaining distance.
f32 Ease(f32* value, f32 target, f32 rate, f32 maxStep, f32 minStep);

inline BOOL IsNearlyZero(f32 x) {
    return x < 0.0008f && x > -0.0008f;
}

#endif
