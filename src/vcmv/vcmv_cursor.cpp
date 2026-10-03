#include "vcmv/vcmv.h"

#include <revolution/arc.h>
#include <revolution/gx.h>
#include <revolution/hbm.h>
#include <revolution/tpl.h>
#include <string.h>

// vcmv_cursor.cpp: the manual viewer's sound effects (played through the HOME
// Menu's sound archive) and its Wii Remote pointer cursors, whose textures
// come from the HOME Menu layout archive.

typedef struct vcmvCursorStyle {
    f32 width;            // 0x00
    f32 height;           // 0x04
    f32 hotX;             // 0x08
    f32 hotY;             // 0x0C
    TPLPalettePtr tpl;    // 0x10
    GXColor color;        // 0x14
} vcmvCursorStyle;

static vcmvCursorStyle sCursorStyles[4] = {
    {50.0f, 50.0f, 0.38f, 0.92f, NULL, {0, 140, 255, 255}},
    {50.0f, 50.0f, 0.38f, 0.92f, NULL, {255, 56, 56, 255}},
    {50.0f, 50.0f, 0.38f, 0.92f, NULL, {16, 189, 13, 255}},
    {50.0f, 50.0f, 0.38f, 0.92f, NULL, {255, 156, 0, 255}},
};

static s32 sLastSound[2] = {-1, 0};

static s32 sLastSoundFrame;
// Three words nothing references, which the original link kept (force_active
// in config.yml)
s32 vcmvCursorUnused1;
s32 vcmvCursorUnused2;
s32 vcmvCursorUnused3;
static TPLPalettePtr sShadowTPL;
s32 vcmvCursorSwitchTimer;

void vcmvPlaySound(s32 id) {
    if (sLastSound[0] == id && vcmvFrame - sLastSoundFrame < 3) {
        return;
    }
    sLastSound[0] = id;
    sLastSoundFrame = vcmvFrame;

    switch (id) {
    case 0: {
        volatile vcmvCursor* c = &vcmvCursors[vcmvCurChan];
        if (!vcmvRumbling && c->pointing && vcmvFrame - vcmvRumbleStart >= 15) {
            vcmvRumbleRequest = TRUE;
        }
        HBMPlaySound(0x17);
        break;
    }
    case 1:
        HBMPlaySound(0x18);
        break;
    case 2:
        HBMPlaySound(0x16);
        break;
    case 3:
        HBMPlaySound(0x1B);
        break;
    case 5:
        HBMPlaySound(0x1A);
        break;
    }
}

void vcmvCursorInit(void) {}

static inline void vcmvLoadArcFile(ARCHandle handle, const char* name, TPLPalettePtr* out) {
    ARCFileInfo file;

    ARCOpen(&handle, name, &file);
    *out = (TPLPalettePtr)ARCGetStartAddrInMem(&file);
    ARCClose(&file);
}

void vcmvLoadCursorTextures(HBMDataInfo* info) {
    char path[40] = "arc/timg/defcursor_final";
    ARCHandle handle;
    char* name;
    s32 i;

    name = path + strlen(path);
    ARCInitHandle(info->layoutBuf, &handle);

    strcpy(name, "64_a.tpl");
    vcmvLoadArcFile(handle, path, &sShadowTPL);
    TPLBind(sShadowTPL);

    strcpy(name, "_p1.tpl");
    for (i = 0; i < 4; i++) {
        name[2] = '1' + i;
        vcmvLoadArcFile(handle, path, &sCursorStyles[i].tpl);
        TPLBind(sCursorStyles[i].tpl);
    }
}

void vcmvDrawCursor(s32 chan) {
    volatile vcmvCursor* c = &vcmvCursors[chan];
    vcmvCursorStyle* style;
    f32 x, y, hx, hy, w, h;
    u8 alpha;
    GXTexObj texObj;
    vcmvQuad quad;

    if (c->active == 0) {
        return;
    }

    style = &sCursorStyles[chan];
    x = c->drawX;
    y = -c->drawY;
    hx = c->horizonX;
    hy = c->horizonY;
    w = style->width;
    h = style->height;

    if (chan == vcmvCurChan) {
        if ((u32)c->active >= 576) {
            alpha = 192;
        } else {
            alpha = (u32)c->active / 3;
        }
        if (vcmvCursorSwitchTimer != 0) {
            f32 scale;
            if (vcmvCursorSwitchTimer > 10) {
                scale = 0.02f * (20 - vcmvCursorSwitchTimer) + 1.0f;
            } else {
                scale = 0.02f * vcmvCursorSwitchTimer + 1.0f;
            }
            w *= scale;
            h *= scale;
            vcmvCursorSwitchTimer--;
        }
    } else {
        if ((u32)c->active >= 256) {
            alpha = 64;
        } else {
            alpha = (u32)c->active / 4;
        }
    }

    {
        GXColor shadow = {224, 224, 224, alpha};

        quad.x[1] = x - style->hotX * (hx * w) - style->hotY * (hy * h);
        quad.x[0] = quad.x[1] + hy * h;
        quad.x[3] = quad.x[0] + hx * w;
        quad.x[2] = quad.x[3] - hy * h;
        quad.y[1] = y + style->hotX * (hy * w) - style->hotY * (hx * h);
        quad.y[0] = quad.y[1] + hx * h;
        quad.y[3] = quad.y[0] - hy * w;
        quad.y[2] = quad.y[3] - hx * h;

        TPLGetGXTexObjFromPalette(sShadowTPL, &texObj, 0);
        GXLoadTexObj(&texObj, GX_TEXMAP1);
        TPLGetGXTexObjFromPalette(style->tpl, &texObj, 0);
        GXLoadTexObj(&texObj, GX_TEXMAP0);
        {
            GXColor white = {255, 255, 255, 255};
            GXSetTevColor(GX_TEVREG0, style->color);
            GXSetTevColor(GX_TEVREG1, white);
        }
        GXSetTevColor(GX_TEVREG2, shadow);
    }

    GXClearVtxDesc();
    GXInvalidateVtxCache();
    GXInvalidateTexAll();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GXSetNumChans(0);
    GXSetNumTexGens(1);
    GXSetNumIndStages(0);
    GXSetNumTevStages(2);

    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C0, GX_CC_C1, GX_CC_TEXC, GX_CC_ZERO);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE, GX_PTIDENTITY);

    GXSetTevDirect(GX_TEVSTAGE1);
    GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_CPREV, GX_CC_TEXC, GX_CC_ZERO);
    GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_A2, GX_CA_TEXA, GX_CA_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP1, GX_COLOR_NULL);

    GXSetColorUpdate(GX_TRUE);
    GXSetAlphaUpdate(GX_FALSE);
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
    GXSetCullMode(GX_CULL_NONE);
    GXSetClipMode(GX_CLIP_ENABLE);
    vcmvDrawQuad(&quad);
}
