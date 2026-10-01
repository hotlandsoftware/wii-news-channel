#include <revolution/gx/GXEnum.h>
#include <revolution/gx/GXGeometry.h>
#include <revolution/private/xf_mem.h>
#include <revolution/gx.h>
#include <revolution/gx/GXRegs.h>
#include <revolution/gx/shortcut_xf_mem.h>


static inline void __GXXfVtxSpecs(void) {
    u32 nCols, nNrm, nTex, reg;
    nNrm = (gx->hasBiNrms) ? 2 : ((gx->hasNrms) ? 1 : 0);
    nCols = (u32)__cntlzw((gx->vcdLo & CP_VCD_REG_LO_COLALL_MASK) >> CP_VCD_REG_LO_COLALL_SHIFT);
    nCols = (33 - nCols) >> 1;
    nTex = (u32)__cntlzw((gx->vcdHi & CP_VCD_REG_HI_TEXALL_MASK) >> CP_VCD_REG_HI_TEXALL_SHIFT);
    nTex = (33 - nTex) >> 1;
    reg = XF_INVERTEXSPEC_F(nCols, nNrm, nTex);
    GX_WRITE_XF_REG(0x1008, reg, 0);
    gx->bpSentNot = GX_TRUE;
}

static inline void SETVCDATTR(GXAttr Attr, GXAttrType Type) {
    switch (Attr) {
    case GX_VA_PNMTXIDX:
        SC_CP_VCD_REG_LO_SET_PMIDX(gx->vcdLo, Type);
        break;
    case GX_VA_TEX0MTXIDX:
        SC_CP_VCD_REG_LO_SET_T0MIDX(gx->vcdLo, Type);
        break;
    case GX_VA_TEX1MTXIDX:
        SC_CP_VCD_REG_LO_SET_T1MIDX(gx->vcdLo, Type);
        break;
    case GX_VA_TEX2MTXIDX:
        SC_CP_VCD_REG_LO_SET_T2MIDX(gx->vcdLo, Type);
        break;
    case GX_VA_TEX3MTXIDX:
        SC_CP_VCD_REG_LO_SET_T3MIDX(gx->vcdLo, Type);
        break;
    case GX_VA_TEX4MTXIDX:
        SC_CP_VCD_REG_LO_SET_T4MIDX(gx->vcdLo, Type);
        break;
    case GX_VA_TEX5MTXIDX:
        SC_CP_VCD_REG_LO_SET_T5MIDX(gx->vcdLo, Type);
        break;
    case GX_VA_TEX6MTXIDX:
        SC_CP_VCD_REG_LO_SET_T6MIDX(gx->vcdLo, Type);
        break;
    case GX_VA_TEX7MTXIDX:
        SC_CP_VCD_REG_LO_SET_T7MIDX(gx->vcdLo, Type);
        break;
    case GX_VA_POS:
        SC_CP_VCD_REG_LO_SET_POS(gx->vcdLo, Type);
        break;
    case GX_VA_NRM:
        if (Type != GX_NONE) {
            gx->hasNrms = TRUE;
            gx->hasBiNrms = FALSE;
            gx->nrmType = Type;
        } else {
            gx->hasNrms = FALSE;
        }
        break;
    case GX_VA_NBT:
        if (Type != GX_NONE) {
            gx->hasBiNrms = TRUE;
            gx->hasNrms = FALSE;
            gx->nrmType = Type;
        } else {
            gx->hasBiNrms = FALSE;
        }
        break;
    case GX_VA_CLR0:
        SC_CP_VCD_REG_LO_SET_COL0(gx->vcdLo, Type);
        break;
    case GX_VA_CLR1:
        SC_CP_VCD_REG_LO_SET_COL1(gx->vcdLo, Type);
        break;
    case GX_VA_TEX0:
        SC_CP_VCD_REG_HI_SET_TEX0(gx->vcdHi, Type);
        break;
    case GX_VA_TEX1:
        SC_CP_VCD_REG_HI_SET_TEX1(gx->vcdHi, Type);
        break;
    case GX_VA_TEX2:
        SC_CP_VCD_REG_HI_SET_TEX2(gx->vcdHi, Type);
        break;
    case GX_VA_TEX3:
        SC_CP_VCD_REG_HI_SET_TEX3(gx->vcdHi, Type);
        break;
    case GX_VA_TEX4:
        SC_CP_VCD_REG_HI_SET_TEX4(gx->vcdHi, Type);
        break;
    case GX_VA_TEX5:
        SC_CP_VCD_REG_HI_SET_TEX5(gx->vcdHi, Type);
        break;
    case GX_VA_TEX6:
        SC_CP_VCD_REG_HI_SET_TEX6(gx->vcdHi, Type);
        break;
    case GX_VA_TEX7:
        SC_CP_VCD_REG_HI_SET_TEX7(gx->vcdHi, Type);
        break;
    default:
        break;
    }
}

