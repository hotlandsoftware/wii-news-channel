#define NW4R_MATH_VEC2_NO_DTOR
#include <news/MathUtil.h>
#include <nw4r/math/math_arithmetic.h>
#include <nw4r/math/math_triangular.h>
#include <nw4r/math/math_types.h>
#include <revolution/mtx.h>
#include <revolution/os.h>
#include <revolution/net.h>

using namespace nw4r;

// Scratch matrix shared by the model/camera code.
math::MTX34 gWorkMtx;

static const wchar_t sDigits[] = L"0123456789";

// The SDK fast cast (OSu16tof32).
static inline f32 U16ToF32(register u16* in) {
    register f32 ret;
    asm {
        psq_l ret, 0(in), 1, 3
    }
    return ret;
}

static inline BOOL IsLeapYear(s32 year) {
    return year % 4 == 0;
}

void WrapHour(s32* hour) {
    *hour %= 24;
}

static inline BOOL NotEqual(const math::VEC2& a, const math::VEC2& b) {
    return a.x != b.x || a.y != b.y;
}

static inline void SubP(math::VEC2* out, const math::VEC2* a, const math::VEC2* b) {
    out->x = a->x - b->x;
    out->y = a->y - b->y;
}

static inline f32 LenV(math::VEC2 v) {
    return math::FSqrt(v.x * v.x + v.y * v.y);
}
static inline f32 LenR(const math::VEC2& v) {
    return math::FSqrt(v.x * v.x + v.y * v.y);
}
static inline f32 LenP(const math::VEC2* v) {
    return math::FSqrt(v->x * v->x + v->y * v->y);
}
f32 Ease(math::VEC2* value, const math::VEC2* target, f32 rate, f32 maxStep, f32 minStep) {
    math::VEC2 e;
    if (NotEqual(*value, *target)) {
        math::VEC2 diff;
        diff = math::VEC2(value->x - target->x, value->y - target->y);
        f32 len = LenV(diff);
        math::VEC2 d = diff;
        if (len < minStep) {
            *value = *target;
        } else {
            f32 step = len * rate;
            d.x *= rate;
            d.y *= rate;
            if (IsNearlyZero(step)) {
                *value = *target;
            } else {
                if (step > maxStep) {
                    f32 s = maxStep / step;
                    d.x *= s;
                    d.y *= s;
                } else if (step < minStep) {
                    f32 s = minStep / step;
                    d.x *= s;
                    d.y *= s;
                }
                value->x -= d.x;
                value->y -= d.y;
            }
        }
    }
    SubP(&e, value, target);
    return LenP(&e);
}

f32 Ease(f32* value, f32 target, f32 rate, f32 maxStep, f32 minStep) {
    if (*value != target) {
        f32 d = *value - target;
        f32 dist = __fabsf(d);
        if (dist < minStep) {
            *value = target;
        } else {
            f32 step = dist * rate;
            d *= rate;
            if (IsNearlyZero(step)) {
                *value = target;
            } else {
                if (step > maxStep) {
                    if (d < 0.0f) {
                        maxStep = -maxStep;
                    }
                    d = maxStep;
                } else if (step < minStep) {
                    if (d < 0.0f) {
                        minStep = -minStep;
                    }
                    d = minStep;
                }
                *value -= d;
            }
        }
    }
    return __fabsf(*value - target);
}

static inline void WrapDegrees(f32* angle) {
    if (*angle < 0.0f) {
        *angle += 360.0f;
    } else if (*angle >= 360.0f) {
        *angle -= 360.0f;
    }
}

f32 EaseAngle(f32* value, f32 target, f32 rate, f32 maxStep, f32 minStep) {
    WrapDegrees(value);
    WrapDegrees(&target);
    if (*value != target) {
        f32 d = *value - target;
        if (d < -180.0f) {
            d += 360.0f;
        } else if (d > 180.0f) {
            d -= 360.0f;
        }
        d *= rate;
        f32 dist = __fabsf(d);
        if (dist <= minStep) {
            *value = target;
        } else {
            if (dist > maxStep) {
                if (d < 0.0f) {
                    maxStep = -maxStep;
                }
                d = maxStep;
            }
            *value -= d;
            WrapDegrees(value);
        }
    }
    return __fabsf(*value - target);
}

void Chase(f32* value, f32 target, f32 step) {
    if (*value > target) {
        *value -= step;
        if (*value < target) {
            *value = target;
        }
    } else if (*value < target) {
        *value += step;
        if (*value > target) {
            *value = target;
        }
    }
}

f32 CosineEase(u16 angle) {
    return 0.5f - 0.5f * math::CosFIdx(0.00390625f * U16ToF32(&angle));
}

