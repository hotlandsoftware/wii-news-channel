#ifndef REVOLUTION_GX_H
#define REVOLUTION_GX_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _GXColor {
    u8 r, g, b, a;
} GXColor;


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

typedef u8 GXBool;

#define GX_FALSE 0
#define GX_TRUE 1

typedef enum _GXTevAlphaArg {
    GX_CA_APREV,
    GX_CA_A0,
    GX_CA_A1,
    GX_CA_A2,
    GX_CA_TEXA,
    GX_CA_RASA,
    GX_CA_KONST,
    GX_CA_ZERO,
} GXTevAlphaArg;

typedef enum _GXTevOp {
    GX_TEV_ADD,
    GX_TEV_SUB,
} GXTevOp;

typedef enum _GXTevBias {
    GX_TB_ZERO,
    GX_TB_ADDHALF,
    GX_TB_SUBHALF,
} GXTevBias;

typedef enum _GXTevScale {
    GX_CS_SCALE_1,
    GX_CS_SCALE_2,
    GX_CS_SCALE_4,
    GX_CS_DIVIDE_2,
} GXTevScale;

typedef enum _GXTevMode {
    GX_MODULATE,
    GX_DECAL,
    GX_BLEND,
    GX_REPLACE,
    GX_PASSCLR,
} GXTevMode;

typedef enum _GXTevSwapSel {
    GX_TEV_SWAP0,
    GX_TEV_SWAP1,
    GX_TEV_SWAP2,
    GX_TEV_SWAP3,
} GXTevSwapSel;

typedef enum _GXTexCoordID {
    GX_TEXCOORD0,
    GX_TEXCOORD_NULL = 0xFF,
} GXTexCoordID;

typedef enum _GXTexMapID {
    GX_TEXMAP0,
    GX_TEXMAP_NULL = 0xFF,
} GXTexMapID;

typedef enum _GXChannelID {
    GX_COLOR0,
    GX_COLOR1,
    GX_ALPHA0,
    GX_ALPHA1,
    GX_COLOR0A0,
    GX_COLOR1A1,
    GX_COLOR_ZERO,
    GX_ALPHA_BUMP,
    GX_ALPHA_BUMPN,
    GX_COLOR_NULL = 0xFF,
} GXChannelID;

typedef enum _GXColorSrc {
    GX_SRC_REG,
    GX_SRC_VTX,
} GXColorSrc;

typedef enum _GXLightID {
    GX_LIGHT_NULL = 0,
} GXLightID;

typedef enum _GXDiffuseFn {
    GX_DF_NONE,
    GX_DF_SIGN,
    GX_DF_CLAMP,
} GXDiffuseFn;

typedef enum _GXAttnFn {
    GX_AF_SPEC,
    GX_AF_SPOT,
    GX_AF_NONE,
} GXAttnFn;

typedef enum _GXAttr {
    GX_VA_PNMTXIDX,
    GX_VA_TEX0MTXIDX,
    GX_VA_TEX1MTXIDX,
    GX_VA_TEX2MTXIDX,
    GX_VA_TEX3MTXIDX,
    GX_VA_TEX4MTXIDX,
    GX_VA_TEX5MTXIDX,
    GX_VA_TEX6MTXIDX,
    GX_VA_TEX7MTXIDX,
    GX_VA_POS,
    GX_VA_NRM,
    GX_VA_CLR0,
    GX_VA_CLR1,
    GX_VA_TEX0,
} GXAttr;

typedef enum _GXAttrType {
    GX_NONE,
    GX_DIRECT,
    GX_INDEX8,
    GX_INDEX16,
} GXAttrType;

typedef enum _GXVtxFmt {
    GX_VTXFMT0,
    GX_VTXFMT1,
    GX_VTXFMT2,
    GX_VTXFMT3,
    GX_VTXFMT4,
} GXVtxFmt;

typedef enum _GXCompCnt {
    GX_POS_XY = 0,
    GX_POS_XYZ = 1,
    GX_CLR_RGB = 0,
    GX_CLR_RGBA = 1,
    GX_TEX_S = 0,
    GX_TEX_ST = 1,
} GXCompCnt;

typedef enum _GXCompType {
    GX_U8 = 0,
    GX_S8 = 1,
    GX_U16 = 2,
    GX_S16 = 3,
    GX_F32 = 4,
    GX_RGB565 = 0,
    GX_RGB8 = 1,
    GX_RGBX8 = 2,
    GX_RGBA4 = 3,
    GX_RGBA6 = 4,
    GX_RGBA8 = 5,
} GXCompType;

typedef enum _GXPrimitive {
    GX_QUADS = 0x80,
} GXPrimitive;

typedef enum _GXBlendMode {
    GX_BM_NONE,
    GX_BM_BLEND,
    GX_BM_LOGIC,
    GX_BM_SUBTRACT,
} GXBlendMode;

typedef enum _GXBlendFactor {
    GX_BL_ZERO,
    GX_BL_ONE,
    GX_BL_SRCCLR,
    GX_BL_INVSRCCLR,
    GX_BL_SRCALPHA,
    GX_BL_INVSRCALPHA,
    GX_BL_DSTALPHA,
    GX_BL_INVDSTALPHA,
} GXBlendFactor;