void GXSetVtxDesc(GXAttr attr, GXAttrType type) {
    SETVCDATTR(attr, type);
    if (gx->hasNrms || gx->hasBiNrms) {
        SC_CP_VCD_REG_LO_SET_NRM(gx->vcdLo, gx->nrmType);
    } else {
        SC_CP_VCD_REG_LO_SET_NRM(gx->vcdLo, GX_NONE);
    }

    gx->dirtyState |= 0x8;
}

void GXSetVtxDescv(const GXVtxDescList *attrPtr) {

    while (attrPtr->attr != GX_VA_NULL) {
        SETVCDATTR(attrPtr->attr, attrPtr->type);
        attrPtr++;
    }

    if (gx->hasNrms || gx->hasBiNrms) {
        SC_CP_VCD_REG_LO_SET_NRM(gx->vcdLo, gx->nrmType);
    } else {
        SC_CP_VCD_REG_LO_SET_NRM(gx->vcdLo, 0);
    }
    gx->dirtyState |= 8;
}

void __GXSetVCD(void) {
    GX_WRITE_CP_STRM_REG(CP_VCD_LO, 0, gx->vcdLo);
    GX_WRITE_CP_STRM_REG(CP_VCD_HI, 0, gx->vcdHi);
    __GXXfVtxSpecs();
}

void __GXCalculateVLim(void) {
    if (gx->vNum) {
        static u8 tbl1[4] = {0, 4, 1, 2};
        static u8 tbl2[4] = {0, 8, 1, 2};
        static u8 tbl3[4] = {0, 12, 1, 2};
        GXCompCnt nc;
        u32 vlm, b, vl, vh, va;
        vl = gx->vcdLo;
        vh = gx->vcdHi;
        va = gx->vatA[GX_VTXFMT0];
        nc = (GXCompCnt)CP_VAT_REG_A_GET_NRMCNT(va);

        vlm = ((vl >> 0) & 0x1);
        vlm += ((vl >> 1) & 0x1);
        vlm += ((vl >> 2) & 0x1);
        vlm += ((vl >> 3) & 0x1);
        vlm += ((vl >> 4) & 0x1);
        vlm += ((vl >> 5) & 0x1);
        vlm += ((vl >> 6) & 0x1);
        vlm += ((vl >> 7) & 0x1);
        vlm += ((vl >> 8) & 0x1);
        vlm += tbl3[((vl >> 9) & 0x3)];
        b = (u32)((nc == GX_NRM_NBT) ? 3 : 1);
        vlm += tbl3[((vl >> 11) & 0x3)] * b;
        vlm += tbl1[((vl >> 13) & 0x3)];
        vlm += tbl1[((vl >> 15) & 0x3)];
        vlm += tbl2[((vh >> (0 * 2)) & 0x3)];
        vlm += tbl2[((vh >> (1 * 2)) & 0x3)];
        vlm += tbl2[((vh >> (2 * 2)) & 0x3)];
        vlm += tbl2[((vh >> (3 * 2)) & 0x3)];
        vlm += tbl2[((vh >> (4 * 2)) & 0x3)];
        vlm += tbl2[((vh >> (5 * 2)) & 0x3)];
        vlm += tbl2[((vh >> (6 * 2)) & 0x3)];
        vlm += tbl2[((vh >> (7 * 2)) & 0x3)];
        gx->vLim = (u16)vlm;
    }
}

