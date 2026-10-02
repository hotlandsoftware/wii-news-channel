#ifndef GXGET_H
#define GXGET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>
#include <macros.h>
#include <revolution/gx.h>

void GXGetViewportv(f32 *);

u32 GXGetTexObjTlut(const GXTexObj *);

void GXGetTexObjAll(const GXTexObj *, void **, u16 *, u16 *, GXTexFmt *, GXTexWrapMode *, GXTexWrapMode *, GXBool *);

void GXGetTexObjLODAll(const GXTexObj *, GXTexFilter *, GXTexFilter *, f32 *, f32 *, f32 *, GXBool *, GXBool *, GXAnisotropy *);

GXTexFmt GXGetTexObjFmt(const GXTexObj *);
GXBool GXGetTexObjMipMap(const GXTexObj *); 

// Added for nw4r::ef (Task 9) (GXAttr.c)
void GXGetVtxDesc(GXAttr attr, GXAttrType* type);
void GXGetVtxDescv(GXVtxDescList* vcd);
void GXGetVtxAttrFmt(GXVtxFmt fmt, GXAttr attr, GXCompCnt* cnt, GXCompType* type, u8* frac);
void GXGetVtxAttrFmtv(GXVtxFmt fmt, GXVtxAttrFmtList* vat);

#ifdef __cplusplus
}
#endif

#endif // GXGET_H
