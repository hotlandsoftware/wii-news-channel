#ifndef REVOLUTION_GX_H
#define REVOLUTION_GX_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _GXColor {
    u8 r, g, b, a;
} GXColor;

typedef struct _GXRenderModeObj {
    u32 viTVmode;      // at 0x00
    u16 fbWidth;       // at 0x04
    u16 efbHeight;     // at 0x06
    u16 xfbHeight;     // at 0x08
    u16 viXOrigin;     // at 0x0A
    u16 viYOrigin;     // at 0x0C
    u16 viWidth;       // at 0x0E
    u16 viHeight;      // at 0x10
    u32 xFBmode;       // at 0x14
    u8 field_rendering;       // at 0x18
    u8 aa;                    // at 0x19
    u8 sample_pattern[12][2]; // at 0x1A
    u8 vfilter[7];            // at 0x32
} GXRenderModeObj;

typedef enum _GXTevRegID {
    GX_TEVPREV,
    GX_TEVREG0,
    GX_TEVREG1,
    GX_TEVREG2,
} GXTevRegID;

typedef enum _GXTevStageID {
    GX_TEVSTAGE0,
} GXTevStageID;

typedef enum _GXTevColorArg {
    GX_CC_CPREV,
    GX_CC_APREV,
    GX_CC_C0,
    GX_CC_A0,
    GX_CC_C1,
    GX_CC_A1,
    GX_CC_C2,
    GX_CC_A2,
    GX_CC_TEXC,
    GX_CC_TEXA,
    GX_CC_RASC,
    GX_CC_RASA,
    GX_CC_ONE,
    GX_CC_HALF,
    GX_CC_KONST,
    GX_CC_ZERO,
} GXTevColorArg;

typedef enum _GXCompare {
    GX_NEVER,
    GX_LESS,
    GX_EQUAL,
    GX_LEQUAL,
    GX_GREATER,
    GX_NEQUAL,
    GX_GEQUAL,
    GX_ALWAYS,
} GXCompare;

void GXSetZMode(u8 compareEnable, GXCompare func, u8 updateEnable);
void GXSetTevColor(GXTevRegID id, GXColor color);
void GXSetTevColorIn(GXTevStageID stage, GXTevColorArg a, GXTevColorArg b, GXTevColorArg c,
                     GXTevColorArg d);

#ifdef __cplusplus
}
#endif

#endif