void GXGetVtxDesc(GXAttr attr, GXAttrType* type) {
    u32 cpType;


    switch (attr) {
    case GX_VA_PNMTXIDX:   cpType = CP_VCD_REG_LO_GET_PMIDX(gx->vcdLo); break;
    case GX_VA_TEX0MTXIDX: cpType = CP_VCD_REG_LO_GET_T0MIDX(gx->vcdLo); break;
    case GX_VA_TEX1MTXIDX: cpType = CP_VCD_REG_LO_GET_T1MIDX(gx->vcdLo); break;
    case GX_VA_TEX2MTXIDX: cpType = CP_VCD_REG_LO_GET_T2MIDX(gx->vcdLo); break;
    case GX_VA_TEX3MTXIDX: cpType = CP_VCD_REG_LO_GET_T3MIDX(gx->vcdLo); break;
    case GX_VA_TEX4MTXIDX: cpType = CP_VCD_REG_LO_GET_T4MIDX(gx->vcdLo); break;
    case GX_VA_TEX5MTXIDX: cpType = CP_VCD_REG_LO_GET_T5MIDX(gx->vcdLo); break;
    case GX_VA_TEX6MTXIDX: cpType = CP_VCD_REG_LO_GET_T6MIDX(gx->vcdLo); break;
    case GX_VA_TEX7MTXIDX: cpType = CP_VCD_REG_LO_GET_T7MIDX(gx->vcdLo); break;
    case GX_VA_POS:        cpType = CP_VCD_REG_LO_GET_POS(gx->vcdLo); break;
    case GX_VA_NRM:        cpType = gx->hasNrms ? CP_VCD_REG_LO_GET_NRM(gx->vcdLo) : 0; break;
    case GX_VA_NBT:        cpType = gx->hasBiNrms ? CP_VCD_REG_LO_GET_NRM(gx->vcdLo) : 0; break;
    case GX_VA_CLR0:       cpType = CP_VCD_REG_LO_GET_COL0(gx->vcdLo); break;
    case GX_VA_CLR1:       cpType = CP_VCD_REG_LO_GET_COL1(gx->vcdLo); break;
    case GX_VA_TEX0:       cpType = CP_VCD_REG_HI_GET_TEX0(gx->vcdHi);  break;
    case GX_VA_TEX1:       cpType = CP_VCD_REG_HI_GET_TEX1(gx->vcdHi);  break;
    case GX_VA_TEX2:       cpType = CP_VCD_REG_HI_GET_TEX2(gx->vcdHi);  break;
    case GX_VA_TEX3:       cpType = CP_VCD_REG_HI_GET_TEX3(gx->vcdHi);  break;
    case GX_VA_TEX4:       cpType = CP_VCD_REG_HI_GET_TEX4(gx->vcdHi);  break;
    case GX_VA_TEX5:       cpType = CP_VCD_REG_HI_GET_TEX5(gx->vcdHi); break;
    case GX_VA_TEX6:       cpType = CP_VCD_REG_HI_GET_TEX6(gx->vcdHi); break;
    case GX_VA_TEX7:       cpType = CP_VCD_REG_HI_GET_TEX7(gx->vcdHi); break;
    default: cpType = 0; break;
    }
    *type = cpType;
}

void GXGetVtxDescv(GXVtxDescList* vcd) {
    GXAttr attr;


    for (attr = GX_VA_PNMTXIDX; attr <= GX_VA_TEX7; attr++) {
        vcd[attr].attr = attr;
        GXGetVtxDesc(attr, &vcd[attr].type);
    }

    vcd[attr].attr = GX_VA_NBT;
    GXGetVtxDesc(GX_VA_NBT, &vcd[attr].type);

    attr++;
    vcd[attr].attr = GX_VA_NULL;
}

void GXClearVtxDesc(void) {
    gx->vcdLo = 0;
    SC_CP_VCD_REG_LO_SET_POS(gx->vcdLo, GX_DIRECT);
    gx->vcdHi = 0;
    gx->hasNrms = FALSE;
    gx->hasBiNrms = FALSE;
    gx->dirtyState |= 8;
}

