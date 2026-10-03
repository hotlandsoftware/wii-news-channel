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

static inline void jpgdPutI8(u8* p, s32 x, s32 y, u32 tiles, u8 v) {
    s32 t = (x >> 3) + (y >> 2) * tiles;
    p[(t << 5) + ((y & 3) << 3) + (x & 7)] = v;
}

static inline void jpgdPutC8(u8* pb, u8* pr, s32 x, s32 y, u32 tiles, u8 b, u8 r) {
    s32 t = (x >> 3) + (y >> 2) * tiles;
    pb[(t << 5) + ((y & 3) << 3) + (x & 7)] = b;
    pr[(t << 5) + ((y & 3) << 3) + (x & 7)] = r;
}

void jpgdOutYUV411(JPEGDecContext* ctx, u32 x, u32 y) {
    u8* sy;
    s32 j;
    u8* pcr;
    u8* py;
    s32 cw;
    s32 hh;
    u32 cstride;
    u8* scb;
    u32 stride;
    s32 t;
    s32 i;
    s32 w;
    u8* scr;
    u8* pcb;
    JPEGDecHandle* h;

    sy = ctx->pix.y;
    h = ctx->handle;
    hh = 8 / h->scale;
    scb = ctx->pix.cb;
    py = h->planeY;
    stride = h->strideY >> 3;
    pcb = h->planeCb;
    pcr = h->planeCr;
    scr = ctx->pix.cr;
    w = 32 / h->scale;
    for (j = y; j < (s32)(y + hh); j++) {
        for (i = x; i < (s32)(x + w); i += 4) {
            jpgdPutI8(py, i, j, stride, sy[0]);
            jpgdPutI8(py, i + 1, j, stride, sy[1]);
            jpgdPutI8(py, i + 2, j, stride, sy[2]);
            jpgdPutI8(py, i + 3, j, stride, sy[3]);
            sy += 4;
        }
        sy += 32 - w;
    }

    cstride = h->strideC >> 3;
    cw = 8 / h->scale;
    x >>= 2;
    for (j = y; j < (s32)(y + cw); j++) {
        for (i = x; i < (s32)(x + cw); i++) {
            t = (i >> 3) + (j >> 2) * cstride;
            pcb[(t << 5) + ((j & 3) << 3) + (7 & i)] = *scb++ + 0x80;
            pcr[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scr++ + 0x80;
        }
        scb += 8 - cw;
        scr += 8 - cw;
    }
}

void jpgdOutYUV411Edge(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 j;
    u8* pcb;
    s32 w;
    u8* scr;
    u8* pcr;
    u8* scb;
    u32 ctiles;
    s32 hh;
    u32 cx;
    JPEGDecHandle* h;
    s32 i;
    s32 ye;
    s32 t;
    u8* sy;
    u8* py;
    s32 cw;
    s32 xe;
    u32 tiles;

    h = ctx->handle;
    tiles = h->strideY >> 3;
    scb = ctx->pix.cb;
    pcr = h->planeCr;
    py = h->planeY;
    sy = ctx->pix.y;
    w = (h->lastX == x) ? h->remX : (u8)(32 / h->scale);
    scr = ctx->pix.cr;
    hh = (h->lastY == y) ? h->remY : (u8)(8 / h->scale);
    pcb = h->planeCb;

    xe = x + w;
    ye = hh + y;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            PUT_I8(py, i, j, tiles, *sy++);
        }
        sy += 32 - w;
    }

    cw = (3 + w) >> 2;
    cx = x >> 2;
    ctiles = h->strideC >> 3;
    for (j = y; j < (s32)(hh + y); j++) {
        for (i = cx; i < (s32)(cx + cw); i++) {
            t = (i >> 3) + (j >> 2) * ctiles;
            pcb[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scb++ + 0x80;
            pcr[(t << 5) + ((j & 3) << 3) + (7 & i)] = *scr++ + 0x80;
        }
        scb += 8 - cw;
        scr += 8 - cw;
    }
}

