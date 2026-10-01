#ifndef GXPIXEL_H
#define GXPIXEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>
#include <macros.h>
#include <revolution/gx/GXEnum.h>
#include <revolution/gx/GXStruct.h>

void GXSetFog(GXFogType, f32, f32, f32, f32, GXColor);
void GXSetFogRangeAdj(GXBool, u16, const GXFogAdjTable *);
void GXSetBlendMode(GXBlendMode, GXBlendFactor, GXBlendFactor, GXLogicOp);
void GXSetColorUpdate(GXBool);
void GXSetAlphaUpdate(GXBool);
void GXSetZMode(GXBool, GXCompare, GXBool);
void GXSetZCompLoc(GXBool);
void GXSetPixelFmt(GXPixelFmt, GXZFmt16);
void GXSetDither(GXBool);
void GXSetDstAlpha(GXBool, u8);

void GXSetFieldMask(GXBool, GXBool);
void GXSetFieldMode(GXBool, GXBool);

/* Added for g3d (Task 10) */
void GXInitFogAdjTable(GXFogAdjTable* table, u16 width, const f32 projmtx[4][4]);

#ifdef __cplusplus
}
#endif

#endif // GXPIXEL_H
