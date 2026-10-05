// The SDK's GX API on top of the register state (see gx_internal.h).
//
// Each function composes the register values the SDK's own function
// composes (src/revolution/GX is the reference) and loads them. The SDK
// collects part of the state and sends it at the next GXBegin(); here
// everything is loaded at once, which gives the same result because
// nothing is drawn in between. The one thing that does wait for GXBegin()
// is the texture coordinate scale, which depends on the TEV orders and the
// loaded textures together (__GXSetSUTexRegs).
//
// Object functions (GXInitTexObj*, GXInitLight*) are in gx_objects.cpp.

#include "gx_internal.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "pc_noop.h"
#include "pc_video.h"
#include "texdecode.h"

namespace {

// For the getters, and for what a register does not keep.
struct AttrFmt {
    u8 cnt;
    u8 type;
    u8 frac;
};
u8 sVtxDesc[GX_VA_MAX_ATTR];
AttrFmt sVtxAttrFmt[GX_MAX_VTXFMT][GX_VA_MAX_ATTR];
u32 sTexMapId[GX_MAX_TEVSTAGE]; // as given to GXSetTevOrder(), with GX_TEX_DISABLE
u32 sTevCoordEnable;            // stages whose GXSetTevOrder() named a coordinate
u32 sManualCoordScale;          // coordinates under GXSetTexCoordScaleManually()

GXFifoObj sFifo;
GXDrawSyncCallback sDrawSyncCallback;
GXDrawDoneCallback sDrawDoneCallback;

u32 FloatBits(f32 value) {
    u32 bits;
    std::memcpy(&bits, &value, 4);
    return bits;
}

u32 SetField(u32 reg, u32 shift, u32 width, u32 value) {
    u32 mask = ((1u << width) - 1) << shift;
    return (reg & ~mask) | ((value << shift) & mask);
}

// Changes a field of a BP register.
void BPField(u32 reg, u32 shift, u32 width, u32 value) {
    PCGXWriteBP(reg, SetField(gPCGX.bp[reg], shift, width, value));
}

// Number of XFB lines a display copy of `efbHeight` lines produces with the
// 1.8 fixed-point scale `iScale` (the SDK's __GXGetNumXfbLines).
u32 NumXfbLines(u32 efbHeight, u32 iScale) {
    u32 count = (efbHeight - 1) * 256;
    u32 realHeight = count / iScale + 1;

    u32 iScaleD = iScale;
    if (iScaleD > 0x80 && iScaleD < 0x100) {
        while ((iScaleD & 1) == 0) {
            iScaleD >>= 1;
        }
        if (efbHeight % iScaleD == 0) {
            realHeight++;
        }
    }
    if (realHeight > 1024) {
        realHeight = 1024;
    }
    return realHeight;
}

u32 ArrayIndex(GXAttr attr) {
    if (attr == GX_VA_NBT) {
        attr = GX_VA_NRM;
    }
    return static_cast<u32>(attr) - GX_VA_POS; // POS..TEX7: 0-11, the matrix and light arrays: 12-15
}

void SendVtxDesc() {
    u32 lo = 0, hi = 0;
    for (u32 i = 0; i < 9; i++) {
        lo |= (sVtxDesc[GX_VA_PNMTXIDX + i] ? 1u : 0u) << i;
    }
    lo |= static_cast<u32>(sVtxDesc[GX_VA_POS]) << 9;
    lo |= static_cast<u32>(sVtxDesc[GX_VA_NRM]) << 11;
    lo |= static_cast<u32>(sVtxDesc[GX_VA_CLR0]) << 13;
    lo |= static_cast<u32>(sVtxDesc[GX_VA_CLR1]) << 15;
    for (u32 i = 0; i < 8; i++) {
        hi |= static_cast<u32>(sVtxDesc[GX_VA_TEX0 + i]) << (i * 2);
    }
    PCGXWriteCP(PC_CP_VCD_LO, lo);
    PCGXWriteCP(PC_CP_VCD_HI, hi);

    // What the transform unit is told to expect (colours, normals, coordinates).
    u32 colors = (sVtxDesc[GX_VA_CLR0] ? 1 : 0) + (sVtxDesc[GX_VA_CLR1] ? 1 : 0);
    u32 normals = sVtxDesc[GX_VA_NRM] ? (sVtxAttrFmt[0][GX_VA_NRM].cnt != GX_NRM_XYZ ? 2 : 1) : 0;
    u32 coords = 0;
    for (u32 i = 0; i < 8; i++) {
        coords += sVtxDesc[GX_VA_TEX0 + i] ? 1 : 0;
    }
    PCGXWriteXF1(PC_XF_INVTXSPEC, colors | (normals << 2) | (coords << 4));
}

void SendVtxAttrFmt(u32 fmt) {
    const AttrFmt* f = sVtxAttrFmt[fmt];
    u32 a = 0, b = 0, c = 0;
    a |= (f[GX_VA_POS].cnt & 1u) | ((f[GX_VA_POS].type & 7u) << 1) | ((f[GX_VA_POS].frac & 31u) << 4);
    u32 nrmCnt = f[GX_VA_NRM].cnt;
    a |= ((nrmCnt != GX_NRM_XYZ ? 1u : 0u) << 9) | ((f[GX_VA_NRM].type & 7u) << 10);
    a |= ((f[GX_VA_CLR0].cnt & 1u) << 13) | ((f[GX_VA_CLR0].type & 7u) << 14);
    a |= ((f[GX_VA_CLR1].cnt & 1u) << 17) | ((f[GX_VA_CLR1].type & 7u) << 18);
    a |= ((f[GX_VA_TEX0].cnt & 1u) << 21) | ((f[GX_VA_TEX0].type & 7u) << 22) | ((f[GX_VA_TEX0].frac & 31u) << 25);
    a |= 1u << 30; // byte dequantisation: fractions apply to bytes too
    a |= (nrmCnt == GX_NRM_NBT3 ? 1u : 0u) << 31;

    for (u32 i = 1; i <= 3; i++) {
        const AttrFmt& t = f[GX_VA_TEX0 + i];
        b |= ((t.cnt & 1u) | ((t.type & 7u) << 1) | ((t.frac & 31u) << 4)) << ((i - 1) * 9);
    }
    b |= ((f[GX_VA_TEX4].cnt & 1u) << 27) | ((f[GX_VA_TEX4].type & 7u) << 28);
    b |= 1u << 31; // vertex cache enhancement, always set by the SDK

    c |= f[GX_VA_TEX4].frac & 31u;
    for (u32 i = 5; i <= 7; i++) {
        const AttrFmt& t = f[GX_VA_TEX0 + i];
        c |= ((t.cnt & 1u) | ((t.type & 7u) << 1) | ((t.frac & 31u) << 4)) << (5 + (i - 5) * 9);
    }
    PCGXWriteCP(PC_CP_VAT_A + fmt, a);
    PCGXWriteCP(PC_CP_VAT_B + fmt, b);
    PCGXWriteCP(PC_CP_VAT_C + fmt, c);
}

void SendMatrixIndices(u32 a, u32 b) {
    PCGXWriteCP(PC_CP_MATINDEX_A, a);
    PCGXWriteCP(PC_CP_MATINDEX_B, b);
    u32 words[2] = {a, b};
    PCGXWriteXF(PC_XF_MATINDEX_A, 2, words);
}

void SendViewport() {
    const PCGXState& s = gPCGX;
    f32 left = s.viewport[0], top = s.viewport[1], width = s.viewport[2], height = s.viewport[3];
    f32 zmin = s.viewport[4] * s.zScale;
    f32 zmax = s.viewport[5] * s.zScale;
    u32 words[6] = {
        FloatBits(width / 2.0f),
        FloatBits(-height / 2.0f),
        FloatBits(zmax - zmin),
        FloatBits(left + width / 2.0f + 342.0f),
        FloatBits(top + height / 2.0f + 342.0f),
        FloatBits(zmax + s.zOffset),
    };
    PCGXWriteXF(PC_XF_VIEWPORT, 6, words);
}

// The size a texture coordinate is scaled to is the size of the texture it
// is used with (__GXSetSUTexRegs): indirect stages first, then TEV stages.
void SendCoordScales() {
    PCGXState& s = gPCGX;
    if (sManualCoordScale == 0xFF) {
        return;
    }
    auto set = [&s](u32 map, u32 coord) {
        const PCGXTexUnit& t = s.tex[map & 7];
        if (t.width == 0 || t.height == 0) {
            return;
        }
        u32 sreg = SetField(s.bp[PC_BP_SU_SSIZE0 + coord * 2], 0, 16, t.width - 1u);
        u32 treg = SetField(s.bp[PC_BP_SU_SSIZE0 + coord * 2 + 1], 0, 16, t.height - 1u);
        if (sreg != s.bp[PC_BP_SU_SSIZE0 + coord * 2]) {
            PCGXWriteBP(PC_BP_SU_SSIZE0 + coord * 2, sreg);
        }
        if (treg != s.bp[PC_BP_SU_SSIZE0 + coord * 2 + 1]) {
            PCGXWriteBP(PC_BP_SU_SSIZE0 + coord * 2 + 1, treg);
        }
    };
    u32 iref = s.bp[PC_BP_RAS1_IREF];
    for (u32 i = 0; i < PCGXNumIndStages(); i++) {
        u32 map = (iref >> (i * 6)) & 7;
        u32 coord = (iref >> (i * 6 + 3)) & 7;
        if (!(sManualCoordScale & (1u << coord))) {
            set(map, coord);
        }
    }
    for (u32 i = 0; i < PCGXNumTevStages(); i++) {
        PCGXTevOrder order = PCGXGetTevOrder(i);
        u32 map = sTexMapId[i] & ~static_cast<u32>(GX_TEX_DISABLE);
        if (map != GX_TEXMAP_NULL && !(sManualCoordScale & (1u << order.texCoord)) && (sTevCoordEnable & (1u << i))) {
            set(map, order.texCoord);
        }
    }
}

void LoadIdentity(u32 address, u32 rows, u32 columns) {
    u32 words[12];
    for (u32 r = 0; r < rows; r++) {
        for (u32 c = 0; c < columns; c++) {
            words[r * columns + c] = FloatBits(r == c ? 1.0f : 0.0f);
        }
    }
    PCGXWriteXF(address, rows * columns, words);
}

} // namespace