void jpgdOutYUV422(JPEGDecContext* ctx, u32 x, u32 y) {
    u32 cstride;
    u8* py;
    u32 stride;
    s32 hh;
    s32 xe;
    s32 j;
    s32 i;
    u8* sy;
    s32 ye;
    s32 w;
    s32 t;
    JPEGDecHandle* h;
    s32 cw;
    u8* pcb;
    u8* pcr;
    u8* scb;
    u8* scr;

    h = ctx->handle;
    stride = h->strideY >> 3;
    hh = 8 / h->scale;
    sy = ctx->pix.y;
    pcb = h->planeCb;
    scb = ctx->pix.y + 0x80;
    w = 16 / h->scale;
    scr = 0xC0 + ctx->pix.y;
    py = h->planeY;
    pcr = h->planeCr;
    for (j = y; j < (s32)(y + hh); j++) {
        for (i = x; i < (s32)(x + w); i += 2) {
            jpgdPutI8(py, i, j, stride, sy[0]);
            jpgdPutI8(py, i + 1, j, stride, sy[1]);
            sy = sy + 2;
        }
        sy += 16 - w;
    }

    cw = 8 / h->scale;
    x >>= 1;
    ye = y + cw;
    xe = cw + x;
    cstride = h->strideC >> 3;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            t = (i >> 3) + (j >> 2) * cstride;
            pcb[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scb++ + 0x80;
            pcr[(t << 5) + ((j & 3) << 3) + (7 & i)] = *scr++ + 0x80;
        }
        scb += 8 - cw;
        scr += 8 - cw;
    }
}

void jpgdOutYUV422Edge(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 hh;
    u8* sy;
    s32 i;
    s32 j;
    s32 ye;
    s32 cw;
    u8* pcb;
    u32 cx;
    u8* pcr;
    s32 w;
    u32 tiles;
    u8* scr;
    u8* scb;
    s32 t;
    JPEGDecHandle* h;
    u8* py;
    u32 ctiles;
    s32 xe;

    sy = ctx->pix.y;
    scb = ctx->pix.y + 0x80;
    h = ctx->handle;
    tiles = h->strideY >> 3;
    py = h->planeY;
    scr = ctx->pix.y + 0xC0;
    pcb = h->planeCb;
    hh = (h->lastY == y) ? h->remY : (u8)(8 / h->scale);
    w = (h->lastX == x) ? h->remX : (u8)(16 / h->scale);
    pcr = h->planeCr;
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
    s32 t;
    s32 cw;
    u8* py;
    s32 i;
    u8* pcb;
    u8* sy;
    s32 j;
    u8* scb;
    s32 w;
    u8* scr;
    u32 cstride;
    u32 stride;
    JPEGDecHandle* h;
    u8* pcr;
    u32 cx;
    u32 cy;

    sy = ctx->pix.y;
    scb = ctx->pix.cb;
    scr = ctx->pix.cr;
    h = ctx->handle;
    w = 16 / h->scale;
    stride = h->strideY >> 3;
    pcb = h->planeCb;
    py = h->planeY;
    pcr = h->planeCr;
    for (j = y; j < (s32)(y + w); j++) {
        for (i = x; i < (s32)(x + w); i += 2) {
            jpgdPutI8(py, i, j, stride, sy[0]);
            jpgdPutI8(py, i + 1, j, stride, sy[1]);
            sy += 2;
        }
        sy += 16 - w;
    }

    cx = x >> 1;
    cy = y >> 1;
    cw = 8 / h->scale;
    cstride = h->strideC >> 3;
    for (j = cy; j < (s32)(cy + cw); j++) {
        for (i = cx; i < (s32)(cx + cw); i++) {
            t = (i >> 3) + (j >> 2) * cstride;
            pcb[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scb++ + 0x80;
            pcr[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scr++ + 0x80;
        }
        scb += 8 - cw;
        scr += 8 - cw;
    }
}

void jpgdOutYUV420Edge(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 i;
    s32 j;
    s32 hh;
    u32 tiles;
    u32 cy;
    s32 xe;
    s32 ch;
    u8* pcb;
    u8* py;
    u8* pcr;
    JPEGDecHandle* h;
    s32 w;
    s32 t;
    u8* sy;
    u8* scb;
    u32 ctiles;
    u8* scr;
    s32 ye;
    s32 cw;

    h = ctx->handle;
    sy = ctx->pix.y;
    tiles = h->strideY >> 3;
    scr = ctx->pix.cr;
    py = h->planeY;
    pcb = h->planeCb;
    scb = ctx->pix.cb;
    w = (h->lastX == x) ? h->remX : (u8)(16 / h->scale);
    hh = (h->lastY == y) ? h->remY : (u8)(16 / h->scale);
    pcr = h->planeCr;

    xe = x + w;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            PUT_I8(py, i, j, tiles, *sy++);
        }
        sy += 16 - w;
    }

    x >>= 1;
    cy = y >> 1;
    cw = (w + 1) >> 1;
    ch = (hh + 1) >> 1;
    ctiles = h->strideC >> 3;
    for (j = cy; j < (s32)(cy + ch); j++) {
        for (i = x; i < (s32)(x + cw); i++) {
            t = (i >> 3) + (j >> 2) * ctiles;
            pcb[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scb++ + 0x80;
            pcr[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scr++ + 0x80;
        }
        scb += 8 - cw;
        scr += 8 - cw;
    }
}

void jpgdOutYUV444(JPEGDecContext* ctx, u32 x, u32 y) {
    u8* py;
    u8* pcb;
    u8* pcr;
    u8* sy;
    s32 t;
    JPEGDecHandle* h;
    u8* scb;
    s32 n;
    u8* scr;
    s32 i;
    s32 xe;
    s32 j;
    s32 ye;
    u32 stride;

    h = ctx->handle;
    stride = h->strideY >> 3;
    n = 8 / h->scale;
    py = h->planeY;
    sy = ctx->pix.y;
    scb = 0x40 + ctx->pix.y;
    scr = 0x80 + ctx->pix.y;
    xe = x + n;
    pcb = h->planeCb;
    ye = y + n;
    pcr = h->planeCr;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            t = (i >> 3) + (j >> 2) * stride;
            py[(t << 5) + ((3 & j) << 3) + (7 & i)] = *sy++;
            pcb[(t << 5) + ((j & 3) << 3) + (i & 7)] = *scb++ + 0x80;
            pcr[(t << 5) + ((j & 3) << 3) + (7 & i)] = *scr++ + 0x80;
        }
        sy += 8 - n;
        scb += 8 - n;
        scr += 8 - n;
    }
}

