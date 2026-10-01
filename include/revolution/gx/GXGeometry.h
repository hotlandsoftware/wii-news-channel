#ifndef GXGEOMETRY_H
#define GXGEOMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>
#include <macros.h>
#include <revolution/gx/GXEnum.h>
#include <revolution/gx/GXStruct.h>

void GXSetVtxDesc(GXAttr, GXAttrType);
void GXClearVtxDesc(void);

void GXSetVtxAttrFmt(GXVtxFmt, GXAttr, GXCompCnt, GXCompType, u8);
void GXSetVtxAttrFmtv(GXVtxFmt, const GXVtxAttrFmtList *);
void GXSetArray(GXAttr, const void *, u8);
void GXBegin(GXPrimitive, GXVtxFmt, u16);

void GXSetTexCoordGen2(GXTexCoordID, GXTexGenType, GXTexGenSrc, u32, GXBool, u32);

// added by Task 15 (lyt)
static inline void GXSetTexCoordGen(GXTexCoordID dst_coord, GXTexGenType func, GXTexGenSrc src_param, u32 mtx) {
    GXSetTexCoordGen2(dst_coord, func, src_param, mtx, GX_FALSE, GX_PTIDENTITY);
}

void GXSetNumTexGens(u8);
void GXInvalidateVtxCache(void);

void GXSetLineWidth(u8, GXTexOffset);
void GXSetPointSize(u8, GXTexOffset);
void GXEnableTexOffsets(GXTexCoordID, GXBool, GXBool);

static inline void GXEnd(void) {

}

void __GXSetSUTexRegs(void);
void __GXSetVCD(void);
void __GXSetVAT(void);
void __GXCalculateVLim(void);
void __GXSetGenMode(void);
void __GXUpdateBPMask(void);
void __GXSetProjection(void);
void __GXSetViewport(void);

#ifdef __cplusplus
}
#endif

#endif // GXGEOMETRY_H