extern "C" {

// The SDK's default render modes (src/revolution/GX/GXFrameBuf.c); g3d picks
// one by TV format.
GXRenderModeObj GXNtsc480IntDf = {
    VI_TVMODE_NTSC_INT, 640, 480, 480, 40, 0, 640, 480, VI_XFBMODE_DF, 0, 0,
    {{6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}},
    {8, 8, 10, 12, 10, 8, 8}};

GXRenderModeObj GXMpal480IntDf = {
    VI_TVMODE_MPAL_INT, 640, 480, 480, 40, 0, 640, 480, VI_XFBMODE_DF, 0, 0,
    {{6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}},
    {8, 8, 10, 12, 10, 8, 8}};

GXRenderModeObj GXPal528IntDf = {
    VI_TVMODE_PAL_INT, 640, 528, 528, 40, 23, 640, 528, VI_XFBMODE_DF, 0, 0,
    {{6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}},
    {8, 8, 10, 12, 10, 8, 8}};

GXRenderModeObj GXEurgb60Hz480IntDf = {
    VI_TVMODE_EURGB60_INT, 640, 480, 480, 40, 0, 640, 480, VI_XFBMODE_DF, 0, 0,
    {{6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}},
    {8, 8, 10, 12, 10, 8, 8}};

// --- Initialisation --------------------------------------------------------------

// Returns the FIFO object that describes the application's buffer. Nothing
// reads commands from that memory: FIFO writes are decoded as they arrive.
// The state is the SDK's default state (__GXInitGX).
GXFifoObj* GXInit(void* base, u32 size) {
    __GXFifoObj* fifo = reinterpret_cast<__GXFifoObj*>(&sFifo);
    std::memset(&sFifo, 0, sizeof(sFifo));
    fifo->base = static_cast<u8*>(base);
    fifo->top = static_cast<u8*>(base) + size - 4;
    fifo->size = size;
    fifo->hiWatermark = size - 0x4000;
    fifo->loWatermark = (size >> 1) & ~0x1Fu;
    fifo->rdPtr = base;
    fifo->wrPtr = base;
    fifo->count = 0;
    fifo->bind_cpu = GX_TRUE;
    fifo->bind_gp = GX_TRUE;

    PCGXResetState();
    std::memset(sVtxDesc, 0, sizeof(sVtxDesc));
    std::memset(sVtxAttrFmt, 0, sizeof(sVtxAttrFmt));
    std::memset(sTexMapId, 0, sizeof(sTexMapId));
    sTevCoordEnable = 0;
    sManualCoordScale = 0;

    const GXColor black = {0, 0, 0, 0};
    const GXColor white = {255, 255, 255, 255};
    const GXColor clear = {64, 64, 64, 255};

    const GXRenderModeObj* mode = PCVIGetRenderMode();
    u16 fbWidth = mode != nullptr ? mode->fbWidth : 640;
    u16 efbHeight = mode != nullptr ? mode->efbHeight : 480;

    GXSetCopyClear(clear, 0xFFFFFF);
    for (u32 i = 0; i < 8; i++) {
        GXSetTexCoordGen2(static_cast<GXTexCoordID>(i), GX_TG_MTX2x4, static_cast<GXTexGenSrc>(GX_TG_TEX0 + i), GX_IDENTITY,
                          GX_FALSE, GX_PTIDENTITY);
    }
    GXSetNumTexGens(1);
    GXClearVtxDesc();
    GXInvalidateVtxCache();
    for (u32 i = 0; i < GX_MAX_VTXFMT; i++) {
        SendVtxAttrFmt(i);
    }
    GXSetLineWidth(6, GX_TO_ZERO);
    GXSetPointSize(6, GX_TO_ZERO);
    for (u32 i = 0; i < 8; i++) {
        GXEnableTexOffsets(static_cast<GXTexCoordID>(i), GX_FALSE, GX_FALSE);
    }

    // Matrices: identity everywhere the SDK puts one.
    LoadIdentity(PC_XF_POSMTX + GX_PNMTX0 * 4, 3, 4);
    LoadIdentity(PC_XF_NRMMTX + GX_PNMTX0 * 3, 3, 3);
    LoadIdentity(PC_XF_POSMTX + GX_IDENTITY * 4, 3, 4);
    LoadIdentity(PC_XF_POSTMTX + (GX_PTIDENTITY - GX_PTTEXMTX0) * 4, 3, 4);
    PCGXWriteXF1(PC_XF_DUALTEX, 1);
    GXSetCurrentMtx(GX_PNMTX0);

    GXSetViewport(0.0f, 0.0f, fbWidth, efbHeight, 0.0f, 1.0f);
    const f32 identity[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};
    GXSetProjection(identity, GX_ORTHOGRAPHIC);
    GXSetCoPlanar(GX_FALSE);
    GXSetCullMode(GX_CULL_BACK);
    GXSetClipMode(GX_CLIP_ENABLE);
    GXSetScissor(0, 0, fbWidth, efbHeight);
    GXSetScissorBoxOffset(0, 0);

    GXSetNumChans(0);
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXSetChanAmbColor(GX_COLOR0A0, black);
    GXSetChanMatColor(GX_COLOR0A0, white);
    GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXSetChanAmbColor(GX_COLOR1A1, black);
    GXSetChanMatColor(GX_COLOR1A1, white);

    GXInvalidateTexAll();
    for (u32 i = 0; i < 8; i++) {
        GXSetTevOrder(static_cast<GXTevStageID>(i), static_cast<GXTexCoordID>(i), static_cast<GXTexMapID>(i), GX_COLOR0A0);
    }
    for (u32 i = 8; i < GX_MAX_TEVSTAGE; i++) {
        GXSetTevOrder(static_cast<GXTevStageID>(i), GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    }
    GXSetNumTevStages(1);
    GXSetTevOp(GX_TEVSTAGE0, GX_REPLACE);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
    GXSetZTexture(GX_ZT_DISABLE, GX_TF_Z8, 0);
    for (u32 i = 0; i < GX_MAX_TEVSTAGE; i++) {
        GXTevStageID stage = static_cast<GXTevStageID>(i);
        GXSetTevKColorSel(stage, GX_TEV_KCSEL_1_4);
        GXSetTevKAlphaSel(stage, GX_TEV_KASEL_1);
        GXSetTevSwapMode(stage, GX_TEV_SWAP0, GX_TEV_SWAP0);
        GXSetTevDirect(stage);
    }
    GXSetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA);
    GXSetTevSwapModeTable(GX_TEV_SWAP1, GX_CH_RED, GX_CH_RED, GX_CH_RED, GX_CH_ALPHA);
    GXSetTevSwapModeTable(GX_TEV_SWAP2, GX_CH_GREEN, GX_CH_GREEN, GX_CH_GREEN, GX_CH_ALPHA);
    GXSetTevSwapModeTable(GX_TEV_SWAP3, GX_CH_BLUE, GX_CH_BLUE, GX_CH_BLUE, GX_CH_ALPHA);
    GXSetNumIndStages(0);
    for (u32 i = 0; i < GX_MAX_INDTEXSTAGE; i++) {
        GXSetIndTexCoordScale(static_cast<GXIndTexStageID>(i), GX_ITS_1, GX_ITS_1);
    }

    GXSetFog(GX_FOG_NONE, 0.0f, 1.0f, 0.1f, 1.0f, black);
    GXSetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
    GXSetColorUpdate(GX_TRUE);
    GXSetAlphaUpdate(GX_TRUE);
    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GXSetZCompLoc(GX_TRUE);
    GXSetDither(GX_TRUE);
    GXSetDstAlpha(GX_FALSE, 0);
    GXSetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);

    GXSetDispCopySrc(0, 0, fbWidth, efbHeight);
    GXSetDispCopyDst(fbWidth, efbHeight);
    GXSetDispCopyYScale(1.0f);
    GXSetTexCopySrc(0, 0, fbWidth, efbHeight);
    GXSetTexCopyDst(fbWidth, efbHeight, GX_TF_RGB565, GX_FALSE);
    return &sFifo;
}