void jpgdOutYUV444Edge(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 w;
    u8* py;
    u8* pcr;
    u8* pcb;
    u8* sy;
    u8* scb;
    s32 t;
    u8* scr;
    s32 i;
    s32 j;
    s32 hh;
    s32 xe;
    s32 ye;
    JPEGDecHandle* h;
    u32 stride;

    sy = ctx->pix.y;
    h = ctx->handle;
    scb = ctx->pix.y + 0x40;
    scr = ctx->pix.y + 0x80;
    py = h->planeY;
    stride = h->strideY >> 3;
    pcr = h->planeCr;
    pcb = h->planeCb;
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
    s32 j;

    JPEGDecHandle* h = ctx->handle;
    u8* sy = ctx->pix.y;
    s32 n = 8 / h->scale;
    u8* py = h->planeY;
    u32 tiles = h->strideY >> 3;
    s32 t;
    s32 ye;
    s32 i;

    ye = y + n;
    xe = x + n;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            PUT_I8(py, i, j, tiles, *sy++);
        }
        sy += 8 - n;
    }
}

void jpgdOutYUVGrayEdge(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 w;
    u8* py;
    s32 hh;
    JPEGDecHandle* h;
    u8* sy;
    s32 t;
    s32 i;
    s32 xe;
    s32 ye;
    u32 tiles;
    s32 j;

    h = ctx->handle;
    py = h->planeY;
    sy = ctx->pix.y;
    tiles = h->strideY >> 3;
    w = (h->lastX == x) ? h->remX : (u8)(8 / h->scale);
    hh = (h->lastY == y) ? h->remY : (u8)(8 / h->scale);
    ye = y + hh;
    xe = x + w;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            PUT_I8(py, i, j, tiles, *sy++);
        }
        sy += 8 - w;
    }
}