s32 SplitDigits(s32 value, s32* digits, s32 maxDigits) {
    s32 i;
    s32 n = 0;
    for (s32 v = value; v != 0; v /= 10) {
        n++;
    }
    if (n > maxDigits) {
        n = maxDigits;
    }
    for (i = 0; i < n; i++) {
        digits[i] = value % 10;
        value /= 10;
    }
    for (; i < maxDigits; i++) {
        digits[i] = 0;
    }
    return n;
}

wchar_t* FormatNumber(s32 value, wchar_t* buf, s32 width, BOOL zeroPad) {
    s32 digits[10];
    if (width > 10) {
        width = 10;
    }
    if (value < 0) {
        *buf++ = L'-';
        value -= value * 2;
    }
    SplitDigits(value, digits, width);
    for (s32 i = width - 1; i >= 0; i--) {
        if (!zeroPad && i != 0) {
            if (digits[i] != 0) {
                zeroPad = TRUE;
                *buf++ = sDigits[digits[i]];
            }
        } else {
            zeroPad = TRUE;
            *buf++ = sDigits[digits[i]];
        }
    }
    *buf = 0;
    return buf;
}

static inline s32 GetDaysInMonth(s32 month, s32 year) {
    if (month == 4 || month == 6 || month == 9 || month == 11) {
        return 30;
    } else if (month == 2) {
        if (year % 4 == 0) {
            return 29;
        } else {
            return 28;
        }
    } else {
        return 31;
    }
}
static inline s32 GetDaysInYear(s32 year) {
    return year % 4 == 0 ? 366 : 365;
}

void MinutesToCalendarTime(u32 minutes, OSCalendarTime* time) {
    u32 hours = minutes / 60;
    s32 days = (s32)hours / 24;
    s32 totalDays = days;
    s32 year = 0;
    s32 month = 1;
    while (true) {
        s32 n = year % 4 == 0 ? 366 : 365;
        if (days < n) {
            break;
        }
        days -= n;
        year++;
    }
    s32 yday = days;
    while (true) {
        s32 n = GetDaysInMonth(month, year);
        if (days < n) {
            break;
        }
        days -= n;
        month++;
    }
    time->mday = days + 1;
    time->sec = 0;
    time->mon = month - 1;
    time->yday = yday;
    time->msec = 0;
    time->year = year + 2000;
    time->usec = 0;
    time->min = minutes % 60;
    time->hour = (s32)hours % 24;
    time->wday = (totalDays + 6) % 7;
}

u32 GetCurrentMinutes() {
    OSCalendarTime cal;
    NETGetUniversalCalendar(&cal);
    s32 year = cal.year;
    s32 month = cal.mon + 1;
    s32 days = cal.mday;
    s32 hour = cal.hour;
    s32 min = cal.min;
    days += (year - 2000) * 365;
    for (s32 i = 0; i < year - 2000; i += 4) {
        days++;
    }
    for (s32 m = 1; m < month; m++) {
        if (m == 4 || m == 6 || m == 9 || m == 11) {
            days += 30;
        } else if (m == 2) {
            if (((year - 2000) & 3) == 0) {
                days += 29;
            } else {
                days += 28;
            }
        } else {
            days += 31;
        }
    }
    return (days - 1) * 1440 + hour * 60 + min;
}

void Mtx_Translate(math::MTX34* mtx, f32 x, f32 y, f32 z) {
    Mtx m;
    PSMTXTrans(m, x, y, z);
    PSMTXConcat(m, *mtx, *mtx);
}

void Mtx_RotateXDeg(math::MTX34* mtx, f32 angle) {
    Mtx m;
    PSMTXRotRad(m, 'x', NW4R_MATH_DEG_TO_RAD(angle));
    PSMTXConcat(m, *mtx, *mtx);
}

void Mtx_RotateYDeg(math::MTX34* mtx, f32 angle) {
    Mtx m;
    PSMTXRotRad(m, 'y', NW4R_MATH_DEG_TO_RAD(angle));
    PSMTXConcat(m, *mtx, *mtx);
}

void Mtx_RotateZDeg(math::MTX34* mtx, f32 angle) {
    Mtx m;
    PSMTXRotRad(m, 'z', NW4R_MATH_DEG_TO_RAD(angle));
    PSMTXConcat(m, *mtx, *mtx);
}

void Mtx_RotateDeg(math::MTX34* mtx, f32 x, f32 y, f32 z) {
    math::MTX34 m;
    math::MTX34RotXYZFIdx(&m, NW4R_MATH_DEG_TO_FIDX(x), NW4R_MATH_DEG_TO_FIDX(y),
                          NW4R_MATH_DEG_TO_FIDX(z));
    PSMTXConcat(m, *mtx, *mtx);
}

void Mtx_SetRotZDeg(math::MTX34* mtx, f32 angle) {
    PSMTXRotRad(*mtx, 'z', NW4R_MATH_DEG_TO_RAD(angle));
}
