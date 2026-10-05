// GX functions that only read or write the application's own objects:
// GXTexObj, GXTlutObj, GXLightObj, GXFogAdjTable. They work as in the SDK
// (src/revolution/GX/GXTexture.c, GXLight.c), because the game and NW4R read
// the results back while they build a frame. The PC layout of the objects is
// in pc_gx_objects.h: plain values instead of register images.

#include <revolution/gx.h>

#include <cmath>
#include <cstring>

#include "pc_gx_objects.h"

namespace {

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

} // namespace

extern "C" {

void GXInitFogAdjTable(GXFogAdjTable* table, u16 width, const f32 projmtx[4][4]) {
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

void GXInitLightAttn(GXLightObj* lt_obj, f32 a0, f32 a1, f32 a2, f32 k0, f32 k1, f32 k2) {
    PCGXLightObj* obj = Light(lt_obj);
    obj->a[0] = a0;
    obj->a[1] = a1;
    obj->a[2] = a2;
    obj->k[0] = k0;
    obj->k[1] = k1;
    obj->k[2] = k2;
}

void GXInitLightAttnA(GXLightObj* lt_obj, f32 a0, f32 a1, f32 a2) {
    PCGXLightObj* obj = Light(lt_obj);
    obj->a[0] = a0;
    obj->a[1] = a1;
    obj->a[2] = a2;
}

void GXInitLightAttnK(GXLightObj* lt_obj, f32 k0, f32 k1, f32 k2) {
    PCGXLightObj* obj = Light(lt_obj);
    obj->k[0] = k0;
    obj->k[1] = k1;
    obj->k[2] = k2;
}

void GXInitLightSpot(GXLightObj* lt_obj, f32 cutoff, GXSpotFn spot_func) {
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

void GXInitLightDistAttn(GXLightObj* lt_obj, f32 ref_dist, f32 ref_br, GXDistAttnFn dist_func) {
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

void GXInitLightPos(GXLightObj* lt_obj, f32 x, f32 y, f32 z) {
    PCGXLightObj* obj = Light(lt_obj);
    obj->pos[0] = x;
    obj->pos[1] = y;
    obj->pos[2] = z;
}

void GXGetLightPos(const GXLightObj* lt_obj, f32* x, f32* y, f32* z) {
    const PCGXLightObj* obj = Light(lt_obj);
    *x = obj->pos[0];
    *y = obj->pos[1];
    *z = obj->pos[2];
}

void GXInitLightDir(GXLightObj* lt_obj, f32 nx, f32 ny, f32 nz) {
    PCGXLightObj* obj = Light(lt_obj);
    obj->dir[0] = -nx;
    obj->dir[1] = -ny;
    obj->dir[2] = -nz;
}

void GXGetLightDir(const GXLightObj* lt_obj, f32* nx, f32* ny, f32* nz) {
    const PCGXLightObj* obj = Light(lt_obj);
    *nx = -obj->dir[0];
    *ny = -obj->dir[1];
    *nz = -obj->dir[2];
}

void GXInitSpecularDir(GXLightObj* lt_obj, f32 nx, f32 ny, f32 nz) {
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

void GXInitLightColor(GXLightObj* lt_obj, GXColor color) {
    std::memcpy(&Light(lt_obj)->color, &color, sizeof(color));
}

// Texture objects (src/revolution/GX/GXTexture.c)

void GXInitTexObj(GXTexObj* obj, void* image_ptr, u16 width, u16 height, GXTexFmt format, GXTexWrapMode wrap_s,
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

void GXInitTexObjCI(GXTexObj* obj, void* image_ptr, u16 width, u16 height, GXCITexFmt format,
                            GXTexWrapMode wrap_s, GXTexWrapMode wrap_t, GXBool mipmap, u32 tlut_name) {
    GXInitTexObj(obj, image_ptr, width, height, (GXTexFmt)format, wrap_s, wrap_t, mipmap);
    Tex(obj)->flags |= PC_GX_TEX_COLOR_INDEX;
    Tex(obj)->tlutName = tlut_name;
}

void GXInitTexObjLOD(GXTexObj* obj, GXTexFilter min_filt, GXTexFilter mag_filt, f32 min_lod, f32 max_lod,
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

void GXInitTexObjWrapMode(GXTexObj* obj, GXTexWrapMode sm, GXTexWrapMode tm) {
    Tex(obj)->wrapS = (u8)sm;
    Tex(obj)->wrapT = (u8)tm;
}

void GXInitTexObjTlut(GXTexObj* obj, u32 tlut_name) {
    Tex(obj)->tlutName = tlut_name;
}

void GXInitTexObjUserData(GXTexObj* obj, void* user_data) {
    Tex(obj)->userData = user_data;
}

void* GXGetTexObjUserData(const GXTexObj* obj) {
    return Tex(obj)->userData;
}

u16 GXGetTexObjWidth(const GXTexObj* obj) {
    return Tex(obj)->width;
}

u16 GXGetTexObjHeight(const GXTexObj* obj) {
    return Tex(obj)->height;
}

GXTexFmt GXGetTexObjFmt(const GXTexObj* obj) {
    return (GXTexFmt)Tex(obj)->format;
}

GXTexWrapMode GXGetTexObjWrapS(const GXTexObj* obj) {
    return (GXTexWrapMode)Tex(obj)->wrapS;
}

GXTexWrapMode GXGetTexObjWrapT(const GXTexObj* obj) {
    return (GXTexWrapMode)Tex(obj)->wrapT;
}

void GXGetTexObjLODAll(const GXTexObj* obj, GXTexFilter* min_filt, GXTexFilter* mag_filt, f32* min_lod,
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

void GXInitTlutObj(GXTlutObj* tlut_obj, void* lut, GXTlutFmt fmt, u16 n_entries) {
    PCGXTlutObj* t = reinterpret_cast<PCGXTlutObj*>(tlut_obj);
    t->data = lut;
    t->format = fmt;
    t->numEntries = n_entries;
    t->pad = 0;
}

u32 GXGetTexObjTlut(const GXTexObj* obj) {
    return Tex(obj)->tlutName;
}

GXBool GXGetTexObjMipMap(const GXTexObj* obj) {
    return (GXBool)((Tex(obj)->flags & PC_GX_TEX_MIPMAP) != 0);
}

void GXGetTexObjAll(const GXTexObj* obj, void** image_ptr, u16* width, u16* height, GXTexFmt* format,
                    GXTexWrapMode* wrap_s, GXTexWrapMode* wrap_t, GXBool* mipmap) {
    const PCGXTexObj* t = Tex(obj);
    *image_ptr = t->image;
    *width = t->width;
    *height = t->height;
    *format = (GXTexFmt)t->format;
    *wrap_s = (GXTexWrapMode)t->wrapS;
    *wrap_t = (GXTexWrapMode)t->wrapT;
    *mipmap = (GXBool)((t->flags & PC_GX_TEX_MIPMAP) != 0);
}

} // extern "C"
