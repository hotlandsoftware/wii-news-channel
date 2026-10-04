/**
 * CodeWarrior compatibility for the native PC build.
 *
 * This header is force-included (-include) in front of every translation unit
 * of the PC build, so that the shared sources need as few `#ifdef TARGET_PC`
 * blocks as possible. It is never seen by the Wii build.
 *
 * The rules that go with it are in docs/pc_port.md.
 */

#ifndef PC_COMPAT_H
#define PC_COMPAT_H

#ifndef TARGET_PC
#error "include/pc/compat.h is only for the PC build (TARGET_PC)"
#endif

/* The port is 32-bit for now: file-overlay structs hold pointers. */
#if !defined(__i386__)
#error "The PC port must be built as 32-bit x86 (-m32) for now"
#endif

#include <types.h>

/* Layout assumptions shared with the Wii build. */
#ifdef __cplusplus
static_assert(sizeof(void*) == 4, "pointers must be 4 bytes");
static_assert(sizeof(wchar_t) == 2, "wchar_t must be 16 bits (-fshort-wchar)");
static_assert(sizeof(u32) == 4 && sizeof(s32) == 4, "u32/s32");
static_assert(sizeof(u64) == 8 && alignof(u64) == 8, "u64 is 8-aligned");
static_assert(sizeof(f64) == 8 && alignof(f64) == 8, "f64 is 8-aligned");
static_assert(sizeof(long) == 4, "long must be 4 bytes");
#endif

/******************************************************************************
 *
 * Declaration specifiers
 *
 ******************************************************************************/

/* __declspec(section ".init"), __declspec(weak), __declspec(noreturn)... */
#define __declspec(x)

/* include/macros.h: DECL_SECTION / DECL_WEAK expand to __declspec */

/******************************************************************************
 *
 * Compiler intrinsics (PowerPC instructions CodeWarrior exposes as functions)
 *
 ******************************************************************************/

#ifdef __cplusplus
extern "C++" {
#endif

/* Rotate left word immediate then mask insert: bits mb..me (bit 0 = MSB). */
static inline unsigned int pc_rlwimi_impl(unsigned int dst, unsigned int src,
                                          int sh, int mb, int me) {
    unsigned int rot = sh ? ((src << sh) | (src >> (32 - sh))) : src;
    unsigned int mask_b = 0xFFFFFFFFu >> mb;
    unsigned int mask_e = 0xFFFFFFFFu << (31 - me);
    unsigned int mask = (mb <= me) ? (mask_b & mask_e) : (mask_b | mask_e);
    return (dst & ~mask) | (rot & mask);
}
/* CodeWarrior's __rlwimi returns the new value; it does not modify `dst`. */
#define __rlwimi(dst, src, sh, mb, me)                                         \
    pc_rlwimi_impl((unsigned int)(dst), (unsigned int)(src), (sh), (mb), (me))

static inline int __cntlzw(unsigned int x) {
    return x ? __builtin_clz(x) : 32;
}

static inline double __fabs(double x) {
    return __builtin_fabs(x);
}
static inline float __fabsf(float x) {
    return __builtin_fabsf(x);
}
static inline double __fnabs(double x) {
    return -__builtin_fabs(x);
}
static inline int __abs(int x) {
    return x < 0 ? -x : x;
}

/* CodeWarrior's spelling of decltype/typeof */
#define __decltype__(x) __typeof__(x)

/* MSL's name for alloca() */
#define __alloca(n) __builtin_alloca(n)

/* __memclr(ptr, size): MEMCLR() in macros.h */
#define __memclr(p, n) __builtin_memset((p), 0, (n))

#ifdef __cplusplus
} /* extern "C++" */
#endif

/* psq_l / psq_st conversions (U16ToF32 and friends) */
#include <pc/fastcast.h>

/******************************************************************************
 *
 * Reporting from the backend
 *
 ******************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

/* Prints "unimplemented: NAME" the first time it is called for a call site
 * (`*once` is the flag). Used by the generated stubs and by hand-written
 * placeholders in src/pc. */
void PCUnimplemented(const char* name, unsigned char* once);

#ifdef __cplusplus
}
#endif

#define PC_UNIMPLEMENTED()                                                     \
    do {                                                                       \
        static unsigned char once_;                                            \
        PCUnimplemented(__func__, &once_);                                     \
    } while (0)

#endif /* PC_COMPAT_H */
