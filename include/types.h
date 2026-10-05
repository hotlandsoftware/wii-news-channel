#ifndef TYPES_H
#define TYPES_H

#ifdef TARGET_PC
// Native build (see docs/pc_port.md): the host's size_t/NULL/wchar_t, and
// fixed-width types with the same sizes as on the Wii. s32/u32 are `int`
// (CodeWarrior: `long`), which is what the host's size_t and int32_t are on
// 32-bit x86. The 64-bit types are aligned to 8 bytes inside structs as on the
// PowerPC (the i386 ABI would align them to 4).
#include <stddef.h>
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64 __attribute__((aligned(8)));
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64 __attribute__((aligned(8)));
#else
typedef signed char s8;
typedef signed short s16;
typedef signed long s32;
typedef signed long long s64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;

#endif

typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u32 vu32;
typedef volatile u64 vu64;
typedef volatile s8 vs8;
typedef volatile s16 vs16;
typedef volatile s32 vs32;
typedef volatile s64 vs64;

typedef float f32;
#ifdef TARGET_PC
typedef double f64 __attribute__((aligned(8)));
#else
typedef double f64;
#endif
// A pointer to member function is 12 bytes for CodeWarrior and 8 for gcc.
// PC_PMF_PAD(member) after such a member keeps the members behind it at their
// Wii offsets, which the files that declare their own view of a class rely on
// (pc/tools/layout_check.py checks every class of include/news).
#ifdef TARGET_PC
#define PC_PMF_PAD(member) u32 member##PcPad;
#else
#define PC_PMF_PAD(member)
#endif

typedef volatile f32 vf32;
typedef volatile f64 vf64;

typedef int BOOL;

// Placeholder types used by code ported from ogws
typedef int UNKWORD;
typedef void UNKTYPE;
#define TRUE 1
#define FALSE 0

#ifndef TARGET_PC
typedef unsigned long size_t;

#ifndef NULL
#define NULL 0
#endif

#ifndef __cplusplus
typedef unsigned short wchar_t;
#endif
#endif

#endif
