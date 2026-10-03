#ifndef NEWS_MATH_UTIL_H
#define NEWS_MATH_UTIL_H

#include <types.h>

namespace nw4r {
namespace math {
struct VEC2;
struct MTX34;
} // namespace math
} // namespace nw4r

// Moves *value towards target by at most step.
void Chase(f32* value, f32 target, f32 step);

// Moves *value towards target by rate * distance, clamped to [minStep, maxStep]
// (snapping once closer than minStep). Returns the remaining distance.
f32 Ease(f32* value, f32 target, f32 rate, f32 maxStep, f32 minStep);

inline BOOL IsNearlyZero(f32 x) {
    return x < 0.0008f && x > -0.0008f;
}

// The rest of MathUtil.cpp (0x80044508-0x80045238).

// *hour %= 24
void WrapHour(s32* hour);

// Ease() for both components of a VEC2 (the step is clamped on the length).
f32 Ease(nw4r::math::VEC2* value, const nw4r::math::VEC2* target, f32 rate, f32 maxStep,
         f32 minStep);

// Ease() for angles in degrees, both kept in [0, 360) and moving the short way.
f32 EaseAngle(f32* value, f32 target, f32 rate, f32 maxStep, f32 minStep);

// 0.5 - 0.5 * cos(angle): 0 -> 0, 0x8000 -> 1, 0x10000 -> 0 again.
f32 CosineEase(u16 angle);

// Writes the lowest maxDigits decimal digits (least significant first, zero
// filled) and returns the number of significant digits (at most maxDigits).
s32 SplitDigits(s32 value, s32* digits, s32 maxDigits);

// Writes value as at most width digits (leading zeros only with zeroPad) and a
// terminating 0; returns the position of the terminator.
wchar_t* FormatNumber(s32 value, wchar_t* buf, s32 width, BOOL zeroPad);

// Current UTC time (NETGetUniversalCalendar) in minutes since 2000-01-01.
u32 GetCurrentMinutes();

// mtx = op * mtx
void Mtx_Translate(nw4r::math::MTX34* mtx, f32 x, f32 y, f32 z);
void Mtx_RotateXDeg(nw4r::math::MTX34* mtx, f32 angle);
void Mtx_RotateYDeg(nw4r::math::MTX34* mtx, f32 angle);
void Mtx_RotateZDeg(nw4r::math::MTX34* mtx, f32 angle);
void Mtx_RotateDeg(nw4r::math::MTX34* mtx, f32 x, f32 y, f32 z);
// mtx = rotation about z
void Mtx_SetRotZDeg(nw4r::math::MTX34* mtx, f32 angle);

#endif