static inline void SETVAT(u32* va, u32* vb, u32* vc, GXAttr attr, GXCompCnt cnt, GXCompType type, u8 shft) {
    switch (attr) {
    case GX_VA_POS:
        SC_CP_VAT_REG_A_SET_POSCNT(*va, cnt);
        SC_CP_VAT_REG_A_SET_POSFMT(*va, type);
        SC_CP_VAT_REG_A_SET_POSSHFT(*va, shft);
        break;
    case GX_VA_NRM:
    case GX_VA_NBT:
        SC_CP_VAT_REG_A_SET_NRMFMT(*va, type);
        if (cnt == GX_NRM_NBT3) {
            SC_CP_VAT_REG_A_SET_NRMCNT(*va, GX_NRM_NBT);
            SC_CP_VAT_REG_A_SET_NORMALINDEX3(*va, 1);
        } else {
            SC_CP_VAT_REG_A_SET_NRMCNT(*va, cnt);
            SC_CP_VAT_REG_A_SET_NORMALINDEX3(*va, 0);
        }
        break;
    case GX_VA_CLR0:
        SC_CP_VAT_REG_A_SET_COL0CNT(*va, cnt);
        SC_CP_VAT_REG_A_SET_COL0FMT(*va, type);
        break;
    case GX_VA_CLR1:
        SC_CP_VAT_REG_A_SET_COL1CNT(*va, cnt);
        SC_CP_VAT_REG_A_SET_COL1FMT(*va, type);
        break;
    case GX_VA_TEX0:
        SC_CP_VAT_REG_A_SET_TEX0CNT(*va, cnt);
        SC_CP_VAT_REG_A_SET_TEX0FMT(*va, type);
        SC_CP_VAT_REG_A_SET_TEX0SHFT(*va, shft);
        break;
    case GX_VA_TEX1:
        SC_CP_VAT_REG_B_SET_TEX1CNT(*vb, cnt);
        SC_CP_VAT_REG_B_SET_TEX1FMT(*vb, type);
        SC_CP_VAT_REG_B_SET_TEX1SHFT(*vb, shft);
        break;
    case GX_VA_TEX2:
        SC_CP_VAT_REG_B_SET_TEX2CNT(*vb, cnt);
        SC_CP_VAT_REG_B_SET_TEX2FMT(*vb, type);
        SC_CP_VAT_REG_B_SET_TEX2SHFT(*vb, shft);
        break;
    case GX_VA_TEX3:
        SC_CP_VAT_REG_B_SET_TEX3CNT(*vb, cnt);
        SC_CP_VAT_REG_B_SET_TEX3FMT(*vb, type);
        SC_CP_VAT_REG_B_SET_TEX3SHFT(*vb, shft);
        break;
    case GX_VA_TEX4:
        SC_CP_VAT_REG_B_SET_TEX4CNT(*vb, cnt);
        SC_CP_VAT_REG_B_SET_TEX4FMT(*vb, type);
        SC_CP_VAT_REG_C_SET_TEX4SHFT(*vc, shft);
        break;
    case GX_VA_TEX5:
        SC_CP_VAT_REG_C_SET_TEX5CNT(*vc, cnt);
        SC_CP_VAT_REG_C_SET_TEX5FMT(*vc, type);
        SC_CP_VAT_REG_C_SET_TEX5SHFT(*vc, shft);
        break;
    case GX_VA_TEX6:
        SC_CP_VAT_REG_C_SET_TEX6CNT(*vc, cnt);
        SC_CP_VAT_REG_C_SET_TEX6FMT(*vc, type);
        SC_CP_VAT_REG_C_SET_TEX6SHFT(*vc, shft);
        break;
    case GX_VA_TEX7:
        SC_CP_VAT_REG_C_SET_TEX7CNT(*vc, cnt);
        SC_CP_VAT_REG_C_SET_TEX7FMT(*vc, type);
        SC_CP_VAT_REG_C_SET_TEX7SHFT(*vc, shft);
        break;
    default:
        break;
    }
}

void GXSetVtxAttrFmt(GXVtxFmt vtxfmt, GXAttr attr, GXCompCnt cnt, GXCompType type, u8 frac) {
    u32 *va, *vb, *vc;
    va = &gx->vatA[vtxfmt];
    vb = &gx->vatB[vtxfmt];
    vc = &gx->vatC[vtxfmt];
    SETVAT(va, vb, vc, attr, cnt, type, frac);
    gx->dirtyState |= 0x10;
    gx->dirtyVAT |= (u8)(1 << (u8)vtxfmt);
}

