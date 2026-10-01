#ifndef NW4R_G3D_PLATFORM_CPU_H
#define NW4R_G3D_PLATFORM_CPU_H
#include <types.h>
#include <macros.h>

#include <revolution/os.h>

#ifndef ASM
#define ASM asm
#endif

namespace nw4r {
namespace g3d {

/******************************************************************************
 *
 * Fastcast
 *
 ******************************************************************************/
namespace fastcast {

// ogws OSFastCast.h conversions (our os/OSFastCast.h lacks them; kept local
// to g3d so the shared OS header stays unchanged)
namespace detail {

inline f32 OSu8tof32_(register u8* in) {
    register f32 ret;
    ASM (
        psq_l ret, 0(in), 1, 2
    )
    return ret;
}

inline f32 OSu16tof32_(register u16* in) {
    register f32 ret;
    ASM (
        psq_l ret, 0(in), 1, 3
    )
    return ret;
}

inline u8 OSf32tou8_(register f32 arg) {
    f32 a;
    register f32* ptr = &a;
    u8 r;
    ASM (
        psq_st arg, 0(ptr), 1, 2
    )
    r = *(u8*)ptr;
    return r;
}

inline void u8tof32(u8* in, volatile f32* out) {
    *out = OSu8tof32_(in);
}

inline void u16tof32(u16* in, volatile f32* out) {
    *out = OSu16tof32_(in);
}

inline void f32tou8(f32* in, volatile u8* out) {
    *out = OSf32tou8_(*in);
}

} // namespace detail

/******************************************************************************
 *
 * Convert from U8
 *
 ******************************************************************************/
inline f32 U8_0ToF32(const u8* pPtr) {
    f32 x;
    detail::u8tof32(const_cast<u8*>(pPtr), &x);
    return x;
}

/******************************************************************************
 *
 * Convert from U16
 *
 ******************************************************************************/
inline f32 U16_0ToF32(const u16* pPtr) {
    f32 x;
    detail::u16tof32(const_cast<u16*>(pPtr), &x);
    return x;
}

/******************************************************************************
 *
 * Convert from S16
 *
 ******************************************************************************/
inline f32 S7_8ToF32(register const s16* pPtr) {
    register f32 f;

    ASM (
        psq_l f, 0(pPtr), 1, 7
    )

    return f;
}

inline f32 S10_5ToF32(register const s16* pPtr) {
    register f32 f;

    ASM (
        psq_l f, 0(pPtr), 1, 6
    )

    return f;
}

/******************************************************************************
 *
 * Convert from F32
 *
 ******************************************************************************/
inline u8 F32ToU8_0(f32 f) {
    u8 x;
    detail::f32tou8(&f, &x);
    return x;
}

inline s16 F32ToS10_5(register f32 f) {
    s16 x;
    register s16* pPtr = &x;

    ASM (
        psq_st f, 0(pPtr), 1, 6
    )

    return x;
}

/******************************************************************************
 *
 * GQR
 *
 ******************************************************************************/
// ogws OSFastCast.h OSSetGQR6/OSSetGQR7 (OS_GQR_TYPE_S16 = 7)
namespace detail {

inline void SetGQR6(register u32 type, register u32 scale) {
    register u32 val = ((scale << 8 | type) << 16) | ((scale << 8) | type);
    ASM (
        mtspr 0x396, val
    )
}

inline void SetGQR7(register u32 type, register u32 scale) {
    register u32 val = ((scale << 8 | type) << 16) | ((scale << 8) | type);
    ASM (
        mtspr 0x397, val
    )
}

} // namespace detail

inline void SetGQR6_S10_5() {
    detail::SetGQR6(7, 5);
}
inline void SetGQR7_S7_8() {
    detail::SetGQR7(7, 8);
}

/******************************************************************************
 *
 * Initialization
 *
 ******************************************************************************/
namespace detail {

inline void Init() {
    OSInitFastCast();
    SetGQR6_S10_5();
    SetGQR7_S7_8();
}

} // namespace detail
} // namespace fastcast

/******************************************************************************
 *
 * Cache
 *
 ******************************************************************************/
namespace DC {

inline void StoreRange(void* pBase, u32 size) {
    DCStoreRange(pBase, size);
}

inline void StoreRangeNoSync(void* pBase, u32 size) {
    DCStoreRangeNoSync(pBase, size);
}

inline void FlushRangeNoSync(void* pBase, u32 size) {
    DCFlushRangeNoSync(pBase, size);
}

inline void InvalidateRange(void* pBase, u32 size) {
    DCInvalidateRange(pBase, size);
}

} // namespace DC

/******************************************************************************
 *
 * Memory
 *
 ******************************************************************************/
namespace detail {

void Copy32ByteBlocks(void* pDst, const void* pSrc, u32 size);
void ZeroMemory32ByteBlocks(void* pDst, u32 size);

} // namespace detail

/******************************************************************************
 *
 * Initialize fastcast
 *
 ******************************************************************************/
inline void InitFastCast() {
    fastcast::detail::Init();
}

} // namespace g3d
} // namespace nw4r

#endif