// --- Vertex format -----------------------------------------------------------------

void GXSetVtxDesc(GXAttr attr, GXAttrType type) {
    if (attr == GX_VA_NBT) {
        attr = GX_VA_NRM;
    }
    if (static_cast<u32>(attr) < GX_VA_MAX_ATTR) {
        sVtxDesc[attr] = static_cast<u8>(type);
        SendVtxDesc();
    }
}

void GXSetVtxDescv(const GXVtxDescList* list) {
    for (; list->attr != GX_VA_NULL; list++) {
        GXAttr attr = list->attr == GX_VA_NBT ? GX_VA_NRM : list->attr;
        if (static_cast<u32>(attr) < GX_VA_MAX_ATTR) {
            sVtxDesc[attr] = static_cast<u8>(list->type);
        }
    }
    SendVtxDesc();
}

void GXClearVtxDesc(void) {
    std::memset(sVtxDesc, 0, sizeof(sVtxDesc));
    sVtxDesc[GX_VA_POS] = GX_DIRECT; // the hardware always has a position
    SendVtxDesc();
}

void GXGetVtxDesc(GXAttr attr, GXAttrType* type) {
    if (attr == GX_VA_NBT) {
        attr = GX_VA_NRM;
    }
    *type = static_cast<u32>(attr) < GX_VA_MAX_ATTR ? static_cast<GXAttrType>(sVtxDesc[attr]) : GX_NONE;
}

void GXGetVtxDescv(GXVtxDescList* list) {
    u32 n = 0;
    for (u32 attr = GX_VA_PNMTXIDX; attr <= GX_VA_TEX7; attr++) {
        list[n].attr = static_cast<GXAttr>(attr);
        list[n].type = static_cast<GXAttrType>(sVtxDesc[attr]);
        n++;
    }
    list[n].attr = GX_VA_NBT;
    list[n].type = static_cast<GXAttrType>(sVtxDesc[GX_VA_NRM]);
}

void GXSetVtxAttrFmt(GXVtxFmt fmt, GXAttr attr, GXCompCnt cnt, GXCompType type, u8 frac) {
    if (attr == GX_VA_NBT) {
        attr = GX_VA_NRM;
    }
    if (static_cast<u32>(fmt) < GX_MAX_VTXFMT && static_cast<u32>(attr) < GX_VA_MAX_ATTR) {
        sVtxAttrFmt[fmt][attr].cnt = static_cast<u8>(cnt);
        sVtxAttrFmt[fmt][attr].type = static_cast<u8>(type);
        sVtxAttrFmt[fmt][attr].frac = frac;
        SendVtxAttrFmt(fmt);
    }
}

void GXSetVtxAttrFmtv(GXVtxFmt fmt, const GXVtxAttrFmtList* list) {
    if (static_cast<u32>(fmt) >= GX_MAX_VTXFMT) {
        return;
    }
    for (; list->attr != GX_VA_NULL; list++) {
        GXAttr attr = list->attr == GX_VA_NBT ? GX_VA_NRM : list->attr;
        if (static_cast<u32>(attr) < GX_VA_MAX_ATTR) {
            sVtxAttrFmt[fmt][attr].cnt = static_cast<u8>(list->cnt);
            sVtxAttrFmt[fmt][attr].type = static_cast<u8>(list->type);
            sVtxAttrFmt[fmt][attr].frac = list->frac;
        }
    }
    SendVtxAttrFmt(fmt);
}

void GXGetVtxAttrFmt(GXVtxFmt fmt, GXAttr attr, GXCompCnt* cnt, GXCompType* type, u8* frac) {
    if (attr == GX_VA_NBT) {
        attr = GX_VA_NRM;
    }
    if (static_cast<u32>(fmt) < GX_MAX_VTXFMT && static_cast<u32>(attr) < GX_VA_MAX_ATTR) {
        *cnt = static_cast<GXCompCnt>(sVtxAttrFmt[fmt][attr].cnt);
        *type = static_cast<GXCompType>(sVtxAttrFmt[fmt][attr].type);
        *frac = sVtxAttrFmt[fmt][attr].frac;
    } else {
        *cnt = static_cast<GXCompCnt>(0);
        *type = static_cast<GXCompType>(0);
        *frac = 0;
    }
}

void GXGetVtxAttrFmtv(GXVtxFmt fmt, GXVtxAttrFmtList* list) {
    u32 n = 0;
    for (u32 attr = GX_VA_POS; attr <= GX_VA_TEX7; attr++) {
        list[n].attr = static_cast<GXAttr>(attr);
        GXGetVtxAttrFmt(fmt, static_cast<GXAttr>(attr), &list[n].cnt, &list[n].type, &list[n].frac);
        n++;
    }
    list[n].attr = GX_VA_NULL;
}

// The array is read in host byte order unless PCGXSetArrayBigEndian() says
// otherwise afterwards.
void GXSetArray(GXAttr attr, const void* base, u8 stride) {
    u32 index = ArrayIndex(attr);
    if (index < PC_GX_NUM_ARRAYS) {
        gPCGX.arrays[index].base = static_cast<const u8*>(base);
        gPCGX.arrays[index].stride = stride;
        gPCGX.arrays[index].bigEndian = false;
    }
}

void GXInvalidateVtxCache(void) {}

void GXBegin(GXPrimitive type, GXVtxFmt vtxfmt, u16 nverts) {
    SendCoordScales();
    u8 header[3] = {static_cast<u8>(static_cast<u32>(type) | static_cast<u32>(vtxfmt)), static_cast<u8>(nverts >> 8),
                    static_cast<u8>(nverts)};
    PCGXFifoWrite(header, 3);
}

void GXSetLineWidth(u8 width, GXTexOffset texOffsets) {
    u32 reg = SetField(gPCGX.bp[PC_BP_LPSIZE], 0, 8, width);
    PCGXWriteBP(PC_BP_LPSIZE, SetField(reg, 16, 3, texOffsets));
}

void GXSetPointSize(u8 size, GXTexOffset texOffsets) {
    u32 reg = SetField(gPCGX.bp[PC_BP_LPSIZE], 8, 8, size);
    PCGXWriteBP(PC_BP_LPSIZE, SetField(reg, 19, 3, texOffsets));
}

void GXEnableTexOffsets(GXTexCoordID coord, GXBool line, GXBool point) {
    u32 reg = SetField(gPCGX.bp[PC_BP_SU_SSIZE0 + coord * 2], 18, 1, line);
    PCGXWriteBP(PC_BP_SU_SSIZE0 + coord * 2, SetField(reg, 19, 1, point));
}

