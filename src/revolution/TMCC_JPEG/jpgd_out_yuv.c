#include <string.h>
#include "jpgd_internal.h"

s32 jpgdSetupOutputYUV(JPEGDecContext* ctx) {
    JPEGPixelBuffer* pix = &ctx->pix;
    JPEGDecHandle* h = ctx->handle;

    switch (ctx->frame.sampling) {
    case 0:
        ctx->output = jpgdOutYUV411;
        ctx->outputEdge = jpgdOutYUV411Edge;
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
        ctx->output = jpgdOutYUV422;
        ctx->outputEdge = jpgdOutYUV422Edge;
        ctx->blockOut[0] = pix->y;
        ctx->blockOut[1] = ctx->blockOut[0] + ctx->blockSize;
        ctx->blockOut[5] = pix->y + 0x80;
        ctx->blockOut[6] = pix->y + 0xC0;
        ctx->stride = 16;
        ctx->unk19E0 = 0;
        break;
    case 2:
        ctx->output = jpgdOutYUV420;
        ctx->outputEdge = jpgdOutYUV420Edge;
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
        ctx->output = jpgdOutYUV444;
        ctx->outputEdge = jpgdOutYUV444Edge;
        ctx->blockOut[0] = pix->y;
        ctx->blockOut[5] = pix->y + 0x40;
        ctx->blockOut[6] = pix->y + 0x80;
        ctx->stride = 8;
        ctx->unk19E0 = 0;
        break;
    case 4:
        ctx->output = jpgdOutYUVGray;
        ctx->outputEdge = jpgdOutYUVGrayEdge;
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

void jpgdOutYUV411(JPEGDecContext* ctx, u32 x, u32 y) {
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

void jpgdOutYUV411Edge(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 xe;
    s32 ye;
    JPEGDecHandle* h;
    u8* sy;
    u8* scb;
    u8* scr;
    u32 tiles;
    u8* py;
    u8* pcb;
    u8* pcr;
    s32 w;
    s32 hh;
    s32 i;
    s32 j;
    s32 t;
    s32 cw;
    u32 cx;
    u32 ctiles;

    sy = ctx->pix.y;
    scb = ctx->pix.cb;
    scr = ctx->pix.cr;
    h = ctx->handle;
    tiles = h->strideY >> 3;
    py = h->planeY;
    pcb = h->planeCb;
    pcr = h->planeCr;
    w = (h->lastX == x) ? h->remX : (u8)(32 / h->scale);
    hh = (h->lastY == y) ? h->remY : (u8)(8 / h->scale);

    xe = x + w;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            PUT_I8(py, i, j, tiles, *sy++);
        }
        sy += 32 - w;
    }

    cx = x >> 2;
    cw = (w + 3) >> 2;
    ctiles = h->strideC >> 3;
    for (j = y; j < (s32)(y + hh); j++) {
        for (i = cx; i < (s32)(cx + cw); i++) {
            t = (i >> 3) + (j >> 2) * ctiles;
            pcb[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scb++ + 0x80;
            pcr[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scr++ + 0x80;
        }
        scb += 8 - cw;
        scr += 8 - cw;
    }
}

void jpgdOutYUV422(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 xe;
    s32 ye;
    JPEGDecHandle* h = ctx->handle;
    u8* sy = ctx->pix.y;
    u8* scb = ctx->pix.y + 0x80;
    u8* scr = ctx->pix.y + 0xC0;
    u8 scale = h->scale;
    s32 w = 16 / scale;
    u8* py = h->planeY;
    u8* pcb = h->planeCb;
    u32 tiles = h->strideY >> 3;
    u8* pcr = h->planeCr;
    s32 i;
    s32 j;
    s32 t;
    s32 cw;
    u32 cx;
    u32 ctiles;

    for (j = y; j < (s32)(y + 8 / scale); j++) {
        for (i = x; i < (s32)(x + w); i += 2) {
            PUT_I8(py, i, j, tiles, sy[0]);
            PUT_I8(py, i + 1, j, tiles, sy[1]);
            sy += 2;
        }
        sy += 16 - w;
    }

    cx = x >> 1;
    cw = 8 / h->scale;
    ctiles = h->strideC >> 3;
    xe = cx + cw;
    ye = y + cw;
    for (j = y; j < ye; j++) {
        for (i = cx; i < xe; i++) {
            t = (i >> 3) + (j >> 2) * ctiles;
            pcb[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scb++ + 0x80;
            pcr[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scr++ + 0x80;
        }
        scb += 8 - cw;
        scr += 8 - cw;
    }
}

void jpgdOutYUV422Edge(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 xe;
    s32 ye;
    JPEGDecHandle* h;
    u8* sy;
    u8* scb;
    u8* scr;
    u32 tiles;
    u8* py;
    u8* pcb;
    u8* pcr;
    s32 w;
    s32 hh;
    s32 i;
    s32 j;
    s32 t;
    s32 cw;
    u32 cx;
    u32 ctiles;

    sy = ctx->pix.y;
    scb = ctx->pix.y + 0x80;
    scr = ctx->pix.y + 0xC0;
    h = ctx->handle;
    tiles = h->strideY >> 3;
    py = h->planeY;
    pcb = h->planeCb;
    pcr = h->planeCr;
    w = (h->lastX == x) ? h->remX : (u8)(16 / h->scale);
    hh = (h->lastY == y) ? h->remY : (u8)(8 / h->scale);

    xe = x + w;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            PUT_I8(py, i, j, tiles, *sy++);
        }
        sy += 16 - w;
    }

    cx = x >> 1;
    cw = (w + 1) >> 1;
    ctiles = h->strideC >> 3;
    for (j = y; j < (s32)(y + hh); j++) {
        for (i = cx; i < (s32)(cx + cw); i++) {
            t = (i >> 3) + (j >> 2) * ctiles;
            pcb[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scb++ + 0x80;
            pcr[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scr++ + 0x80;
        }
        scb += 8 - cw;
        scr += 8 - cw;
    }
}

void jpgdOutYUV420(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 xe;
    s32 ye;
    JPEGDecHandle* h = ctx->handle;
    u8* sy = ctx->pix.y;
    u8* scb = ctx->pix.cb;
    u8* scr = ctx->pix.cr;
    s32 w = 16 / h->scale;
    u8* py = h->planeY;
    u8* pcb = h->planeCb;
    u32 tiles = h->strideY >> 3;
    u8* pcr = h->planeCr;
    s32 i;
    s32 j;
    s32 t;
    s32 cw;
    u32 cx;
    u32 cy;
    u32 ctiles;

    xe = x + w;
    ye = y + w;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i += 2) {
            PUT_I8(py, i, j, tiles, sy[0]);
            PUT_I8(py, i + 1, j, tiles, sy[1]);
            sy += 2;
        }
        sy += 16 - w;
    }

    cx = x >> 1;
    cy = y >> 1;
    cw = 8 / h->scale;
    ctiles = h->strideC >> 3;
    for (j = cy; j < (s32)(cy + cw); j++) {
        for (i = cx; i < (s32)(cx + cw); i++) {
            t = (i >> 3) + (j >> 2) * ctiles;
            pcb[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scb++ + 0x80;
            pcr[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scr++ + 0x80;
        }
        scb += 8 - cw;
        scr += 8 - cw;
    }
}

void jpgdOutYUV420Edge(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 xe;
    s32 ye;
    JPEGDecHandle* h;
    u8* sy;
    u8* scb;
    u8* scr;
    u32 tiles;
    u8* py;
    u8* pcb;
    u8* pcr;
    s32 w;
    s32 hh;
    s32 i;
    s32 j;
    s32 t;
    s32 cw;
    s32 ch;
    u32 cx;
    u32 cy;
    u32 ctiles;

    sy = ctx->pix.y;
    scb = ctx->pix.cb;
    scr = ctx->pix.cr;
    h = ctx->handle;
    tiles = h->strideY >> 3;
    py = h->planeY;
    pcb = h->planeCb;
    pcr = h->planeCr;
    w = (h->lastX == x) ? h->remX : (u8)(16 / h->scale);
    hh = (h->lastY == y) ? h->remY : (u8)(16 / h->scale);

    xe = x + w;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            PUT_I8(py, i, j, tiles, *sy++);
        }
        sy += 16 - w;
    }

    cx = x >> 1;
    cy = y >> 1;
    cw = (w + 1) >> 1;
    ch = (hh + 1) >> 1;
    ctiles = h->strideC >> 3;
    for (j = cy; j < (s32)(cy + ch); j++) {
        for (i = cx; i < (s32)(cx + cw); i++) {
            t = (i >> 3) + (j >> 2) * ctiles;
            pcb[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scb++ + 0x80;
            pcr[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scr++ + 0x80;
        }
        scb += 8 - cw;
        scr += 8 - cw;
    }
}

void jpgdOutYUV444(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 xe;
    s32 ye;
    u8* pcr;
    u8* sy;
    u8* scb;
    u8* scr;
    s32 n;
    JPEGDecHandle* h;
    s32 j;
    s32 t;
    s32 i;

    u8* py;
    u8* pcb;
    u32 stride;

    sy = ctx->pix.y;
    scb = ctx->pix.y + 0x40;
    scr = ctx->pix.y + 0x80;
    h = ctx->handle;
    n = 8 / h->scale;
    stride = h->strideY >> 3;
    py = h->planeY;
    pcb = h->planeCb;
    pcr = h->planeCr;
    xe = x + n;
    ye = y + n;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            t = (i >> 3) + (j >> 2) * stride;
            py[(t << 5) + ((j & 3) << 3) + (i & 7)] = *sy++;
            pcb[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scb++ + 0x80;
            pcr[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scr++ + 0x80;
        }
        sy += 8 - n;
        scb += 8 - n;
        scr += 8 - n;
    }
}

void jpgdOutYUV444Edge(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 xe;
    s32 ye;
    JPEGDecHandle* h;
    u8* sy;
    u8* scb;
    u8* scr;
    s32 w;
    s32 hh;
    s32 i;
    s32 j;
    s32 t;
    u32 stride;
    u8* py;
    u8* pcb;
    u8* pcr;

    sy = ctx->pix.y;
    scb = ctx->pix.y + 0x40;
    scr = ctx->pix.y + 0x80;
    h = ctx->handle;
    stride = h->strideY >> 3;
    py = h->planeY;
    pcb = h->planeCb;
    pcr = h->planeCr;
    w = (h->lastX == x) ? h->remX : 8 / h->scale;
    hh = (h->lastY == y) ? h->remY : 8 / h->scale;
    xe = x + w;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            t = (i >> 3) + (j >> 2) * stride;
            py[(t << 5) + ((j & 3) << 3) + (i & 7)] = *sy++;
            pcb[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scb++ + 0x80;
            pcr[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scr++ + 0x80;
        }
        sy += 8 - w;
        scb += 8 - w;
        scr += 8 - w;
    }
}

void jpgdOutYUVGray(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 xe;
    s32 ye;
    JPEGDecHandle* h = ctx->handle;
    u8* sy = ctx->pix.y;
    s32 n = 8 / h->scale;
    u8* py = h->planeY;
    u32 tiles = h->strideY >> 3;
    s32 i;
    s32 j;
    s32 t;

    xe = x + n;
    ye = y + n;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            PUT_I8(py, i, j, tiles, *sy++);
        }
        sy += 8 - n;
    }
}

void jpgdOutYUVGrayEdge(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 xe;
    s32 ye;
    JPEGDecHandle* h;
    u8* sy;
    u8* py;
    u32 tiles;
    s32 w;
    s32 hh;
    s32 i;
    s32 j;
    s32 t;

    sy = ctx->pix.y;
    h = ctx->handle;
    py = h->planeY;
    tiles = h->strideY >> 3;
    w = (h->lastX == x) ? h->remX : (u8)(8 / h->scale);
    hh = (h->lastY == y) ? h->remY : (u8)(8 / h->scale);
    xe = x + w;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            PUT_I8(py, i, j, tiles, *sy++);
        }
        sy += 8 - w;
    }
}