void GXSetVtxAttrFmtv(GXVtxFmt vtxfmt, const GXVtxAttrFmtList* list) {
    u32 *va, *vb, *vc;
    va = &gx->vatA[vtxfmt];
    vb = &gx->vatB[vtxfmt];
    vc = &gx->vatC[vtxfmt];

    while (list->attr != GX_VA_NULL) {
        SETVAT(va, vb, vc, list->attr, list->cnt, list->type, list->frac);
        list++;
    }

    gx->dirtyState |= 0x10;
    gx->dirtyVAT |= (u8)(1 << (u8)vtxfmt);
}

void __GXSetVAT(void) {
    s32 i;
    u32 dirty = gx->dirtyVAT;
    i = 0;

    do {
        if (dirty & 0x1) {
            GX_WRITE_CP_STRM_REG(CP_VAT_A, i, gx->vatA[i]);
            GX_WRITE_CP_STRM_REG(CP_VAT_B, i, gx->vatB[i]);
            GX_WRITE_CP_STRM_REG(CP_VAT_C, i, gx->vatC[i]);
        }

        dirty >>= 1;
        i++;
    } while (dirty);

    GX_WRITE_U8(0);
    gx->dirtyVAT = 0;
}

static inline u8 GetFracForNrm(GXCompType type) {
    u8 frac;

    switch (type) {
    case GX_S8:
        frac = 6;
        break;
    case GX_S16:
        frac = 14;
        break;
    default:
    case GX_U16:
        frac = 0;
        break;
    }

    return frac;
}

void GXGetVtxAttrFmt(GXVtxFmt fmt, GXAttr attr, GXCompCnt* cnt, GXCompType* type, u8* frac) {
    u32* va;
    u32* vb;
    u32* vc;


    va = &gx->vatA[fmt];
    vb = &gx->vatB[fmt];
    vc = &gx->vatC[fmt];

    switch (attr) {
    case GX_VA_POS:
        *cnt  = CP_VAT_REG_A_GET_POSCNT(*va);
        *type = CP_VAT_REG_A_GET_POSFMT(*va);
        *frac = CP_VAT_REG_A_GET_POSSHFT(*va);
        return;
    case GX_VA_NRM:
    case GX_VA_NBT:
        *cnt = CP_VAT_REG_A_GET_NRMCNT(*va);
        if (*cnt == GX_TEX_ST && CP_VAT_REG_A_GET_NORMALINDEX3(*va)) {
            *cnt = GX_NRM_NBT3;
        }
        *type = CP_VAT_REG_A_GET_NRMFMT(*va);
        *frac = GetFracForNrm(*type);
        return;
    case GX_VA_CLR0:
        *cnt  = CP_VAT_REG_A_GET_COL0CNT(*va);
        *type = CP_VAT_REG_A_GET_COL0FMT(*va);
        *frac = 0;
        return;
    case GX_VA_CLR1:
        *cnt  = CP_VAT_REG_A_GET_COL1CNT(*va);
        *type = CP_VAT_REG_A_GET_COL1FMT(*va);
        *frac = 0;
        return;
    case GX_VA_TEX0:
        *cnt  = CP_VAT_REG_A_GET_TEX0CNT(*va);
        *type = CP_VAT_REG_A_GET_TEX0FMT(*va);
        *frac = CP_VAT_REG_A_GET_TEX0SHFT(*va);
        return;
    case GX_VA_TEX1:
        *cnt  = CP_VAT_REG_B_GET_TEX1CNT(*vb);
        *type = CP_VAT_REG_B_GET_TEX1FMT(*vb);
        *frac = CP_VAT_REG_B_GET_TEX1SHFT(*vb);
        return;
    case GX_VA_TEX2:
        *cnt  = CP_VAT_REG_B_GET_TEX2CNT(*vb);
        *type = CP_VAT_REG_B_GET_TEX2FMT(*vb);
        *frac = CP_VAT_REG_B_GET_TEX2SHFT(*vb);
        return;
    case GX_VA_TEX3:
        *cnt  = CP_VAT_REG_B_GET_TEX3CNT(*vb);
        *type = CP_VAT_REG_B_GET_TEX3FMT(*vb);
        *frac = CP_VAT_REG_B_GET_TEX3SHFT(*vb);
        return;
    case GX_VA_TEX4:
        *cnt  = CP_VAT_REG_B_GET_TEX4CNT(*vb);
        *type = CP_VAT_REG_B_GET_TEX4FMT(*vb);
        *frac = CP_VAT_REG_C_GET_TEX4SHFT(*vc);
        return;
    case GX_VA_TEX5:
        *cnt  = CP_VAT_REG_C_GET_TEX5CNT(*vc);
        *type = CP_VAT_REG_C_GET_TEX5FMT(*vc);
        *frac = CP_VAT_REG_C_GET_TEX5SHFT(*vc);
        return;
    case GX_VA_TEX6:
        *cnt  = CP_VAT_REG_C_GET_TEX6CNT(*vc);
        *type = CP_VAT_REG_C_GET_TEX6FMT(*vc);
        *frac = CP_VAT_REG_C_GET_TEX6SHFT(*vc);
        return;
    case GX_VA_TEX7:
        *cnt  = CP_VAT_REG_C_GET_TEX7CNT(*vc);
        *type = CP_VAT_REG_C_GET_TEX7FMT(*vc);
        *frac = CP_VAT_REG_C_GET_TEX7SHFT(*vc);
        return;
    default:
        *cnt = GX_POS_XYZ;
        *type = GX_U8;
        *frac = 0;
        return;
    }
}