void GXSetCullMode(GXCullMode mode) {
    // The hardware's numbers for front and back are the other way round.
    static const u8 kHw[4] = {0, 2, 1, 3};
    BPField(PC_BP_GENMODE, 14, 2, kHw[mode & 3]);
}

void GXSetCoPlanar(GXBool enable) {
    BPField(PC_BP_GENMODE, 19, 1, enable);
}

void GXSetClipMode(GXClipMode mode) {
    PCGXWriteXF1(PC_XF_CLIPDISABLE, mode);
}

// --- Transform ---------------------------------------------------------------------

void GXSetProjection(const f32 mtx[4][4], GXProjectionType type) {
    PCGXState& s = gPCGX;
    s.projection[0] = static_cast<f32>(type);
    s.projection[1] = mtx[0][0];
    s.projection[3] = mtx[1][1];
    s.projection[5] = mtx[2][2];
    s.projection[6] = mtx[2][3];
    if (type == GX_ORTHOGRAPHIC) {
        s.projection[2] = mtx[0][3];
        s.projection[4] = mtx[1][3];
    } else {
        s.projection[2] = mtx[0][2];
        s.projection[4] = mtx[1][2];
    }
    u32 words[7];
    for (u32 i = 0; i < 6; i++) {
        words[i] = FloatBits(s.projection[i + 1]);
    }
    words[6] = type;
    PCGXWriteXF(PC_XF_PROJECTION, 7, words);
}

void GXSetProjectionv(const f32* ptr) {
    PCGXState& s = gPCGX;
    std::memcpy(s.projection, ptr, sizeof(s.projection));
    u32 words[7];
    for (u32 i = 0; i < 6; i++) {
        words[i] = FloatBits(ptr[i + 1]);
    }
    words[6] = ptr[0] == 0.0f ? GX_PERSPECTIVE : GX_ORTHOGRAPHIC;
    PCGXWriteXF(PC_XF_PROJECTION, 7, words);
}

void GXGetProjectionv(f32* ptr) {
    std::memcpy(ptr, gPCGX.projection, sizeof(gPCGX.projection));
}

void GXLoadPosMtxImm(const f32 mtx[3][4], u32 id) {
    u32 words[12];
    std::memcpy(words, mtx, sizeof(words));
    PCGXWriteXF(PC_XF_POSMTX + id * 4, 12, words);
}

void GXLoadNrmMtxImm(const f32 mtx[3][4], u32 id) {
    u32 words[9];
    for (u32 r = 0; r < 3; r++) {
        for (u32 c = 0; c < 3; c++) {
            words[r * 3 + c] = FloatBits(mtx[r][c]);
        }
    }
    PCGXWriteXF(PC_XF_NRMMTX + id * 3, 9, words);
}

void GXLoadNrmMtxImm3x3(const f32 mtx[3][3], u32 id) {
    u32 words[9];
    std::memcpy(words, mtx, sizeof(words));
    PCGXWriteXF(PC_XF_NRMMTX + id * 3, 9, words);
}

void GXLoadTexMtxImm(const f32 mtx[][4], u32 id, GXTexMtxType type) {
    u32 address = id >= GX_PTTEXMTX0 ? PC_XF_POSTMTX + (id - GX_PTTEXMTX0) * 4 : PC_XF_POSMTX + id * 4;
    u32 count = type == GX_MTX2x4 ? 8 : 12;
    u32 words[12];
    std::memcpy(words, mtx, count * 4);
    PCGXWriteXF(address, count, words);
}

// Indexed loads: the matrix comes from the array given to
// GXSetArray(GX_POS_MTX_ARRAY / GX_NRM_MTX_ARRAY / GX_TEX_MTX_ARRAY).
static void IndexedLoad(u8 command, u16 index, u32 address, u32 count) {
    u32 arg = ((count - 1) << 12) | address;
    u8 bytes[5] = {command, static_cast<u8>(index >> 8), static_cast<u8>(index), static_cast<u8>(arg >> 8),
                   static_cast<u8>(arg)};
    PCGXFifoWrite(bytes, 5);
}

void GXLoadPosMtxIndx(u16 index, u32 id) {
    IndexedLoad(0x20, index, PC_XF_POSMTX + id * 4, 12);
}

void GXLoadNrmMtxIndx3x3(u16 index, u32 id) {
    IndexedLoad(0x28, index, PC_XF_NRMMTX + id * 3, 9);
}

void GXLoadTexMtxIndx(u16 index, u32 id, GXTexMtxType type) {
    u32 address = id >= GX_PTTEXMTX0 ? PC_XF_POSTMTX + (id - GX_PTTEXMTX0) * 4 : PC_XF_POSMTX + id * 4;
    IndexedLoad(0x30, index, address, type == GX_MTX2x4 ? 8 : 12);
}

void GXSetCurrentMtx(u32 id) {
    SendMatrixIndices(SetField(gPCGX.cpMatIndexA, 0, 6, id), gPCGX.cpMatIndexB);
}

void GXSetViewportJitter(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz, u32 field) {
    if (field == 0) {
        top -= 0.5f;
    }
    PCGXState& s = gPCGX;
    s.viewport[0] = left;
    s.viewport[1] = top;
    s.viewport[2] = wd;
    s.viewport[3] = ht;
    s.viewport[4] = nearz;
    s.viewport[5] = farz;
    SendViewport();
}

void GXSetViewport(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz) {
    GXSetViewportJitter(left, top, wd, ht, nearz, farz, 1);
}

void GXGetViewportv(f32* viewport) {
    std::memcpy(viewport, gPCGX.viewport, sizeof(gPCGX.viewport));
}

void GXSetZScaleOffset(f32 scale, f32 offset) {
    gPCGX.zOffset = offset * 16777215.0f;
    gPCGX.zScale = scale * 16777215.0f + 1.0f;
    SendViewport();
}

void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht) {
    PCGXState& s = gPCGX;
    s.scissor[0] = left;
    s.scissor[1] = top;
    s.scissor[2] = wd;
    s.scissor[3] = ht;
    u32 tp = top + 342, lf = left + 342;
    PCGXWriteBP(PC_BP_SCISSOR_TL, (tp & 0x7FF) | ((lf & 0x7FF) << 12));
    PCGXWriteBP(PC_BP_SCISSOR_BR, ((tp + ht - 1) & 0x7FF) | (((lf + wd - 1) & 0x7FF) << 12));
}

void GXGetScissor(u32* left, u32* top, u32* wd, u32* ht) {
    *left = gPCGX.scissor[0];
    *top = gPCGX.scissor[1];
    *wd = gPCGX.scissor[2];
    *ht = gPCGX.scissor[3];
}

void GXSetScissorBoxOffset(s32 x, s32 y) {
    u32 hx = static_cast<u32>(x + 342) >> 1;
    u32 hy = static_cast<u32>(y + 342) >> 1;
    PCGXWriteBP(PC_BP_SCISSOR_OFFSET, (hx & 0x3FF) | ((hy & 0x3FF) << 10));
}

void GXSetTexCoordGen2(GXTexCoordID coord, GXTexGenType func, GXTexGenSrc src, u32 mtx, GXBool normalize, u32 postMtx) {
    u32 row = 5, form = 0;
    switch (src) {
    case GX_TG_POS:
        row = 0, form = 1;
        break;
    case GX_TG_NRM:
        row = 1, form = 1;
        break;
    case GX_TG_BINRM:
        row = 3, form = 1;
        break;
    case GX_TG_TANGENT:
        row = 4, form = 1;
        break;
    case GX_TG_COLOR0:
    case GX_TG_COLOR1:
        row = 2;
        break;
    default:
        if (src >= GX_TG_TEX0 && src <= GX_TG_TEX7) {
            row = 5 + (src - GX_TG_TEX0);
        }
        break;
    }

    u32 reg = 0;
    switch (func) {
    case GX_TG_MTX2x4:
        reg = (form << 2) | (row << 7);
        break;
    case GX_TG_MTX3x4:
        reg = (1u << 1) | (form << 2) | (row << 7);
        break;
    case GX_TG_SRTG:
        reg = (form << 2) | ((src == GX_TG_COLOR0 ? 2u : 3u) << 4) | (2u << 7);
        break;
    default: // GX_TG_BUMP0 - GX_TG_BUMP7
        reg = (form << 2) | (1u << 4) | (row << 7) | ((static_cast<u32>(src - GX_TG_TEXCOORD0) & 7) << 12) |
              ((static_cast<u32>(func - GX_TG_BUMP0) & 7) << 15);
        break;
    }
    PCGXWriteXF1(PC_XF_TEX0 + coord, reg);
    PCGXWriteXF1(PC_XF_DUALTEX0 + coord, ((postMtx - GX_PTTEXMTX0) & 63) | (normalize ? 1u << 8 : 0));

    u32 a = gPCGX.cpMatIndexA, b = gPCGX.cpMatIndexB;
    if (coord < GX_TEXCOORD4) {
        a = SetField(a, 6 + coord * 6, 6, mtx);
    } else {
        b = SetField(b, (coord - GX_TEXCOORD4) * 6, 6, mtx);
    }
    SendMatrixIndices(a, b);
}

