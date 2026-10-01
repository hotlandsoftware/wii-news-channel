#ifndef NW4R_UT_ALGORITHM_H
#define NW4R_UT_ALGORITHM_H

#include <types.h>

namespace nw4r {
namespace ut {

template <typename T> inline T Min(T a, T b) {
    return (a > b) ? b : a;
}

template <typename T> inline T Max(T a, T b) {
    return (a < b) ? b : a;
}

template <typename T> inline T Clamp(T x, T low, T high) {
    return (x > high) ? high : ((x < low) ? low : x);
}

template <typename T> inline T Abs(T x) {
    return x < 0 ? static_cast<T>(-x) : static_cast<T>(x);
}

template <typename T> inline T BitExtract(T bits, int pos, int len) {
    T mask = (1 << len) - 1;
    return (bits >> pos) & mask;
}

inline u32 GetIntPtr(const void* pPtr) {
    return reinterpret_cast<u32>(pPtr);
}

template <typename T> inline const void* AddOffsetToPtr(const void* base, T offset) {
    return reinterpret_cast<const void*>(GetIntPtr(base) + offset);
}

template <typename T> inline void* AddOffsetToPtr(void* base, T offset) {
    return reinterpret_cast<void*>(GetIntPtr(base) + offset);
}

inline s32 GetOffsetFromPtr(const void* start, const void* end) {
    return static_cast<s32>(GetIntPtr(end) - GetIntPtr(start));
}

} // namespace ut
} // namespace nw4r

#endif
