#include <string.h>
#include "jpgd_internal.h"

s32 fn_80083C60(JPEGDecContext* ctx) {
    JPEGPixelBuffer* pix = &ctx->pix;
    JPEGDecHandle* h = ctx->handle;

    switch (ctx->frame.sampling) {
    case 0:
        ctx->output = fn_80083F14;
        ctx->outputEdge = fn_800844A4;
        ctx->blockOut[0] = pix->y;
        ctx->blockOut[1] = ctx->blockOut[0] + ctx->blockSize;
        ctx->blockOut[2] = ctx->blockOut[1] + ctx->blockSize;
        ctx->blockOut[3] = ctx->blockOut[2] + ctx->blockSize;
        ctx->blockOut[5] = pix->cb;
        ctx->blockOut[6] = pix->cr;
        ctx->stride = 32;
        ctx->unk19E0 = 0;
        break;
    case 1:
        ctx->output = fn_80084AC0;
        ctx->outputEdge = fn_8008522C;
        ctx->blockOut[0] = pix->y;
        ctx->blockOut[1] = ctx->blockOut[0] + ctx->blockSize;
        ctx->blockOut[5] = pix->y + 0x80;
        ctx->blockOut[6] = pix->y + 0xC0;
        ctx->stride = 16;
        ctx->unk19E0 = 0;
        break;
    case 2:
        ctx->output = fn_80085848;
        ctx->outputEdge = fn_80085FB0;
        ctx->blockOut[0] = pix->y;
        ctx->blockOut[1] = ctx->blockOut[0] + ctx->blockSize;
        ctx->blockOut[2] = ctx->blockOut[0] + ctx->blockSize * 16;
        ctx->blockOut[3] = ctx->blockOut[2] + ctx->blockSize;
        ctx->blockOut[5] = pix->cb;
        ctx->blockOut[6] = pix->cr;
        ctx->stride = 16;
        ctx->unk19E0 = 0;
        break;
    case 3:
        ctx->output = fn_800865D8;
        ctx->outputEdge = fn_80086764;
        ctx->blockOut[0] = pix->y;
        ctx->blockOut[5] = pix->y + 0x40;
        ctx->blockOut[6] = pix->y + 0x80;
        ctx->stride = 8;
        ctx->unk19E0 = 0;
        break;
    case 4:
        ctx->output = fn_80086924;
        ctx->outputEdge = fn_80086BAC;
        ctx->blockOut[0] = pix->y;
        ctx->stride = 8;
        ctx->unk19E0 = 0;
        break;
    default:
        return -0x70;
    }

    {
        u16 w = h->width;
        u16 hg = h->height;
        h->strideY = (w / 8 + (w % 8 != 0)) * 8;
        h->heightY = (hg / 4 + (hg % 4 != 0)) * 4;
    }
    {
        u16 w = h->cWidth;
        u16 hg = h->cHeight;
        h->strideC = (w / 8 + (w % 8 != 0)) * 8;
        h->heightC = (hg / 4 + (hg % 4 != 0)) * 4;
    }
    return 0;
}

#define PUT_I8(p, x, y, tiles, v)                       \
    t = ((x) >> 3) + ((y) >> 2) * (tiles);              \
    (p)[(t << 5) + (((y) & 3) << 3) + ((x) & 7)] = (v)

void fn_80083F14(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 t;
    u8* scr = ctx->pix.cr;
    s32 j;
    u32 cx;
    u8* scb = ctx->pix.cb;
    JPEGDecHandle* h = ctx->handle;
    u8* py = h->planeY;
    s32 i;
    s32 cw;
    u8* pcb = h->planeCb;
    u8* sy = ctx->pix.y;
    u32 ctiles;
    s32 hh = 8 / h->scale;
    s32 w = 32 / h->scale;
    u32 tiles = h->strideY >> 3;
    u8* pcr = h->planeCr;

    for (j = y; j < (s32)(y + hh); j++) {
        for (i = x; i < (s32)(x + w); i += 4) {
            PUT_I8(py, i, j, tiles, sy[0]);
            PUT_I8(py, i + 1, j, tiles, sy[1]);
            PUT_I8(py, i + 2, j, tiles, sy[2]);
            PUT_I8(py, i + 3, j, tiles, sy[3]);
            sy += 4;
        }
        sy += 32 - w;
    }

    cw = 8 / h->scale;
    ctiles = h->strideC >> 3;
    cx = x >> 2;
    for (j = y; j < (s32)(y + cw); j++) {
        for (i = cx; i < (s32)(cx + cw); i++) {
            t = (i >> 3) + (j >> 2) * ctiles;
            pcb[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scb++ + 0x80;
            pcr[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scr++ + 0x80;
        }
        scb += 8 - cw;
        scr += 8 - cw;
    }
}