void GXSetNumTexGens(u8 num) {
    BPField(PC_BP_GENMODE, 0, 4, num);
    PCGXWriteXF1(PC_XF_NUMTEX, num);
}

// --- Lighting ------------------------------------------------------------------------

void GXSetNumChans(u8 num) {
    BPField(PC_BP_GENMODE, 4, 3, num);
    PCGXWriteXF1(PC_XF_NUMCOLORS, num);
}

static void SetChanColor(u32 base, GXChannelID chan, GXColor color) {
    u32 rgba = (static_cast<u32>(color.r) << 24) | (static_cast<u32>(color.g) << 16) | (static_cast<u32>(color.b) << 8) |
               color.a;
    u32 index;
    u32 reg;
    switch (chan) {
    case GX_COLOR0:
    case GX_COLOR1:
        index = chan - GX_COLOR0;
        reg = (gPCGX.xf[base + index] & 0xFF) | (rgba & 0xFFFFFF00);
        break;
    case GX_ALPHA0:
    case GX_ALPHA1:
        index = chan - GX_ALPHA0;
        reg = (gPCGX.xf[base + index] & 0xFFFFFF00) | color.a;
        break;
    case GX_COLOR0A0:
    case GX_COLOR1A1:
        index = chan - GX_COLOR0A0;
        reg = rgba;
        break;
    default:
        return;
    }
    PCGXWriteXF1(base + index, reg);
}

void GXSetChanAmbColor(GXChannelID chan, GXColor color) {
    SetChanColor(PC_XF_AMBIENT0, chan, color);
}

void GXSetChanMatColor(GXChannelID chan, GXColor color) {
    SetChanColor(PC_XF_MATERIAL0, chan, color);
}

void GXSetChanCtrl(GXChannelID chan, GXBool enable, GXColorSrc ambSrc, GXColorSrc matSrc, u32 lightMask,
                   GXDiffuseFn diffuseFn, GXAttnFn attnFn) {
    u32 reg = 0;
    reg |= (matSrc & 1u);
    reg |= (enable ? 1u : 0u) << 1;
    reg |= (lightMask & 0xF) << 2;
    reg |= (ambSrc & 1u) << 6;
    reg |= ((attnFn == GX_AF_SPEC ? GX_DF_NONE : diffuseFn) & 3u) << 7;
    reg |= (attnFn != GX_AF_NONE ? 1u : 0u) << 9;
    reg |= (attnFn != GX_AF_SPEC ? 1u : 0u) << 10;
    reg |= ((lightMask >> 4) & 0xF) << 11;

    u32 index = chan & 3; // COLOR0, COLOR1, ALPHA0, ALPHA1; COLOR0A0 and COLOR1A1 are 0 and 1 again
    PCGXWriteXF1(PC_XF_COLOR0CNTRL + index, reg);
    if (chan == GX_COLOR0A0) {
        PCGXWriteXF1(PC_XF_ALPHA0CNTRL, reg);
    } else if (chan == GX_COLOR1A1) {
        PCGXWriteXF1(PC_XF_ALPHA0CNTRL + 1, reg);
    }
}

void GXLoadLightObjImm(const GXLightObj* light, GXLightID id) {
    const PCGXLightObj* obj = reinterpret_cast<const PCGXLightObj*>(light);
    u32 index = 0;
    while (index < 7 && !(static_cast<u32>(id) & (1u << index))) {
        index++;
    }
    u32 words[16];
    std::memcpy(words, obj, sizeof(words));
    // The colour is four bytes in the object and one RGBA word in XF memory.
    const u8* color = reinterpret_cast<const u8*>(&obj->color);
    words[3] = (static_cast<u32>(color[0]) << 24) | (static_cast<u32>(color[1]) << 16) |
               (static_cast<u32>(color[2]) << 8) | color[3];
    PCGXWriteXF(PC_XF_LIGHTS + index * 16, 16, words);
}

// --- TEV -------------------------------------------------------------------------------

void GXSetNumTevStages(u8 num) {
    BPField(PC_BP_GENMODE, 10, 4, (num - 1u) & 15);
}

void GXSetTevColorIn(GXTevStageID stage, GXTevColorArg a, GXTevColorArg b, GXTevColorArg c, GXTevColorArg d) {
    u32 reg = gPCGX.bp[PC_BP_TEV_COLOR_ENV0 + stage * 2];
    reg = (reg & ~0xFFFFu) | (d & 15u) | ((c & 15u) << 4) | ((b & 15u) << 8) | ((a & 15u) << 12);
    PCGXWriteBP(PC_BP_TEV_COLOR_ENV0 + stage * 2, reg);
}

void GXSetTevAlphaIn(GXTevStageID stage, GXTevAlphaArg a, GXTevAlphaArg b, GXTevAlphaArg c, GXTevAlphaArg d) {
    u32 reg = gPCGX.bp[PC_BP_TEV_COLOR_ENV0 + stage * 2 + 1];
    reg = (reg & ~0xFFF0u) | ((d & 7u) << 4) | ((c & 7u) << 7) | ((b & 7u) << 10) | ((a & 7u) << 13);
    PCGXWriteBP(PC_BP_TEV_COLOR_ENV0 + stage * 2 + 1, reg);
}

static void SetTevOp(u32 regIndex, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp, GXTevRegID out) {
    u32 reg = gPCGX.bp[regIndex] & 0xFFFF;
    if (op <= GX_TEV_SUB) {
        reg |= (bias & 3u) << 16;
        reg |= (op & 1u) << 18;
        reg |= (scale & 3u) << 20;
    } else {
        // compare: the bias field says "compare", scale and op say which one
        reg |= 3u << 16;
        reg |= (op & 1u) << 18;
        reg |= ((op >> 1) & 3u) << 20;
    }
    reg |= (clamp ? 1u : 0u) << 19;
    reg |= (out & 3u) << 22;
    PCGXWriteBP(regIndex, reg);
}

void GXSetTevColorOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp, GXTevRegID out) {
    SetTevOp(PC_BP_TEV_COLOR_ENV0 + stage * 2, op, bias, scale, clamp, out);
}

void GXSetTevAlphaOp(GXTevStageID stage, GXTevOp op, GXTevBias bias, GXTevScale scale, GXBool clamp, GXTevRegID out) {
    SetTevOp(PC_BP_TEV_COLOR_ENV0 + stage * 2 + 1, op, bias, scale, clamp, out);
}

void GXSetTevOp(GXTevStageID stage, GXTevMode mode) {
    GXTevColorArg carg = stage == GX_TEVSTAGE0 ? GX_CC_RASC : GX_CC_CPREV;
    GXTevAlphaArg aarg = stage == GX_TEVSTAGE0 ? GX_CA_RASA : GX_CA_APREV;
    switch (mode) {
    case GX_MODULATE:
        GXSetTevColorIn(stage, GX_CC_ZERO, GX_CC_TEXC, carg, GX_CC_ZERO);
        GXSetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_TEXA, aarg, GX_CA_ZERO);
        break;
    case GX_DECAL:
        GXSetTevColorIn(stage, carg, GX_CC_TEXC, GX_CC_TEXA, GX_CC_ZERO);
        GXSetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, aarg);
        break;
    case GX_BLEND:
        GXSetTevColorIn(stage, carg, GX_CC_ONE, GX_CC_TEXC, GX_CC_ZERO);
        GXSetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_TEXA, aarg, GX_CA_ZERO);
        break;
    case GX_REPLACE:
        GXSetTevColorIn(stage, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
        GXSetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
        break;
    default: // GX_PASSCLR
        GXSetTevColorIn(stage, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, carg);
        GXSetTevAlphaIn(stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, aarg);
        break;
    }
    GXSetTevColorOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
}

