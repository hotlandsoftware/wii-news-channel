/**
 * Common macros
 */

#ifndef MACROS_H
#define MACROS_H

/******************************************************************************
 *
 * Strings
 *
 ******************************************************************************/

// Stringify expression
#define __STR(x) #x
#define STR(x) __STR(x)

// Concatenate strings
#define __CONCAT(x, y) x##y
#define CONCAT(x, y) __CONCAT(x, y)

// Multi-character character constants
// clang-format off
#define TWOCC(c0, c1)                                                          \
    (u32)((c0 & 0xFF) << 8  | (c1 & 0xFF))
#define THREECC(c0, c1, c2)                                                    \
    (u32)((c0 & 0xFF) << 16 | (c1 & 0xFF) << 8  | (c2 & 0xFF))
#define FOURCC(c0, c1, c2, c3)                                                 \
    (u32)((c0 & 0xFF) << 24 | (c1 & 0xFF) << 16 | (c2 & 0xFF) << 8 | (c3 & 0xFF))
// clang-format on

/******************************************************************************
 *
 * Arithmetic
 *
 ******************************************************************************/

// Min/max expression
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

// Clamp to a range
#define CLAMP(low, high, x)                                                    \
    ((x) > (high) ? (high) : ((x) < (low) ? (low) : (x)))

// Round up value
#define ROUND_UP(x, align) (((x) + (align) - 1) & (-(align)))
#define ROUND_UP_PTR(x, align)                                                 \
    ((void*)((((u32)(x)) + (align) - 1) & (~((align) - 1))))

// Round down value
#define ROUND_DOWN(x, align) ((x) & (-(align)))
#define ROUND_DOWN_PTR(x, align) ((void*)(((u32)(x)) & (~((align) - 1))))

// Distance between pointers
#define PTR_DISTANCE(start, end) ((u8*)(end) - (u8*)(start))

/******************************************************************************
 *
 * Arrays
 *
 ******************************************************************************/

// Size of compile-time arrays
#define ARRAY_SIZE(x) (sizeof((x)) / sizeof((x)[0]))
#define LENGTHOF(x) ARRAY_SIZE(x)

// Declare an array of hardware registers
#define DECL_HW_REGS(NAME) FLEXIBLE_ARRAY(NAME##_HW_REGS)

/******************************************************************************
 *
 * Intrinsics
 *
 ******************************************************************************/

// Memory clear intrinsic
#define MEMCLR(x) __memclr((x), sizeof(*(x)))

/******************************************************************************
 *
 * Attributes
 *
 ******************************************************************************/

// Alignment attribute
#define ALIGN(x) __attribute__((aligned(x)))

// Place a symbol in a specific ELF section
#define DECL_SECTION(x) __declspec(section x)

// Give a symbol weak linkage
#define DECL_WEAK __declspec(weak)

/******************************************************************************
 *
 * RVL SDK / Petari compatibility
 *
 * Everything below is guarded with #ifndef because MetroTRK (trk.h) and the
 * command line (-D__REGISTER=register) may define some of these already.
 *
 ******************************************************************************/

#ifndef ATTRIBUTE_ALIGN
#define ATTRIBUTE_ALIGN(num) __attribute__((aligned(num)))
#endif

#ifndef ATTRIBUTE_PACKED
#define ATTRIBUTE_PACKED __attribute__((packed))
#endif

// (GC/2.7, used for MetroTRK, rejects __attribute__((weak)))
#ifndef ATTRIBUTE_WEAK
#if defined(__MWERKS__) && __MWERKS__ >= 0x3000 && !defined(METRO_TRK)
#define ATTRIBUTE_WEAK __attribute__((weak))
#else
#define ATTRIBUTE_WEAK
#endif
#endif

#ifndef ATTRIBUTE_UNUSED
#define ATTRIBUTE_UNUSED __attribute__((unused))
#endif

// Function inlining control (not supported by MWCC < 3.0, e.g. GC/2.7)
#ifndef ALWAYS_INLINE
#if defined(__MWERKS__) && __MWERKS__ >= 0x3000 && !defined(METRO_TRK)
#define ALWAYS_INLINE __attribute__((always_inline))
#define NO_INLINE __attribute__((noinline))
#else
#define ALWAYS_INLINE
#define NO_INLINE
#endif
#endif

// ogws spelling of NO_INLINE (used by headers/sources ported from ogws)
#ifndef DECOMP_DONT_INLINE
#if defined(__MWERKS__) && __MWERKS__ >= 0x3000 && !defined(METRO_TRK)
#define DECOMP_DONT_INLINE __attribute__((never_inline))
#else
#define DECOMP_DONT_INLINE
#endif
#endif

// Place a variable at a fixed address: `vu32 REG AT_ADDRESS(0xCC000000);`
#ifndef AT_ADDRESS
#ifdef __MWERKS__
#define AT_ADDRESS(x) : x
#else
#define AT_ADDRESS(x)
#endif
#endif

// ogws spelling
#ifndef DECL_ADDRESS
#define DECL_ADDRESS(x) AT_ADDRESS(x)
#endif

#ifndef __REGISTER
#ifdef __MWERKS__
#define __REGISTER register
#else
#define __REGISTER
#endif
#endif

// Unsigned element count (Petari's ARRAY_SIZE is (s32); ours above is unsigned)
#ifndef ARRAY_SIZEU
#define ARRAY_SIZEU(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef ALIGN_PREV
#define ALIGN_PREV(X, N) ((X) & ~((N) - 1))
#define ALIGN_NEXT(X, N) ALIGN_PREV(((X) + (N) - 1), N)
#endif

#ifndef IS_ALIGNED
#define IS_ALIGNED(x, align) (((unsigned long)(x) & ((align) - 1)) == 0)
#define IS_NOT_ALIGNED(X, N) (((X) & ((N) - 1)) != 0)
#endif

// Comparing a reference's address to NULL (needed to match some code)
#ifndef IS_REF_NULL
#define IS_REF_NULL(r) (&(r) == NULL)
#define IS_REF_NONNULL(r) (&(r) != NULL)
#endif

// Petari macros used by the BTE sources (same as ARRAY_SIZEU / a ?: to 0/1)
#ifndef ARRAY_LENGTH
#define ARRAY_LENGTH(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef BOOLIFY_TERNARY
#define BOOLIFY_TERNARY(expr_) ((expr_) ? 1 : 0)
#define BOOLIFY_TERNARY_FALSE(expr_) ((expr_) ? 0 : 1)
#endif

#endif
