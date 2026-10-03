#ifndef GXVERT_H
#define GXVERT_H

#include <revolution/base/PPCWGPipe.h>

#ifdef __cplusplus
extern "C" {
#endif

// CONFLICT (Petari): Petari declares `volatile PPCWGPipe GXWGFifo : 0xCC008000;`
// and generates plain `static` (not inline) vertex functions. The News Channel
// game code is built with -inline noauto and was matched against
// `static inline` functions that write through a pointer cast, so that form
// is kept here (it also matches ogws's GXVert.h).
//
// GXVERT_FIFO_VARIABLE (opt-in, define before including this header) selects the
// absolute-address variable instead. The compiler then knows that a FIFO write cannot
// alias a local variable (GXDraw.c needs that).
#ifdef GXVERT_FIFO_VARIABLE
volatile PPCWGPipe GXWGFifo : 0xCC008000;
#else
#define GXWGFifo (*(volatile PPCWGPipe*)0xCC008000)
#endif

#define __GXCDEF(prfx, n, t) __GXCDEF##n(prfx##n##t, t, t)
#define __GXCDEFX(func, n, t) __GXCDEF##n(func, t, t)

#define __GXCDEF1(func, ts, td)                                                \
    static inline void func(ts x) {                                            \
        GXWGFifo.td = (td)x;                                                   \
    }

#define __GXCDEF2(func, ts, td)                                                \
    static inline void func(ts x, ts y) {                                      \
        GXWGFifo.td = (td)x;                                                   \
        GXWGFifo.td = (td)y;                                                   \
    }

#define __GXCDEF3(func, ts, td)                                                \
    static inline void func(ts x, ts y, ts z) {                                \
        GXWGFifo.td = (td)x;                                                   \
        GXWGFifo.td = (td)y;                                                   \
        GXWGFifo.td = (td)z;                                                   \
    }

#define __GXCDEF4(func, ts, td)                                                \
    static inline void func(ts x, ts y, ts z, ts w) {                          \
        GXWGFifo.td = (td)x;                                                   \
        GXWGFifo.td = (td)y;                                                   \
        GXWGFifo.td = (td)z;                                                   \
        GXWGFifo.td = (td)w;                                                   \
    }

__GXCDEF(GXCmd, 1, u8)
__GXCDEF(GXCmd, 1, u16)
__GXCDEF(GXCmd, 1, u32)
__GXCDEF(GXCmd, 1, f32)

__GXCDEF(GXParam, 1, u8)
__GXCDEF(GXParam, 1, u16)
__GXCDEF(GXParam, 1, u32)
__GXCDEF(GXParam, 1, s8)
__GXCDEF(GXParam, 1, s16)
__GXCDEF(GXParam, 1, s32)
__GXCDEF(GXParam, 1, f32)

__GXCDEF(GXPosition, 3, f32)
__GXCDEF(GXPosition, 3, u8)
__GXCDEF(GXPosition, 3, s8)
__GXCDEF(GXPosition, 3, u16)
__GXCDEF(GXPosition, 3, s16)

__GXCDEF(GXPosition, 2, f32)
__GXCDEF(GXPosition, 2, u8)
__GXCDEF(GXPosition, 2, s8)
__GXCDEF(GXPosition, 2, u16)
__GXCDEF(GXPosition, 2, s16)

__GXCDEFX(GXPosition1x16, 1, u16)
__GXCDEFX(GXPosition1x8, 1, u8)

__GXCDEF(GXNormal, 3, f32)
__GXCDEF(GXNormal, 3, s16)
__GXCDEF(GXNormal, 3, s8)
__GXCDEFX(GXNormal1x16, 1, u16)
__GXCDEFX(GXNormal1x8, 1, u8)

__GXCDEF(GXColor, 4, u8)
__GXCDEF(GXColor, 1, u32)
__GXCDEF(GXColor, 3, u8)
__GXCDEFX(GXColor1x16, 1, u16)
__GXCDEFX(GXColor1x8, 1, u8)

__GXCDEF(GXTexCoord, 2, f32)
__GXCDEF(GXTexCoord, 2, s16)
__GXCDEF(GXTexCoord, 2, u16)
__GXCDEF(GXTexCoord, 2, s8)
__GXCDEF(GXTexCoord, 2, u8)
__GXCDEF(GXTexCoord, 1, f32)
__GXCDEF(GXTexCoord, 1, s16)
__GXCDEF(GXTexCoord, 1, u16)
__GXCDEF(GXTexCoord, 1, s8)
__GXCDEF(GXTexCoord, 1, u8)
__GXCDEFX(GXTexCoord1x16, 1, u16)
__GXCDEFX(GXTexCoord1x8, 1, u8)

__GXCDEF(GXMatrixIndex, 1, u8)

#ifdef __cplusplus
}
#endif

#endif  // GXVERT_H