static void SetTevRegister(u32 id, s32 r, s32 g, s32 b, s32 a, bool konst) {
    u32 type = konst ? 0x800000u : 0;
    u32 ra = (r & 0x7FF) | ((a & 0x7FF) << 12) | type;
    u32 bg = (b & 0x7FF) | ((g & 0x7FF) << 12) | type;
    PCGXWriteBP(PC_BP_TEV_REG_RA0 + id * 2, ra);
    PCGXWriteBP(PC_BP_TEV_REG_RA0 + id * 2 + 1, bg);
}

void GXSetTevColor(GXTevRegID id, GXColor color) {
    SetTevRegister(id, color.r, color.g, color.b, color.a, false);
}

void GXSetTevColorS10(GXTevRegID id, GXColorS10 color) {
    SetTevRegister(id, color.r, color.g, color.b, color.a, false);
}

void GXSetTevKColor(GXTevKColorID id, GXColor color) {
    SetTevRegister(id, color.r, color.g, color.b, color.a, true);
}

void GXSetTevKColorSel(GXTevStageID stage, GXTevKColorSel sel) {
    BPField(PC_BP_TEV_KSEL0 + stage / 2, (stage & 1) ? 14 : 4, 5, sel);
}

void GXSetTevKAlphaSel(GXTevStageID stage, GXTevKAlphaSel sel) {
    BPField(PC_BP_TEV_KSEL0 + stage / 2, (stage & 1) ? 19 : 9, 5, sel);
}

void GXSetTevSwapMode(GXTevStageID stage, GXTevSwapSel ras, GXTevSwapSel tex) {
    u32 reg = gPCGX.bp[PC_BP_TEV_COLOR_ENV0 + stage * 2 + 1];
    reg = (reg & ~0xFu) | (ras & 3u) | ((tex & 3u) << 2);
    PCGXWriteBP(PC_BP_TEV_COLOR_ENV0 + stage * 2 + 1, reg);
}

void GXSetTevSwapModeTable(GXTevSwapSel table, GXTevColorChan r, GXTevColorChan g, GXTevColorChan b, GXTevColorChan a) {
    u32 reg = gPCGX.bp[PC_BP_TEV_KSEL0 + table * 2];
    PCGXWriteBP(PC_BP_TEV_KSEL0 + table * 2, (reg & ~0xFu) | (r & 3u) | ((g & 3u) << 2));
    reg = gPCGX.bp[PC_BP_TEV_KSEL0 + table * 2 + 1];
    PCGXWriteBP(PC_BP_TEV_KSEL0 + table * 2 + 1, (reg & ~0xFu) | (b & 3u) | ((a & 3u) << 2));
}

void GXSetAlphaCompare(GXCompare comp0, u8 ref0, GXAlphaOp op, GXCompare comp1, u8 ref1) {
    PCGXWriteBP(PC_BP_ALPHA_COMPARE, ref0 | (static_cast<u32>(ref1) << 8) | ((comp0 & 7u) << 16) | ((comp1 & 7u) << 19) |
                                         ((op & 3u) << 22));
}

void GXSetTevOrder(GXTevStageID stage, GXTexCoordID coord, GXTexMapID map, GXChannelID color) {
    static const u8 kChannel[9] = {0, 1, 0, 1, 0, 1, 7, 5, 6};
    sTexMapId[stage] = map;
    u32 tmap = static_cast<u32>(map) & ~static_cast<u32>(GX_TEX_DISABLE);
    if (tmap >= GX_MAX_TEXMAP) {
        tmap = GX_TEXMAP0;
    }
    u32 tcoord;
    if (coord >= GX_MAX_TEXCOORD) {
        tcoord = GX_TEXCOORD0;
        sTevCoordEnable &= ~(1u << stage);
    } else {
        tcoord = coord;
        sTevCoordEnable |= 1u << stage;
    }
    u32 channel = (color == GX_COLOR_NULL || static_cast<u32>(color) > 8) ? 7 : kChannel[color];
    bool enable = map != GX_TEXMAP_NULL && !(static_cast<u32>(map) & GX_TEX_DISABLE);

    u32 half = tmap | (tcoord << 3) | ((enable ? 1u : 0u) << 6) | (channel << 7);
    u32 reg = gPCGX.bp[PC_BP_RAS1_TREF0 + stage / 2];
    if (stage & 1) {
        reg = (reg & 0x000FFF) | (half << 12);
    } else {
        reg = (reg & 0xFFF000) | half;
    }
    PCGXWriteBP(PC_BP_RAS1_TREF0 + stage / 2, reg);
}

void GXSetZTexture(GXZTexOp op, GXTexFmt fmt, u32 bias) {
    u32 type = fmt == GX_TF_Z8 ? 0 : (fmt == GX_TF_Z16 ? 1 : 2);
    PCGXWriteBP(PC_BP_ZTEX_BIAS, bias & 0xFFFFFF);
    PCGXWriteBP(PC_BP_ZTEX_MODE, type | ((op & 3u) << 2));
}

// --- Indirect textures ----------------------------------------------------------------

void GXSetNumIndStages(u8 num) {
    BPField(PC_BP_GENMODE, 16, 3, num);
}

void GXSetIndTexOrder(GXIndTexStageID stage, GXTexCoordID coord, GXTexMapID map) {
    u32 m = map == GX_TEXMAP_NULL ? static_cast<u32>(GX_TEXMAP0) : static_cast<u32>(map);
    u32 c = coord == GX_TEXCOORD_NULL ? static_cast<u32>(GX_TEXCOORD0) : static_cast<u32>(coord);
    u32 reg = SetField(gPCGX.bp[PC_BP_RAS1_IREF], stage * 6, 3, m);
    PCGXWriteBP(PC_BP_RAS1_IREF, SetField(reg, stage * 6 + 3, 3, c));
}

void GXSetIndTexCoordScale(GXIndTexStageID stage, GXIndTexScale scaleS, GXIndTexScale scaleT) {
    u32 index = PC_BP_RAS1_SS0 + stage / 2;
    u32 shift = (stage & 1) * 8;
    u32 reg = SetField(gPCGX.bp[index], shift, 4, scaleS);
    PCGXWriteBP(index, SetField(reg, shift + 4, 4, scaleT));
}

void GXSetIndTexMtx(GXIndTexMtxID id, const f32 mtx[2][3], s8 scaleExp) {
    u32 index;
    switch (id) {
    case GX_ITM_0:
    case GX_ITM_1:
    case GX_ITM_2:
        index = id - GX_ITM_0;
        break;
    case GX_ITM_S0:
    case GX_ITM_S1:
    case GX_ITM_S2:
        index = id - GX_ITM_S0;
        break;
    case GX_ITM_T0:
    case GX_ITM_T1:
    case GX_ITM_T2:
        index = id - GX_ITM_T0;
        break;
    default:
        index = 0;
        break;
    }
    u32 scale = static_cast<u32>(scaleExp + 17);
    for (u32 column = 0; column < 3; column++) {
        u32 m0 = static_cast<u32>(static_cast<s32>(mtx[0][column] * 1024.0f)) & 0x7FF;
        u32 m1 = static_cast<u32>(static_cast<s32>(mtx[1][column] * 1024.0f)) & 0x7FF;
        PCGXWriteBP(PC_BP_IND_MTXA0 + index * 3 + column, m0 | (m1 << 11) | (((scale >> (column * 2)) & 3) << 22));
    }
}

void GXSetTevIndirect(GXTevStageID stage, GXIndTexStageID indStage, GXIndTexFormat format, GXIndTexBiasSel bias,
                      GXIndTexMtxID matrix, GXIndTexWrap wrapS, GXIndTexWrap wrapT, GXBool addPrev, GXBool utcLod,
                      GXIndTexAlphaSel alphaSel) {
    u32 reg = (indStage & 3u) | ((format & 3u) << 2) | ((bias & 7u) << 4) | ((alphaSel & 3u) << 7) |
              ((matrix & 15u) << 9) | ((wrapS & 7u) << 13) | ((wrapT & 7u) << 16) | ((utcLod ? 1u : 0u) << 19) |
              ((addPrev ? 1u : 0u) << 20);
    PCGXWriteBP(PC_BP_IND_CMD0 + stage, reg);
}