typedef enum _GXLogicOp {
    GX_LO_CLEAR,
} GXLogicOp;

typedef enum _GXProjectionType {
    GX_PERSPECTIVE,
    GX_ORTHOGRAPHIC,
} GXProjectionType;

#define GX_PNMTX0 0
#define GX_PNMTX1 3

typedef struct _GXTexObj {
    u32 dummy[8];
} GXTexObj;

typedef enum _VITVMode {
    VI_TVMODE_NTSC_INT = 0,
    VI_TVMODE_NTSC_DS = 1,
    VI_TVMODE_NTSC_PROG = 2,
    VI_TVMODE_PAL_INT = 4,
    VI_TVMODE_PAL_DS = 5,
    VI_TVMODE_EURGB60_INT = 20,
    VI_TVMODE_EURGB60_DS = 21,
    VI_TVMODE_EURGB60_PROG = 22,
    VI_TVMODE_MPAL_INT = 8,
} VITVMode;

typedef struct _GXRenderModeObj {
    VITVMode viTVmode;        // at 0x00
    u16 fbWidth;              // at 0x04
    u16 efbHeight;            // at 0x06
    u16 xfbHeight;            // at 0x08
    u16 viXOrigin;            // at 0x0A
    u16 viYOrigin;            // at 0x0C
    u16 viWidth;              // at 0x0E
    u16 viHeight;             // at 0x10
    u32 xFBmode;              // at 0x14
    u8 field_rendering;       // at 0x18
    u8 aa;                    // at 0x19
    u8 sample_pattern[12][2]; // at 0x1A
    u8 vfilter[7];            // at 0x32
} GXRenderModeObj;

void GXClearVtxDesc(void);
void GXSetVtxDesc(GXAttr attr, GXAttrType type);
void GXSetVtxAttrFmt(GXVtxFmt fmt, GXAttr attr, GXCompCnt cnt, GXCompType type, u8 frac);
void GXSetNumTexGens(u8 nTexGens);
void GXBegin(GXPrimitive type, GXVtxFmt fmt, u16 nVerts);
void GXDrawDone(void);
void GXSetDispCopySrc(u16 left, u16 top, u16 wd, u16 ht);
void GXSetDispCopyDst(u16 wd, u16 ht);
f32 GXGetYScaleFactor(u16 efbHeight, u16 xfbHeight);
u32 GXSetDispCopyYScale(f32 vscale);
void GXCopyDisp(void* dest, GXBool clear);
void GXSetNumChans(u8 nChans);
void GXSetChanCtrl(GXChannelID chan, GXBool enable, GXColorSrc amb_src, GXColorSrc mat_src,
                   u32 light_mask, GXDiffuseFn diff_fn, GXAttnFn attn_fn);
void GXLoadTexObj(const GXTexObj* obj, GXTexMapID id);
void GXSetNumIndStages(u8 nIndStages);
void GXSetTevDirect(GXTevStageID tev_stage);
void GXSetTevOp(GXTevStageID id, GXTevMode mode);
void GXSetTevAlphaIn(GXTevStageID stage, GXTevAlphaArg a, GXTevAlphaArg b, GXTevAlphaArg c,
                     GXTevAlphaArg d);
void GXSetTevColorOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale,
                     GXBool clamp, GXTevRegID out_reg);
void GXSetTevAlphaOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale,
                     GXBool clamp, GXTevRegID out_reg);
void GXSetTevSwapMode(GXTevStageID stage, GXTevSwapSel ras_sel, GXTevSwapSel tex_sel);
void GXSetTevOrder(GXTevStageID stage, GXTexCoordID coord, GXTexMapID map, GXChannelID color);
void GXSetNumTevStages(u8 nStages);
void GXSetBlendMode(GXBlendMode type, GXBlendFactor src_factor, GXBlendFactor dst_factor,
                    GXLogicOp op);
void GXSetProjection(const f32 mtx[4][4], GXProjectionType type);
void GXLoadPosMtxImm(const f32 mtx[3][4], u32 id);
void GXSetCurrentMtx(u32 id);

typedef union _PPCWGPipe {
    u8 u8;
    s8 s8;
    u16 u16;
    s16 s16;
    u32 u32;
    s32 s32;
    f32 f32;
} PPCWGPipe;

#define GXWGFifo (*(volatile PPCWGPipe*)0xCC008000)

static inline void GXPosition3f32(f32 x, f32 y, f32 z) {
    GXWGFifo.f32 = x;
    GXWGFifo.f32 = y;
    GXWGFifo.f32 = z;
}

static inline void GXPosition2f32(f32 x, f32 y) {
    GXWGFifo.f32 = x;
    GXWGFifo.f32 = y;
}

static inline void GXColor1u32(u32 clr) {
    GXWGFifo.u32 = clr;
}

static inline void GXTexCoord2f32(f32 s, f32 t) {
    GXWGFifo.f32 = s;
    GXWGFifo.f32 = t;
}

static inline void GXEnd(void) {}

#ifdef __cplusplus
}
#endif

#endif
