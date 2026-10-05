#include <news/Draw2D.h>
#include <news/Common.h>
#include <revolution/gx.h>

using namespace nw4r;

void Draw2D_TexPos(TPLPalette* tpl, u32 index, const math::VEC3* pos, f32 scaleX, f32 scaleY,
                   u8 flags) {
    GXTexObj texObj;
    TPL_GetTexObj(tpl, index, &texObj);
    GXLoadTexObj(&texObj, GX_TEXMAP0);

    f32 x0 = pos->x;
    f32 y0 = pos->y;
    f32 x1 = pos->x + scaleX * TPL_GetWidth(tpl, index);
    f32 y1 = pos->y + scaleY * TPL_GetHeight(tpl, index);

    f32 s0, s1, t0, t1;
    if (flags & 1) {
        s0 = 1.0f;
        s1 = 0.0f;
    } else {
        s0 = 0.0f;
        s1 = 1.0f;
    }
    if (flags & 2) {
        t0 = 1.0f;
        t1 = 0.0f;
    } else {
        t0 = 0.0f;
        t1 = 1.0f;
    }

    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(x0, y0, pos->z);
    GXTexCoord2f32(s0, t0);
    GXPosition3f32(x1, y0, pos->z);
    GXTexCoord2f32(s1, t0);
    GXPosition3f32(x1, y1, pos->z);
    GXTexCoord2f32(s1, t1);
    GXPosition3f32(x0, y1, pos->z);
    GXTexCoord2f32(s0, t1);
    GXEnd();
}

void Draw2D_TexRect(TPLPalette* tpl, u32 index, const ut::Rect* rect, f32 z, u32 flags) {
    GXTexObj texObj;
    TPL_GetTexObj(tpl, index, &texObj);
    GXLoadTexObj(&texObj, GX_TEXMAP0);

    f32 x0 = rect->left;
    f32 y0 = rect->top;
    f32 x1 = rect->right;
    f32 y1 = rect->bottom;

    f32 s0, s1, t0, t1;
    if (flags & 1) {
        s0 = 1.0f;
        s1 = 0.0f;
    } else {
        s0 = 0.0f;
        s1 = 1.0f;
    }
    if (flags & 2) {
        t0 = 1.0f;
        t1 = 0.0f;
    } else {
        t0 = 0.0f;
        t1 = 1.0f;
    }

    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(x0, y0, z);
    GXTexCoord2f32(s0, t0);
    GXPosition3f32(x1, y0, z);
    GXTexCoord2f32(s1, t0);
    GXPosition3f32(x1, y1, z);
    GXTexCoord2f32(s1, t1);
    GXPosition3f32(x0, y1, z);
    GXTexCoord2f32(s0, t1);
    GXEnd();
}

void Draw2D_TexRectTiled(TPLPalette* tpl, u32 index, const ut::Rect* rect, f32 z, u32 flags) {
    GXTexObj texObj;
    TPL_GetTexObj(tpl, index, &texObj);
    GXLoadTexObj(&texObj, GX_TEXMAP0);

    f32 x0 = rect->left;
    f32 y0 = rect->top;
    f32 x1 = rect->right;
    f32 y1 = rect->bottom;

    f32 s0, s1, t0, t1;
    f32 texWidth = TPL_GetWidth(tpl, index);
    f32 width = __fabsf(rect->GetWidth());
    if (flags & 1) {
        s0 = width / texWidth;
        s1 = 0.0f;
    } else {
        s0 = 0.0f;
        s1 = width / texWidth;
    }
    if (flags & 2) {
        t0 = 1.0f;
        t1 = 0.0f;
    } else {
        t0 = 0.0f;
        t1 = 1.0f;
    }

    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(x0, y0, z);
    GXTexCoord2f32(s0, t0);
    GXPosition3f32(x1, y0, z);
    GXTexCoord2f32(s1, t0);
    GXPosition3f32(x1, y1, z);
    GXTexCoord2f32(s1, t1);
    GXPosition3f32(x0, y1, z);
    GXTexCoord2f32(s0, t1);
    GXEnd();
}

static inline void SetupFillGX() {
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE,
                  GX_AF_NONE);
    GXSetNumChans(1);
    GXSetNumTexGens(0);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetNumTevStages(1);
    GXSetNumIndStages(0);
}

void Draw2D_FillQuad(const math::VEC3* quad, const GXColor* color) {
    SetupFillGX();

    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    for (int i = 0; i < 4; i++) {
        GXPosition3f32(quad[i].x, quad[i].y, quad[i].z);
#ifdef TARGET_PC
        // PC: a colour's bytes are not 0xRRGGBBAA as a u32 on a little-endian
        // host (docs/pc_port.md, "Colours")
        GXColor4u8(color->r, color->g, color->b, color->a);
#else
        GXColor1u32(*(const u32*)color);
#endif
    }
    GXEnd();
}

void Draw2D_FillRect(const ut::Rect* rect, const ut::Color* color) {
    math::VEC3 quad[4] = {
        math::VEC3(rect->left, rect->top, 0.0f),
        math::VEC3(rect->left, rect->bottom, 0.0f),
        math::VEC3(rect->right, rect->bottom, 0.0f),
        math::VEC3(rect->right, rect->top, 0.0f),
    };
    Draw2D_FillQuad(quad, color);
}

void Draw2D_FillQuadGradient(const math::VEC3* quad, const ut::Color* colors) {
    SetupFillGX();

    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    for (int i = 0; i < 4; i++) {
        GXPosition3f32(quad[i].x, quad[i].y, quad[i].z);
#ifdef TARGET_PC
        GXColor4u8(colors[i].r, colors[i].g, colors[i].b, colors[i].a);
#else
        GXColor1u32(*(const u32*)&colors[i]);
#endif
    }
    GXEnd();
}