void GXSetTevDirect(GXTevStageID stage) {
    GXSetTevIndirect(stage, GX_INDTEXSTAGE0, GX_ITF_8, GX_ITB_NONE, GX_ITM_OFF, GX_ITW_OFF, GX_ITW_OFF, GX_FALSE,
                     GX_FALSE, GX_ITBA_OFF);
}

void GXSetTevIndWarp(GXTevStageID stage, GXIndTexStageID indStage, GXBool signedOffset, GXBool replace,
                     GXIndTexMtxID matrix) {
    GXIndTexWrap wrap = replace ? GX_ITW_0 : GX_ITW_OFF;
    GXSetTevIndirect(stage, indStage, GX_ITF_8, signedOffset ? GX_ITB_STU : GX_ITB_NONE, matrix, wrap, wrap, GX_FALSE,
                     GX_FALSE, GX_ITBA_OFF);
}

// --- Pixel engine ----------------------------------------------------------------------

// The fog registers are kept (type and colour) but fog is not drawn yet.
void GXSetFog(GXFogType type, f32 startz, f32 endz, f32 nearz, f32 farz, GXColor color) {
    PCGXWriteBP(PC_BP_FOG_PARAM3, ((static_cast<u32>(type) & 7u) << 21) | (((static_cast<u32>(type) >> 3) & 1u) << 20));
    PCGXWriteBP(PC_BP_FOG_COLOR, (static_cast<u32>(color.r) << 16) | (static_cast<u32>(color.g) << 8) | color.b);
}

void GXSetFogRangeAdj(GXBool enable, u16 center, const GXFogAdjTable* table) {
    PCGXWriteBP(PC_BP_FOG_RANGE, ((center + 342u) & 0x3FF) | ((enable ? 1u : 0u) << 10));
}

void GXSetBlendMode(GXBlendMode type, GXBlendFactor src, GXBlendFactor dst, GXLogicOp op) {
    u32 reg = gPCGX.bp[PC_BP_CMODE0];
    reg = SetField(reg, 0, 1, type == GX_BM_BLEND || type == GX_BM_SUBTRACT);
    reg = SetField(reg, 1, 1, type == GX_BM_LOGIC);
    reg = SetField(reg, 11, 1, type == GX_BM_SUBTRACT);
    reg = SetField(reg, 5, 3, dst);
    reg = SetField(reg, 8, 3, src);
    reg = SetField(reg, 12, 4, op);
    PCGXWriteBP(PC_BP_CMODE0, reg);
}

void GXSetColorUpdate(GXBool enable) {
    BPField(PC_BP_CMODE0, 3, 1, enable);
}

void GXSetAlphaUpdate(GXBool enable) {
    BPField(PC_BP_CMODE0, 4, 1, enable);
}

void GXSetDither(GXBool enable) {
    BPField(PC_BP_CMODE0, 2, 1, enable);
}

void GXSetZMode(GXBool enable, GXCompare func, GXBool update) {
    PCGXWriteBP(PC_BP_ZMODE, (enable ? 1u : 0u) | ((func & 7u) << 1) | ((update ? 1u : 0u) << 4));
}

void GXSetZCompLoc(GXBool beforeTex) {
    BPField(PC_BP_PE_CONTROL, 6, 1, beforeTex);
}

void GXSetPixelFmt(GXPixelFmt pixelFmt, GXZFmt16 zFmt) {
    static const u8 kHw[8] = {0, 1, 2, 3, 4, 4, 4, 5};
    u32 reg = SetField(gPCGX.bp[PC_BP_PE_CONTROL], 0, 3, kHw[pixelFmt & 7]);
    PCGXWriteBP(PC_BP_PE_CONTROL, SetField(reg, 3, 3, zFmt));
}

void GXSetDstAlpha(GXBool enable, u8 alpha) {
    PCGXWriteBP(PC_BP_CMODE1, alpha | ((enable ? 1u : 0u) << 8));
}

void GXSetFieldMask(GXBool, GXBool) {}
void GXSetFieldMode(GXBool, GXBool) {}

// --- Textures ----------------------------------------------------------------------------

void GXLoadTexObj(const GXTexObj* obj, GXTexMapID id) {
    const PCGXTexObj* t = reinterpret_cast<const PCGXTexObj*>(obj);
    if (static_cast<u32>(id) >= 8) {
        return;
    }
    PCGXTexUnit& unit = gPCGX.tex[id];
    unit.image = t->image;
    unit.width = t->width;
    unit.height = t->height;
    unit.format = static_cast<u8>(t->format);
    unit.wrapS = t->wrapS;
    unit.wrapT = t->wrapT;
    unit.minFilter = t->minFilter;
    unit.magFilter = t->magFilter;
    unit.minLod = t->minLod;
    unit.maxLod = t->maxLod;
    unit.lodBias = t->lodBias;
    unit.maxAniso = t->maxAniso;
    if (t->flags & PC_GX_TEX_COLOR_INDEX) {
        unit.tlutSlot = static_cast<u16>(t->tlutName < PC_GX_NUM_TLUTS ? t->tlutName : 0);
        unit.tlutFormat = static_cast<u8>(gPCGX.tluts[unit.tlutSlot].format);
    }
    gPCGX.dirty |= PC_GX_DIRTY_TEXTURES;
}

// Copies the palette into the slot, as the hardware copies it into texture
// memory: later changes to the application's copy need another GXLoadTlut().
void GXLoadTlut(const GXTlutObj* obj, u32 name) {
    const PCGXTlutObj* t = reinterpret_cast<const PCGXTlutObj*>(obj);
    if (name >= PC_GX_NUM_TLUTS || t->data == nullptr) {
        return;
    }
    PCGXTlutSlot& slot = gPCGX.tluts[name];
    if (slot.capacity < t->numEntries) {
        slot.data = static_cast<u8*>(std::realloc(slot.data, t->numEntries * 2u));
        slot.capacity = t->numEntries;
    }
    std::memcpy(slot.data, t->data, t->numEntries * 2u);
    slot.count = t->numEntries;
    slot.format = t->format;
    slot.hash = PCGXHashBytes(slot.data, slot.count * 2);
    // Units that already use the slot see the new palette and its format.
    for (PCGXTexUnit& unit : gPCGX.tex) {
        if (unit.tlutSlot == name) {
            unit.tlutFormat = static_cast<u8>(t->format);
        }
    }
    gPCGX.dirty |= PC_GX_DIRTY_TEXTURES;
}

void GXInvalidateTexAll(void) {
    PCGXTextureNewGeneration();
}

void GXTexModeSync(void) {}

void GXSetTexCoordScaleManually(GXTexCoordID coord, GXBool enable, u16 ss, u16 ts) {
    if (enable) {
        sManualCoordScale |= 1u << coord;
        PCGXWriteBP(PC_BP_SU_SSIZE0 + coord * 2, SetField(gPCGX.bp[PC_BP_SU_SSIZE0 + coord * 2], 0, 16, ss - 1u));
        PCGXWriteBP(PC_BP_SU_SSIZE0 + coord * 2 + 1, SetField(gPCGX.bp[PC_BP_SU_SSIZE0 + coord * 2 + 1], 0, 16, ts - 1u));
    } else {
        sManualCoordScale &= ~(1u << coord);
    }
}

u32 GXGetTexBufferSize(u16 width, u16 height, u32 format, GXBool mipmap, u8 maxLod) {
    u32 size = PCGXTextureDataSize(format, width, height);
    if (mipmap) {
        for (u32 level = 1; level < maxLod; level++) {
            u32 w = width >> level, h = height >> level;
            if (w == 0 && h == 0) {
                break;
            }
            size += PCGXTextureDataSize(format, w ? w : 1, h ? h : 1);
        }
    }
    return size;
}

// --- Frame buffer --------------------------------------------------------------------------

