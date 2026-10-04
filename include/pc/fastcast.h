/**
 * C versions of the paired-single "fast cast" loads and stores.
 *
 * On the Wii, `psq_l`/`psq_st` through GQR2..GQR5 (set up by OSInitFastCast)
 * convert between f32 and u8/u16/s8/s16: loads are exact, stores truncate
 * toward zero and saturate to the range of the integer type. The game, NW4R
 * and the SDK use this through small asm inlines (U16ToF32, OSf32tou16...).
 * Their TARGET_PC versions call these functions.
 */

#ifndef PC_FASTCAST_H
#define PC_FASTCAST_H

#include <types.h>

#ifdef __cplusplus
extern "C++" {
#endif

static inline f32 PCFastCastU8ToF32(u8 x) {
    return (f32)x;
}
static inline f32 PCFastCastU16ToF32(u16 x) {
    return (f32)x;
}
static inline f32 PCFastCastS8ToF32(s8 x) {
    return (f32)x;
}
static inline f32 PCFastCastS16ToF32(s16 x) {
    return (f32)x;
}

/* NaN converts like the PowerPC does for the unsigned case (0); the signed
 * case is not relied on by any caller. */
static inline s32 PCFastCastSaturate(f32 x, s32 lo, s32 hi) {
    if (!(x > (f32)lo)) {
        return lo;
    }
    if (x >= (f32)hi) {
        return hi;
    }
    return (s32)x; /* truncates toward zero */
}

static inline u8 PCFastCastF32ToU8(f32 x) {
    return (u8)PCFastCastSaturate(x, 0, 0xFF);
}
static inline u16 PCFastCastF32ToU16(f32 x) {
    return (u16)PCFastCastSaturate(x, 0, 0xFFFF);
}
static inline s8 PCFastCastF32ToS8(f32 x) {
    return (s8)PCFastCastSaturate(x, -0x80, 0x7F);
}
static inline s16 PCFastCastF32ToS16(f32 x) {
    return (s16)PCFastCastSaturate(x, -0x8000, 0x7FFF);
}

#ifdef __cplusplus
} /* extern "C++" */
#endif

#endif /* PC_FASTCAST_H */
