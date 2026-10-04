// GX placeholder: every GX function the program calls, accepted and ignored.
//
// TODO(milestone 3): replace this file with the GX -> OpenGL layer. Everything
// here is weak (PC_NOOP), so a real definition in another file takes over
// function by function.
//
// Two parts:
//
// 1. Functions that only read or write the application's own objects
//    (GXTexObj, GXTlutObj, GXLightObj, GXFogAdjTable), compute a value
//    (GXGetYScaleFactor) or have a getter NW4R reads back (vertex
//    descriptors). These work like the SDK's, because the game and NW4R
//    depend on the results while they build a frame. The PC layout of the
//    objects is in pc_gx_objects.h.
// 2. Everything that would send state or geometry to the graphics processor:
//    no-ops. The FIFO writes of GXBegin()...GXEnd() land in gx_fifo.cpp.

#include <revolution/gx.h>

#include <cmath>
#include <cstring>

#include "pc_gx_objects.h"
#include "pc_noop.h"

namespace {

// Vertex descriptor and attribute formats, kept for GXGetVtxDesc() and
// GXGetVtxAttrFmt().
struct AttrFmt {
    u8 cnt;
    u8 type;
    u8 frac;
};

u8 sVtxDesc[GX_VA_MAX_ATTR];
AttrFmt sVtxAttrFmt[GX_MAX_VTXFMT][GX_VA_MAX_ATTR];

// Height of the display copy source (GXSetDispCopySrc), for GXSetDispCopyYScale().
u16 sDispCopySrcHeight = 480;

GXFifoObj sFifo;

PCGXTexObj* Tex(GXTexObj* obj) {
    return reinterpret_cast<PCGXTexObj*>(obj);
}
const PCGXTexObj* Tex(const GXTexObj* obj) {
    return reinterpret_cast<const PCGXTexObj*>(obj);
}
PCGXLightObj* Light(GXLightObj* obj) {
    return reinterpret_cast<PCGXLightObj*>(obj);
}
const PCGXLightObj* Light(const GXLightObj* obj) {
    return reinterpret_cast<const PCGXLightObj*>(obj);
}

bool IsColorIndex(u32 format) {
    return format == GX_TF_C4 || format == GX_TF_C8 || format == GX_TF_C14X2;
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

} // namespace

extern "C" {

// The SDK's default render modes (src/revolution/GX/GXFrameBuf.c); g3d picks
// one by TV format.
PC_NOOP GXRenderModeObj GXNtsc480IntDf = {
    VI_TVMODE_NTSC_INT, 640, 480, 480, 40, 0, 640, 480, VI_XFBMODE_DF, 0, 0,
    {{6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}},
    {8, 8, 10, 12, 10, 8, 8}};

PC_NOOP GXRenderModeObj GXMpal480IntDf = {
    VI_TVMODE_MPAL_INT, 640, 480, 480, 40, 0, 640, 480, VI_XFBMODE_DF, 0, 0,
    {{6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}},
    {8, 8, 10, 12, 10, 8, 8}};

PC_NOOP GXRenderModeObj GXPal528IntDf = {
    VI_TVMODE_PAL_INT, 640, 528, 528, 40, 23, 640, 528, VI_XFBMODE_DF, 0, 0,
    {{6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}},
    {8, 8, 10, 12, 10, 8, 8}};

PC_NOOP GXRenderModeObj GXEurgb60Hz480IntDf = {
    VI_TVMODE_EURGB60_INT, 640, 480, 480, 40, 0, 640, 480, VI_XFBMODE_DF, 0, 0,
    {{6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}},
    {8, 8, 10, 12, 10, 8, 8}};

// --- Part 1: functions with results ------------------------------------------

// Returns the FIFO object that describes the application's buffer. Nothing
// reads commands from it: FIFO writes go to gx_fifo.cpp.
PC_NOOP GXFifoObj* GXInit(void* base, u32 size) {
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

    std::memset(sVtxDesc, 0, sizeof(sVtxDesc));
    std::memset(sVtxAttrFmt, 0, sizeof(sVtxAttrFmt));
    return &sFifo;
}

PC_NOOP f32 GXGetYScaleFactor(u16 efbHeight, u16 xfbHeight) {
    u32 tgtHt = xfbHeight;
    f32 yScale = (f32)xfbHeight / (f32)efbHeight;
    u32 iScale = (u32)(256.0f / yScale) & 0x1ff;
    u32 realHt = NumXfbLines(efbHeight, iScale);

    while (realHt > xfbHeight) {
        tgtHt--;
        yScale = (f32)tgtHt / (f32)efbHeight;
        iScale = (u32)(256.0f / yScale) & 0x1ff;
        realHt = NumXfbLines(efbHeight, iScale);
    }

    f32 fScale = yScale;
    while (realHt < xfbHeight) {
        fScale = yScale;
        tgtHt++;
        yScale = (f32)tgtHt / (f32)efbHeight;
        iScale = (u32)(256.0f / yScale) & 0x1ff;
        realHt = NumXfbLines(efbHeight, iScale);
    }
    return fScale;
}

PC_NOOP void GXSetDispCopySrc(u16 left, u16 top, u16 width, u16 height) {
    sDispCopySrcHeight = height;
}

// Returns the number of XFB lines the copy will write.
PC_NOOP u32 GXSetDispCopyYScale(f32 vscale) {
    u32 iScale = (u32)(256.0f / vscale) & 0x1ff;
    return NumXfbLines(sDispCopySrcHeight, iScale);
}

PC_NOOP void GXSetVtxDesc(GXAttr attr, GXAttrType type) {
    if (attr == GX_VA_NBT) {
        attr = GX_VA_NRM;
    }
    if ((u32)attr < GX_VA_MAX_ATTR) {
        sVtxDesc[attr] = (u8)type;
    }
}

PC_NOOP void GXClearVtxDesc(void) {
    std::memset(sVtxDesc, 0, sizeof(sVtxDesc));
    sVtxDesc[GX_VA_POS] = GX_DIRECT; // the hardware always has a position
}

PC_NOOP void GXGetVtxDesc(GXAttr attr, GXAttrType* type) {
    if (attr == GX_VA_NBT) {
        attr = GX_VA_NRM;
    }
    *type = (u32)attr < GX_VA_MAX_ATTR ? (GXAttrType)sVtxDesc[attr] : GX_NONE;
}

PC_NOOP void GXSetVtxAttrFmt(GXVtxFmt fmt, GXAttr attr, GXCompCnt cnt, GXCompType type, u8 frac) {
    if (attr == GX_VA_NBT) {
        attr = GX_VA_NRM;
    }
    if ((u32)fmt < GX_MAX_VTXFMT && (u32)attr < GX_VA_MAX_ATTR) {
        sVtxAttrFmt[fmt][attr].cnt = (u8)cnt;
        sVtxAttrFmt[fmt][attr].type = (u8)type;
        sVtxAttrFmt[fmt][attr].frac = frac;
    }
}

PC_NOOP void GXGetVtxAttrFmt(GXVtxFmt fmt, GXAttr attr, GXCompCnt* cnt, GXCompType* type, u8* frac) {
    if (attr == GX_VA_NBT) {
        attr = GX_VA_NRM;
    }
    if ((u32)fmt < GX_MAX_VTXFMT && (u32)attr < GX_VA_MAX_ATTR) {
        *cnt = (GXCompCnt)sVtxAttrFmt[fmt][attr].cnt;
        *type = (GXCompType)sVtxAttrFmt[fmt][attr].type;
        *frac = sVtxAttrFmt[fmt][attr].frac;
    } else {
        *cnt = (GXCompCnt)0;
        *type = (GXCompType)0;
        *frac = 0;
    }
}

PC_NOOP void GXInitFogAdjTable(GXFogAdjTable* table, u16 width, const f32 projmtx[4][4]) {
    f32 nearZ, sideX;

    if (0.0 == projmtx[3][3]) {
        nearZ = projmtx[2][3] / (projmtx[2][2] - 1.0f);
        sideX = nearZ / projmtx[0][0];
    } else {
        sideX = 1.0f / projmtx[0][0];
        nearZ = 1.73205f * sideX;
    }

    f32 iw = 2.0f / width;
    for (u32 i = 0; i < 10; i++) {
        f32 xi = (i + 1) << 5;
        xi *= iw;
        xi *= sideX;
        f32 rangeVal = sqrtf(1.0f + ((xi * xi) / (nearZ * nearZ)));
        table->r[i] = (u32)(256.0f * rangeVal) & 0xFFF;
    }
}

// Lights (src/revolution/GX/GXLight.c)

PC_NOOP void GXInitLightAttn(GXLightObj* lt_obj, f32 a0, f32 a1, f32 a2, f32 k0, f32 k1, f32 k2) {
    PCGXLightObj* obj = Light(lt_obj);
    obj->a[0] = a0;
    obj->a[1] = a1;
    obj->a[2] = a2;
    obj->k[0] = k0;
    obj->k[1] = k1;
    obj->k[2] = k2;
}

PC_NOOP void GXInitLightAttnA(GXLightObj* lt_obj, f32 a0, f32 a1, f32 a2) {
    PCGXLightObj* obj = Light(lt_obj);
    obj->a[0] = a0;
    obj->a[1] = a1;
    obj->a[2] = a2;
}

PC_NOOP void GXInitLightAttnK(GXLightObj* lt_obj, f32 k0, f32 k1, f32 k2) {
    PCGXLightObj* obj = Light(lt_obj);
    obj->k[0] = k0;
    obj->k[1] = k1;
    obj->k[2] = k2;
}

PC_NOOP void GXInitLightSpot(GXLightObj* lt_obj, f32 cutoff, GXSpotFn spot_func) {
    const f32 kPi = 3.14159265358979323846f;
    f32 r, d, cr, a0, a1, a2;
    PCGXLightObj* obj = Light(lt_obj);

    if (cutoff <= 0.0f || cutoff > 90.0f) {
        spot_func = GX_SP_OFF;
    }

    r = cutoff * kPi / 180.0f;
    cr = cos(r);

    switch (spot_func) {
    case GX_SP_FLAT:
        a0 = -1000.0f * cr;
        a1 = 1000.0f;
        a2 = 0.0f;
        break;
    case GX_SP_COS:
        a1 = 1.0f / (1.0f - cr);
        a0 = -cr * a1;
        a2 = 0.0f;
        break;
    case GX_SP_COS2:
        a2 = 1.0f / (1.0f - cr);
        a0 = 0.0F;
        a1 = -cr * a2;
        break;
    case GX_SP_SHARP:
        d = 1.0f / ((1.0f - cr) * (1.0f - cr));
        a0 = cr * (cr - 2.0f) * d;
        a1 = 2.0f * d;
        a2 = -d;
        break;
    case GX_SP_RING1:
        d = 1.0f / ((1.0f - cr) * (1.0f - cr));
        a2 = -4.0f * d;
        a0 = a2 * cr;
        a1 = 4.0f * (1.0f + cr) * d;
        break;
    case GX_SP_RING2:
        d = 1.0f / ((1.0f - cr) * (1.0f - cr));
        a0 = 1.0f - 2.0f * cr * cr * d;
        a1 = 4.0f * cr * d;
        a2 = -2.0f * d;
        break;
    case GX_SP_OFF:
    default:
        a0 = 1.0f;
        a1 = 0.0f;
        a2 = 0.0f;
        break;
    }

    obj->a[0] = a0;
    obj->a[1] = a1;
    obj->a[2] = a2;
}

PC_NOOP void GXInitLightDistAttn(GXLightObj* lt_obj, f32 ref_dist, f32 ref_br, GXDistAttnFn dist_func) {
    f32 k0, k1, k2;
    PCGXLightObj* obj = Light(lt_obj);

    if (ref_dist < 0.0f) {
        dist_func = GX_DA_OFF;
    }
    if (ref_br <= 0.0f || ref_br >= 1.0f) {
        dist_func = GX_DA_OFF;
    }

    switch (dist_func) {
    case GX_DA_GENTLE:
        k0 = 1.0f;
        k1 = (1.0f - ref_br) / (ref_br * ref_dist);
        k2 = 0.0f;
        break;
    case GX_DA_MEDIUM:
        k0 = 1.0f;
        k1 = 0.5f * (1.0f - ref_br) / (ref_br * ref_dist);
        k2 = 0.5f * (1.0f - ref_br) / (ref_br * ref_dist * ref_dist);
        break;
    case GX_DA_STEEP:
        k0 = 1.0f;
        k1 = 0.0f;
        k2 = (1.0f - ref_br) / (ref_br * ref_dist * ref_dist);
        break;
    case GX_DA_OFF:
    default:
        k0 = 1.0f;
        k1 = 0.0f;
        k2 = 0.0f;
        break;
    }

    obj->k[0] = k0;
    obj->k[1] = k1;
    obj->k[2] = k2;
}

PC_NOOP void GXInitLightPos(GXLightObj* lt_obj, f32 x, f32 y, f32 z) {
    PCGXLightObj* obj = Light(lt_obj);
    obj->pos[0] = x;
    obj->pos[1] = y;
    obj->pos[2] = z;
}

PC_NOOP void GXGetLightPos(const GXLightObj* lt_obj, f32* x, f32* y, f32* z) {
    const PCGXLightObj* obj = Light(lt_obj);
    *x = obj->pos[0];
    *y = obj->pos[1];
    *z = obj->pos[2];
}

PC_NOOP void GXInitLightDir(GXLightObj* lt_obj, f32 nx, f32 ny, f32 nz) {
    PCGXLightObj* obj = Light(lt_obj);
    obj->dir[0] = -nx;
    obj->dir[1] = -ny;
    obj->dir[2] = -nz;
}

PC_NOOP void GXGetLightDir(const GXLightObj* lt_obj, f32* nx, f32* ny, f32* nz) {
    const PCGXLightObj* obj = Light(lt_obj);
    *nx = -obj->dir[0];
    *ny = -obj->dir[1];
    *nz = -obj->dir[2];
}

PC_NOOP void GXInitSpecularDir(GXLightObj* lt_obj, f32 nx, f32 ny, f32 nz) {
    const f32 kBigNumber = 1.0E+18f;
    PCGXLightObj* obj = Light(lt_obj);
    f32 vx = -nx;
    f32 vy = -ny;
    f32 vz = (-nz + 1.0f);
    f32 mag = vx * vx + vy * vy + vz * vz;

    if (mag != 0.0f) {
        mag = 1.0f / (f32)sqrt(mag);
    }

    obj->dir[0] = vx * mag;
    obj->dir[1] = vy * mag;
    obj->dir[2] = vz * mag;
    obj->pos[0] = nx * -kBigNumber;
    obj->pos[1] = ny * -kBigNumber;
    obj->pos[2] = nz * -kBigNumber;
}

PC_NOOP void GXInitLightColor(GXLightObj* lt_obj, GXColor color) {
    std::memcpy(&Light(lt_obj)->color, &color, sizeof(color));
}

// Texture objects (src/revolution/GX/GXTexture.c)

PC_NOOP void GXInitTexObj(GXTexObj* obj, void* image_ptr, u16 width, u16 height, GXTexFmt format, GXTexWrapMode wrap_s,
                          GXTexWrapMode wrap_t, GXBool mipmap) {
    PCGXTexObj* t = Tex(obj);
    std::memset(t, 0, sizeof(*t));

    t->image = image_ptr;
    t->width = width;
    t->height = height;
    t->format = format;
    t->wrapS = (u8)wrap_s;
    t->wrapT = (u8)wrap_t;
    t->magFilter = GX_LINEAR;

    if (mipmap) {
        t->flags |= PC_GX_TEX_MIPMAP;
        t->minFilter = IsColorIndex(format) ? GX_LIN_MIP_NEAR : GX_LIN_MIP_LIN;

        u32 size = width > height ? width : height;
        u32 maxLod = 0;
        while ((size >>= 1) != 0) {
            maxLod++;
        }
        t->maxLod = (u8)(maxLod * 16.0f);
    } else {
        t->minFilter = GX_LINEAR;
    }
}

PC_NOOP void GXInitTexObjCI(GXTexObj* obj, void* image_ptr, u16 width, u16 height, GXCITexFmt format,
                            GXTexWrapMode wrap_s, GXTexWrapMode wrap_t, GXBool mipmap, u32 tlut_name) {
    GXInitTexObj(obj, image_ptr, width, height, (GXTexFmt)format, wrap_s, wrap_t, mipmap);
    Tex(obj)->flags |= PC_GX_TEX_COLOR_INDEX;
    Tex(obj)->tlutName = tlut_name;
}

PC_NOOP void GXInitTexObjLOD(GXTexObj* obj, GXTexFilter min_filt, GXTexFilter mag_filt, f32 min_lod, f32 max_lod,
                             f32 lod_bias, GXBool bias_clamp, GXBool do_edge_lod, GXAnisotropy max_aniso) {
    PCGXTexObj* t = Tex(obj);

    if (lod_bias < -4.0f) {
        lod_bias = -4.0f;
    } else if (lod_bias >= 4.0f) {
        lod_bias = 3.99f;
    }
    t->lodBias = (s8)(lod_bias * 32.0f);

    t->magFilter = (mag_filt == GX_LINEAR) ? GX_LINEAR : GX_NEAR;
    t->minFilter = (u8)min_filt;
    t->maxAniso = (u8)max_aniso;
    t->flags &= ~(PC_GX_TEX_BIAS_CLAMP | PC_GX_TEX_EDGE_LOD);
    if (bias_clamp) {
        t->flags |= PC_GX_TEX_BIAS_CLAMP;
    }
    if (do_edge_lod) {
        t->flags |= PC_GX_TEX_EDGE_LOD;
    }

    if (min_lod < 0) {
        min_lod = 0;
    } else if (min_lod > 10.0f) {
        min_lod = 10.0f;
    }
    t->minLod = (u8)(min_lod * 16.0f);

    if (max_lod < 0) {
        max_lod = 0;
    } else if (max_lod > 10.0f) {
        max_lod = 10.0f;
    }
    t->maxLod = (u8)(max_lod * 16.0f);
}

PC_NOOP void GXInitTexObjWrapMode(GXTexObj* obj, GXTexWrapMode sm, GXTexWrapMode tm) {
    Tex(obj)->wrapS = (u8)sm;
    Tex(obj)->wrapT = (u8)tm;
}

PC_NOOP void GXInitTexObjTlut(GXTexObj* obj, u32 tlut_name) {
    Tex(obj)->tlutName = tlut_name;
}

PC_NOOP void GXInitTexObjUserData(GXTexObj* obj, void* user_data) {
    Tex(obj)->userData = user_data;
}

PC_NOOP void* GXGetTexObjUserData(const GXTexObj* obj) {
    return Tex(obj)->userData;
}

PC_NOOP u16 GXGetTexObjWidth(const GXTexObj* obj) {
    return Tex(obj)->width;
}

PC_NOOP u16 GXGetTexObjHeight(const GXTexObj* obj) {
    return Tex(obj)->height;
}

PC_NOOP GXTexFmt GXGetTexObjFmt(const GXTexObj* obj) {
    return (GXTexFmt)Tex(obj)->format;
}

PC_NOOP GXTexWrapMode GXGetTexObjWrapS(const GXTexObj* obj) {
    return (GXTexWrapMode)Tex(obj)->wrapS;
}

PC_NOOP GXTexWrapMode GXGetTexObjWrapT(const GXTexObj* obj) {
    return (GXTexWrapMode)Tex(obj)->wrapT;
}

PC_NOOP void GXGetTexObjLODAll(const GXTexObj* obj, GXTexFilter* min_filt, GXTexFilter* mag_filt, f32* min_lod,
                               f32* max_lod, f32* lod_bias, GXBool* bias_clamp, GXBool* do_edge_lod,
                               GXAnisotropy* max_aniso) {
    const PCGXTexObj* t = Tex(obj);
    *min_filt = (GXTexFilter)t->minFilter;
    *mag_filt = (GXTexFilter)t->magFilter;
    *min_lod = t->minLod / 16.0f;
    *max_lod = t->maxLod / 16.0f;
    *lod_bias = (f32)t->lodBias / 32.0f;
    *bias_clamp = (GXBool)((t->flags & PC_GX_TEX_BIAS_CLAMP) != 0);
    *do_edge_lod = (GXBool)((t->flags & PC_GX_TEX_EDGE_LOD) != 0);
    *max_aniso = (GXAnisotropy)t->maxAniso;
}

PC_NOOP void GXInitTlutObj(GXTlutObj* tlut_obj, void* lut, GXTlutFmt fmt, u16 n_entries) {
    PCGXTlutObj* t = reinterpret_cast<PCGXTlutObj*>(tlut_obj);
    t->data = lut;
    t->format = fmt;
    t->numEntries = n_entries;
    t->pad = 0;
}

// --- Part 2: state and drawing: accepted and ignored -------------------------

PC_NOOP void GXBegin(GXPrimitive, GXVtxFmt, u16) {}
PC_NOOP void GXCallDisplayList(const void *, u32) {}
PC_NOOP void GXCopyDisp(void*, GXBool) {}
PC_NOOP void GXCopyTex(void*, GXBool) {}
PC_NOOP void GXDrawCube(void) {}
PC_NOOP void GXDrawCylinder(u8 sides) {}
PC_NOOP void GXDrawDone(void) {}
PC_NOOP void GXDrawSphere(u32 stacks, u32 sectors) {}
PC_NOOP void GXDrawTorus(f32 rc, u8 numc, u8 numt) {}
PC_NOOP void GXEnableTexOffsets(GXTexCoordID, GXBool, GXBool) {}
PC_NOOP void GXInvalidateTexAll(void) {}
PC_NOOP void GXInvalidateVtxCache(void) {}
PC_NOOP void GXLoadLightObjImm(const GXLightObj*, GXLightID) {}
PC_NOOP void GXLoadNrmMtxImm(const f32 mtx[3][4], u32 id) {}
PC_NOOP void GXLoadNrmMtxIndx3x3(u16 mtx_indx, u32 id) {}
PC_NOOP void GXLoadPosMtxImm(const f32 mtx[3][4], u32 id) {}
PC_NOOP void GXLoadPosMtxIndx(u16 mtx_indx, u32 id) {}
PC_NOOP void GXLoadTexMtxImm(const f32 mtx[][4], u32 id, GXTexMtxType type) {}
PC_NOOP void GXLoadTexObj(const GXTexObj *, GXTexMapID) {}
PC_NOOP void GXLoadTlut(const GXTlutObj *, u32) {}
PC_NOOP void GXPixModeSync(void) {}
PC_NOOP void GXSetAlphaCompare(GXCompare comp0, u8 ref0, GXAlphaOp logic, GXCompare comp1, u8 ref1) {}
PC_NOOP void GXSetAlphaUpdate(GXBool) {}
PC_NOOP void GXSetArray(GXAttr attr, const void* pBase, u8 stride) {}
PC_NOOP void GXSetBlendMode(GXBlendMode mode, GXBlendFactor srcFactor, GXBlendFactor dstFactor, GXLogicOp logic) {}
PC_NOOP void GXSetChanAmbColor(GXChannelID id, GXColor color) {}
PC_NOOP void GXSetChanCtrl(GXChannelID id, GXBool enable, GXColorSrc ambSrc, GXColorSrc matSrc, u32 lightMask, GXDiffuseFn diffuseFn, GXAttnFn attnFn) {}
PC_NOOP void GXSetChanMatColor(GXChannelID id, GXColor color) {}
PC_NOOP void GXSetClipMode(GXClipMode) {}
PC_NOOP void GXSetCoPlanar(GXBool) {}
PC_NOOP void GXSetColorUpdate(GXBool) {}
PC_NOOP void GXSetCopyClear(GXColor, u32) {}
PC_NOOP void GXSetCopyFilter(GXBool, const u8[12][2], GXBool, const u8[7]) {}
PC_NOOP void GXSetCullMode(GXCullMode mode) {}
PC_NOOP void GXSetCurrentMtx(u32 id) {}
PC_NOOP void GXSetDispCopyDst(u16, u16) {}
PC_NOOP void GXSetDispCopyGamma(GXGamma) {}
PC_NOOP void GXSetFog(GXFogType, f32, f32, f32, f32, GXColor) {}
PC_NOOP void GXSetFogRangeAdj(GXBool, u16, const GXFogAdjTable *) {}
PC_NOOP void GXSetIndTexCoordScale(GXIndTexStageID, GXIndTexScale, GXIndTexScale) {}
PC_NOOP void GXSetIndTexMtx(GXIndTexMtxID id, const f32 mtx[2][3], s8 scaleExp) {}
PC_NOOP void GXSetIndTexOrder(GXIndTexStageID, GXTexCoordID, GXTexMapID) {}
PC_NOOP void GXSetLineWidth(u8, GXTexOffset) {}
PC_NOOP void GXSetNumChans(u8 num) {}
PC_NOOP void GXSetNumIndStages(u8 num) {}
PC_NOOP void GXSetNumTevStages(u8 num) {}
PC_NOOP void GXSetNumTexGens(u8 num) {}
PC_NOOP void GXSetPixelFmt(GXPixelFmt, GXZFmt16) {}
PC_NOOP void GXSetPointSize(u8, GXTexOffset) {}
PC_NOOP void GXSetProjection(const f32 mtx[4][4], GXProjectionType type) {}
PC_NOOP void GXSetScissor(u32, u32, u32, u32) {}
PC_NOOP void GXSetScissorBoxOffset(s32, s32) {}
PC_NOOP void GXSetTevAlphaIn(GXTevStageID, GXTevAlphaArg, GXTevAlphaArg, GXTevAlphaArg, GXTevAlphaArg) {}
PC_NOOP void GXSetTevAlphaOp(GXTevStageID, GXTevOp, GXTevBias, GXTevScale, GXBool, GXTevRegID) {}
PC_NOOP void GXSetTevColor(GXTevRegID id, GXColor color) {}
PC_NOOP void GXSetTevColorIn(GXTevStageID stage, GXTevColorArg a, GXTevColorArg b, GXTevColorArg c, GXTevColorArg d) {}
PC_NOOP void GXSetTevColorOp(GXTevStageID, GXTevOp, GXTevBias, GXTevScale, GXBool, GXTevRegID) {}
PC_NOOP void GXSetTevColorS10(GXTevRegID id, GXColorS10 color) {}
PC_NOOP void GXSetTevDirect(GXTevStageID) {}
PC_NOOP void GXSetTevIndirect(GXTevStageID, GXIndTexStageID, GXIndTexFormat, GXIndTexBiasSel, GXIndTexMtxID, GXIndTexWrap, GXIndTexWrap, GXBool, GXBool, GXIndTexAlphaSel) {}
PC_NOOP void GXSetTevKAlphaSel(GXTevStageID, GXTevKAlphaSel) {}
PC_NOOP void GXSetTevKColor(GXTevKColorID id, GXColor color) {}
PC_NOOP void GXSetTevKColorSel(GXTevStageID, GXTevKColorSel) {}
PC_NOOP void GXSetTevOp(GXTevStageID, GXTevMode) {}
PC_NOOP void GXSetTevOrder(GXTevStageID, GXTexCoordID, GXTexMapID, GXChannelID) {}
PC_NOOP void GXSetTevSwapMode(GXTevStageID, GXTevSwapSel, GXTevSwapSel) {}
PC_NOOP void GXSetTevSwapModeTable(GXTevSwapSel swap, GXTevColorChan r, GXTevColorChan g, GXTevColorChan b, GXTevColorChan a) {}
PC_NOOP void GXSetTexCoordGen2(GXTexCoordID id, GXTexGenType func, GXTexGenSrc param, u32 mtx, GXBool normalize, u32 postMtx) {}
PC_NOOP void GXSetTexCopyDst(u16, u16, GXTexFmt, GXBool) {}
PC_NOOP void GXSetTexCopySrc(u16, u16, u16, u16) {}
PC_NOOP void GXSetViewport(f32, f32, f32, f32, f32, f32) {}
PC_NOOP void GXSetViewportJitter(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz, u32 field) {}
PC_NOOP void GXSetZCompLoc(GXBool) {}
PC_NOOP void GXSetZMode(GXBool test, GXCompare compare, GXBool update) {}
PC_NOOP void GXSetZScaleOffset(f32, f32) {}
PC_NOOP void GXSetZTexture(GXZTexOp, GXTexFmt, u32) {}

} // extern "C"