f32 GXGetYScaleFactor(u16 efbHeight, u16 xfbHeight) {
    u32 tgtHt = xfbHeight;
    f32 yScale = static_cast<f32>(xfbHeight) / static_cast<f32>(efbHeight);
    u32 iScale = static_cast<u32>(256.0f / yScale) & 0x1ff;
    u32 realHt = NumXfbLines(efbHeight, iScale);

    while (realHt > xfbHeight) {
        tgtHt--;
        yScale = static_cast<f32>(tgtHt) / static_cast<f32>(efbHeight);
        iScale = static_cast<u32>(256.0f / yScale) & 0x1ff;
        realHt = NumXfbLines(efbHeight, iScale);
    }

    f32 fScale = yScale;
    while (realHt < xfbHeight) {
        fScale = yScale;
        tgtHt++;
        yScale = static_cast<f32>(tgtHt) / static_cast<f32>(efbHeight);
        iScale = static_cast<u32>(256.0f / yScale) & 0x1ff;
        realHt = NumXfbLines(efbHeight, iScale);
    }
    return fScale;
}

u16 GXGetNumXfbLines(u16 efbHeight, f32 yScale) {
    u32 iScale = static_cast<u32>(256.0f / yScale) & 0x1ff;
    return static_cast<u16>(NumXfbLines(efbHeight, iScale));
}

void GXSetDispCopySrc(u16 left, u16 top, u16 width, u16 height) {
    gPCGX.dispCopySrc = {left, top, width, height};
}

void GXSetTexCopySrc(u16 left, u16 top, u16 width, u16 height) {
    gPCGX.texCopySrc = {left, top, width, height};
}

void GXSetDispCopyDst(u16 width, u16 height) {
    gPCGX.dispCopyDstWidth = width;
    gPCGX.dispCopyDstHeight = height;
}

void GXSetTexCopyDst(u16 width, u16 height, GXTexFmt format, GXBool mipmap) {
    gPCGX.texCopyDstWidth = width;
    gPCGX.texCopyDstHeight = height;
    gPCGX.texCopyFormat = format;
    gPCGX.texCopyMipmap = mipmap != GX_FALSE;
}

// Returns the number of XFB lines the copy will write.
u32 GXSetDispCopyYScale(f32 vscale) {
    u32 iScale = static_cast<u32>(256.0f / vscale) & 0x1ff;
    PCGXWriteBP(PC_BP_COPY_YSCALE, iScale);
    return NumXfbLines(gPCGX.dispCopySrc.height, iScale);
}

void GXSetCopyClear(GXColor color, u32 z) {
    PCGXWriteBP(PC_BP_CLEAR_AR, color.r | (static_cast<u32>(color.a) << 8));
    PCGXWriteBP(PC_BP_CLEAR_GB, color.b | (static_cast<u32>(color.g) << 8));
    PCGXWriteBP(PC_BP_CLEAR_Z, z & 0xFFFFFF);
}

// Not drawn: the vertical filter of the display copy, its gamma and the
// field modes change the picture only slightly.
void GXSetCopyFilter(GXBool, const u8[12][2], GXBool, const u8[7]) {}
void GXSetDispCopyGamma(GXGamma) {}
void GXSetDispCopyFrame2Field(GXCopyMode) {}
void GXSetCopyClamp(GXFBClamp) {}
void GXClearBoundingBox(void) {}

static void SendCopySource(const PCGXCopyRect& rect) {
    PCGXLoadBP((PC_BP_EFB_SRC_TL << 24) | (rect.left & 0x3FFu) | ((rect.top & 0x3FFu) << 10));
    PCGXLoadBP((PC_BP_EFB_SRC_SIZE << 24) | ((rect.width - 1u) & 0x3FF) | (((rect.height - 1u) & 0x3FF) << 10));
}

// The EFB (the display copy source) becomes the picture of the XFB `dest`.
// The VI backend shows it when the application selects that XFB
// (VISetNextFrameBuffer). `dest` itself is not written.
void GXCopyDisp(void* dest, GXBool clear) {
    SendCopySource(gPCGX.dispCopySrc);
    PCGXRenderCopyDisp(dest, clear != GX_FALSE);
}

// Encodes the EFB (the texture copy source) into `dest`, with 16-bit texels
// in host byte order.
void GXCopyTex(void* dest, GXBool clear) {
    SendCopySource(gPCGX.texCopySrc);
    PCGXRenderCopyTex(dest, clear != GX_FALSE);
}

void GXPeekARGB(u16 x, u16 y, u32* color) {
    if (!PCGXRenderPeek(x, y, color, nullptr)) {
        *color = 0;
    }
}

void GXPeekZ(u16 x, u16 y, u32* z) {
    if (!PCGXRenderPeek(x, y, nullptr, z)) {
        *z = 0xFFFFFF;
    }
}

// --- Synchronisation -------------------------------------------------------------------------

// Everything is drawn when it is submitted, so there is nothing to wait for.
void GXFlush(void) {}
void GXPixModeSync(void) {}
void GXAbortFrame(void) {}
void GXSetMisc(GXMiscToken, u32) {}

GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback callback) {
    GXDrawSyncCallback old = sDrawSyncCallback;
    sDrawSyncCallback = callback;
    return old;
}

GXDrawDoneCallback GXSetDrawDoneCallback(GXDrawDoneCallback callback) {
    GXDrawDoneCallback old = sDrawDoneCallback;
    sDrawDoneCallback = callback;
    return old;
}

void GXSetDrawSync(u16 token) {
    if (sDrawSyncCallback != nullptr) {
        sDrawSyncCallback(token);
    }
}

void GXSetDrawDone(void) {
    if (sDrawDoneCallback != nullptr) {
        sDrawDoneCallback();
    }
}

void GXDrawDone(void) {
    GXSetDrawDone();
}

// --- Display lists -----------------------------------------------------------------------------

// Until GXEndDisplayList(), FIFO writes and the register loads of the API go
// to `list` as a big-endian command stream instead of being executed. What a
// register cannot hold is not recorded: GXLoadTexObj(), GXLoadTlut(),
// GXSetArray() and the EFB copies act at once.
void GXBeginDisplayList(void* list, u32 size) {
    PCGXState& s = gPCGX;
    s.recordBuffer = static_cast<u8*>(list);
    s.recordSize = size;
    s.recordUsed = 0;
    s.recordOverflow = false;
}

// Returns the size of the list, padded to 32 bytes with NOPs; 0 if it did
// not fit.
u32 GXEndDisplayList(void) {
    PCGXState& s = gPCGX;
    if (s.recordBuffer == nullptr) {
        return 0;
    }
    u32 used = s.recordUsed;
    bool overflow = s.recordOverflow;
    u32 padded = (used + 31) & ~31u;
    if (padded > s.recordSize) {
        overflow = true;
    } else {
        std::memset(s.recordBuffer + used, 0, padded - used);
    }
    s.recordBuffer = nullptr;
    s.recordSize = s.recordUsed = 0;
    s.recordOverflow = false;
    return overflow ? 0 : padded;
}

void GXCallDisplayList(const void* list, u32 size) {
    if (gPCGX.recordBuffer != nullptr) {
        u32 pointer = reinterpret_cast<u32>(list);
        u8 bytes[9] = {0x40,
                       static_cast<u8>(pointer >> 24),
                       static_cast<u8>(pointer >> 16),
                       static_cast<u8>(pointer >> 8),
                       static_cast<u8>(pointer),
                       static_cast<u8>(size >> 24),
                       static_cast<u8>(size >> 16),
                       static_cast<u8>(size >> 8),
                       static_cast<u8>(size)};
        PCGXFifoWrite(bytes, 9);
        return;
    }
    SendCoordScales();
    PCGXExecuteList(list, size);
}

// --- Not implemented: the SDK's debug shapes (milestone 6) ----------------------------------------

PC_NOOP void GXDrawCube(void) {}
PC_NOOP void GXDrawCylinder(u8 sides) {}
PC_NOOP void GXDrawSphere(u32 stacks, u32 sectors) {}
PC_NOOP void GXDrawTorus(f32 rc, u8 numc, u8 numt) {}

} // extern "C"

void PCGXSetArrayBigEndian(u32 attr, bool bigEndian) {
    u32 index = ArrayIndex(static_cast<GXAttr>(attr));
    if (index < PC_GX_NUM_ARRAYS) {
        gPCGX.arrays[index].bigEndian = bigEndian;
    }
}