void GXGetVtxAttrFmtv(GXVtxFmt fmt, GXVtxAttrFmtList* vat) {
    GXAttr attr;


    for (attr = GX_VA_POS; attr <= GX_VA_TEX7; attr++) {
        vat->attr = attr;
        GXGetVtxAttrFmt(fmt, attr, &vat->cnt, &vat->type, &vat->frac);
        vat++;
    }

    vat->attr = GX_VA_NULL;
}

void GXSetArray(GXAttr attr, const void* base_ptr, u8 stride) {
    GXAttr cpAttr;
    u32 phyAddr;
    attr = (attr == GX_VA_NBT) ? GX_VA_NRM : attr;
    cpAttr = (GXAttr)(attr - GX_VA_POS);
    phyAddr = GX_PHY_ADDR(base_ptr);
    GX_WRITE_CP_STRM_REG(CP_ARRAY_BASE, cpAttr, phyAddr);
    GX_WRITE_CP_STRM_REG(CP_ARRAY_STRIDE, cpAttr, (u32)stride);
}

void GXInvalidateVtxCache(void) {
    GX_WRITE_U8(0x48);
}

void GXSetTexCoordGen2(GXTexCoordID dst_coord, GXTexGenType func, GXTexGenSrc src_param, u32 mtx, GXBool normalize, u32 pt_texmtx) {
    u32 reg = 0, row, bumprow, form;
    GXAttr mtxIdAttr;
    form = 0;
    row = bumprow = 5;

    switch (src_param) {
    case GX_TG_POS:
        row = 0;
        form = 1;
        break;
    case GX_TG_NRM:
        row = 1;
        form = 1;
        break;
    case GX_TG_BINRM:
        row = 3;
        form = 1;
        break;
    case GX_TG_TANGENT:
        row = 4;
        form = 1;
        break;
    case GX_TG_COLOR0:
        row = 2;
        break;
    case GX_TG_COLOR1:
        row = 2;
        break;
    case GX_TG_TEX0:
        row = 5;
        break;
    case GX_TG_TEX1:
        row = 6;
        break;
    case GX_TG_TEX2:
        row = 7;
        break;
    case GX_TG_TEX3:
        row = 8;
        break;
    case GX_TG_TEX4:
        row = 9;
        break;
    case GX_TG_TEX5:
        row = 0xA;
        break;
    case GX_TG_TEX6:
        row = 0xB;
        break;
    case GX_TG_TEX7:
        row = 0xC;
        break;
    case GX_TG_TEXCOORD0:
        bumprow = 5;
        break;
    case GX_TG_TEXCOORD1:
        bumprow = 6;
        break;
    case GX_TG_TEXCOORD2:
        bumprow = 7;
        break;
    case GX_TG_TEXCOORD3:
        bumprow = 8;
        break;
    case GX_TG_TEXCOORD4:
        bumprow = 9;
        break;
    case GX_TG_TEXCOORD5:
        bumprow = 0xA;
        break;
    case GX_TG_TEXCOORD6:
        bumprow = 0xB;
        break;
    default:
        break;
    }

    switch (func) {
    case GX_TG_MTX2x4:
        SC_XF_TEX_SET_PROJECTION((reg), 0);
        SC_XF_TEX_SET_INPUT_FORM((reg), form);
        SC_XF_TEX_SET_TEXGEN_TYPE((reg), 0);
        SC_XF_TEX_SET_SOURCE_ROW((reg), row);
        break;
    case GX_TG_MTX3x4:
        SC_XF_TEX_SET_PROJECTION((reg), 1);
        SC_XF_TEX_SET_INPUT_FORM((reg), form);
        SC_XF_TEX_SET_TEXGEN_TYPE((reg), 0);
        SC_XF_TEX_SET_SOURCE_ROW((reg), row);
        break;
    case GX_TG_BUMP0:
    case GX_TG_BUMP1:
    case GX_TG_BUMP2:
    case GX_TG_BUMP3:
    case GX_TG_BUMP4:
    case GX_TG_BUMP5:
    case GX_TG_BUMP6:
    case GX_TG_BUMP7:
        SC_XF_TEX_SET_PROJECTION((reg), 0);
        SC_XF_TEX_SET_INPUT_FORM((reg), form);
        SC_XF_TEX_SET_TEXGEN_TYPE((reg), 1);
        SC_XF_TEX_SET_SOURCE_ROW((reg), row);
        SC_XF_TEX_SET_BUMP_MAP_SOURCE((reg), (src_param - GX_TG_TEXCOORD0));
        SC_XF_TEX_SET_BUMP_MAP_LIGHT((reg), (func - GX_TG_BUMP0));
        break;

    case GX_TG_SRTG:
        SC_XF_TEX_SET_PROJECTION((reg), 0);
        SC_XF_TEX_SET_INPUT_FORM((reg), form);
        if (src_param == GX_TG_COLOR0) {
            SC_XF_TEX_SET_TEXGEN_TYPE((reg), XF_TEXGEN_COLOR_STRGBC0);
        } else {
            SC_XF_TEX_SET_TEXGEN_TYPE((reg), XF_TEXGEN_COLOR_STRGBC1);
        }

        SC_XF_TEX_SET_SOURCE_ROW((reg), 2);
        break;

    default:
        break;
    }

    gx->texGenCtrl[dst_coord] = reg;
    gx->dirtyState |= (0x10000 << dst_coord);

    reg = 0;
    SC_XF_DUALTEX_F_SET_DUALMATRIX_ADRS(reg, (pt_texmtx - GX_PTTEXMTX0));
    SC_XF_DUALTEX_F_SET_NORMAL_ENABLE(reg, normalize);
    gx->dualTexGenCtrl[dst_coord] = reg;

    switch (dst_coord) {
    case GX_TEXCOORD0:
        SC_CP_MATIDX_REG_A_SET_TEX0IDX(gx->matIdxA, mtx);
        break;
    case GX_TEXCOORD1:
        SC_CP_MATIDX_REG_A_SET_TEX1IDX(gx->matIdxA, mtx);
        break;
    case GX_TEXCOORD2:
        SC_CP_MATIDX_REG_A_SET_TEX2IDX(gx->matIdxA, mtx);
        break;
    case GX_TEXCOORD3:
        SC_CP_MATIDX_REG_A_SET_TEX3IDX(gx->matIdxA, mtx);
        break;
    case GX_TEXCOORD4:
        SC_CP_MATIDX_REG_B_SET_TEX4IDX(gx->matIdxB, mtx);
        break;
    case GX_TEXCOORD5:
        SC_CP_MATIDX_REG_B_SET_TEX5IDX(gx->matIdxB, mtx);
        break;
    case GX_TEXCOORD6:
        SC_CP_MATIDX_REG_B_SET_TEX6IDX(gx->matIdxB, mtx);
        break;
    default:
        SC_CP_MATIDX_REG_B_SET_TEX7IDX(gx->matIdxB, mtx);
        break;
    }

    mtxIdAttr = (GXAttr)(GX_VA_TEX0MTXIDX + (dst_coord - GX_TEXCOORD0));
    gx->dirtyState |= 0x4000000;
}

void GXSetNumTexGens(u8 nTexGens) {
    SC_GEN_MODE_SET_NTEX(gx->genMode, nTexGens);
    gx->dirtyState |= 0x2000004;
}
