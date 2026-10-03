#include <string.h>
#include "jpgd_internal.h"

s32 jpgdSetupOutputRGBA8(JPEGDecContext* ctx) {
    JPEGPixelBuffer* pix = &ctx->pix;
    JPEGDecHandle* h = ctx->handle;

    switch (ctx->frame.sampling) {
    case 0:
        ctx->output = jpgdOutRGBA8_411;
        ctx->outputEdge = jpgdOutRGBA8_411Edge;
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
        ctx->output = jpgdOutRGBA8_422;
        ctx->outputEdge = jpgdOutRGBA8_422Edge;
        ctx->blockOut[0] = pix->y;
        ctx->blockOut[1] = ctx->blockOut[0] + ctx->blockSize;
        ctx->blockOut[5] = pix->y + 0x80;
        ctx->blockOut[6] = pix->y + 0xC0;
        ctx->stride = 16;
        ctx->unk19E0 = 0;
        break;
    case 2:
        ctx->output = jpgdOutRGBA8_420;
        ctx->outputEdge = jpgdOutRGBA8_420Edge;
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
        ctx->output = jpgdOutRGBA8_444;
        ctx->outputEdge = jpgdOutRGBA8_444Edge;
        ctx->blockOut[0] = pix->y;
        ctx->blockOut[5] = pix->y + 0x40;
        ctx->blockOut[6] = pix->y + 0x80;
        ctx->stride = 8;
        ctx->unk19E0 = 0;
        break;
    case 4:
        ctx->output = jpgdOutRGBA8_Gray;
        ctx->outputEdge = jpgdOutRGBA8_GrayEdge;
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
        h->strideY = (w / 4 + (w % 4 != 0)) * 4;
        h->heightY = (hg / 4 + (hg % 4 != 0)) * 4;
    }
    return 0;
}

/* outputs */

#define PUT_PIXEL(x, y)                                                                          \
    t = ((x) >> 2) * 2 + ((y) >> 2) * tiles;                                                     \
    ((u16*)(out + (((y) & 3) << 3)))[((x) & 3) + t * 16] = (u8)r + 0xFF00;               \
    ((u16*)(out + (((y) & 3) << 3)))[((x) & 3) + (t + 1) * 16] = ((g << 8) & 0xFF00) + (u8)b
#define TILES() ((h->strideY >> 1) & ~1)

#define YCC_CHROMA(cbv, crv)                    \
    cb = (s8)(cbv);                             \
    cr = (s8)(crv);                             \
    cga = -(cb * 0x58 + cr * 0xB7) >> 8;        \
    cra = (cr * 0x167) >> 8;                    \
    cba = (cb * 0x1C6) >> 8

#define CLAMP255(v)    \
    (v) = ((v) > 255) ? 255 : ((v) < 0) ? 0 : (v)

#define YCC_PIXEL(yv)                     \
    yy = (yv);                            \
    r = yy + cra;                         \
    g = yy + cga;                         \
    b = yy + cba;                         \
    if ((b | r | g) >> 8) {               \
        CLAMP255(b);                      \
        CLAMP255(g);                      \
        CLAMP255(r);                      \
    }

void jpgdOutRGBA8_411(JPEGDecContext* ctx, u32 x, u32 y) {
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
    s32 k;
    s32 t;
    u32 tiles;
    s32 cra;
    s32 cga;
    s32 cba;
    s32 r;
    s32 g;
    s32 b;
    s32 cb;
    s32 cr;
    s32 yy;
    u8* out;

    sy = ctx->pix.y;
    scb = ctx->pix.cb;
    scr = ctx->pix.cr;
    h = ctx->handle;
    w = 32 / h->scale;
    hh = 8 / h->scale;
    tiles = TILES();
    out = h->out;
    xe = x + w;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i += 4) {
            YCC_CHROMA(*scb++, *scr++);
            YCC_PIXEL(sy[0]);
            PUT_PIXEL(i, j);
            YCC_PIXEL(sy[1]);
            PUT_PIXEL(i + 1, j);
            YCC_PIXEL(sy[2]);
            PUT_PIXEL(i + 2, j);
            YCC_PIXEL(sy[3]);
            PUT_PIXEL(i + 3, j);
            sy += 4;
        }
        sy += 32 - w;
        scb += (32 - w) / 4;
        scr += (32 - w) / 4;
    }
}

void jpgdOutRGBA8_411Edge(JPEGDecContext* ctx, u32 x, u32 y) {
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
    s32 k;
    s32 t;
    u32 tiles;
    s32 cra;
    s32 cga;
    s32 cba;
    s32 r;
    s32 g;
    s32 b;
    s32 cb;
    s32 cr;
    s32 yy;
    u8* out;

    sy = ctx->pix.y;
    scb = ctx->pix.cb;
    scr = ctx->pix.cr;
    h = ctx->handle;
    w = (h->lastX == x) ? h->remX : 32 / h->scale;
    hh = (h->lastY == y) ? h->remY : 8 / h->scale;
    tiles = TILES();
    out = h->out;
    xe = x + w;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            if (((i - x) & 3) == 0) {
                YCC_CHROMA(*scb++, *scr++);
            }
            YCC_PIXEL(*sy++);
            PUT_PIXEL(i, j);
        }
        sy += 32 - w;
        scb += 8 - (w + 3) / 4;
        scr += 8 - (w + 3) / 4;
    }
}

void jpgdOutRGBA8_422(JPEGDecContext* ctx, u32 x, u32 y) {
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
    s32 k;
    s32 t;
    u32 tiles;
    s32 cra;
    s32 cga;
    s32 cba;
    s32 r;
    s32 g;
    s32 b;
    s32 cb;
    s32 cr;
    s32 yy;
    u8* out;

    sy = ctx->pix.y;
    scb = ctx->pix.y + 0x80;
    scr = ctx->pix.y + 0xC0;
    h = ctx->handle;
    w = 16 / h->scale;
    hh = 8 / h->scale;
    tiles = TILES();
    out = h->out;
    xe = x + w;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i += 2) {
            YCC_CHROMA(*scb++, *scr++);
            YCC_PIXEL(sy[0]);
            PUT_PIXEL(i, j);
            YCC_PIXEL(sy[1]);
            PUT_PIXEL(i + 1, j);
            sy += 2;
        }
        sy += 16 - w;
        scb += (16 - w) / 2;
        scr += (16 - w) / 2;
    }
}

void jpgdOutRGBA8_422Edge(JPEGDecContext* ctx, u32 x, u32 y) {
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
    s32 k;
    s32 t;
    u32 tiles;
    s32 cra;
    s32 cga;
    s32 cba;
    s32 r;
    s32 g;
    s32 b;
    s32 cb;
    s32 cr;
    s32 yy;
    u8* out;

    sy = ctx->pix.y;
    scb = ctx->pix.y + 0x80;
    scr = ctx->pix.y + 0xC0;
    h = ctx->handle;
    w = (h->lastX == x) ? h->remX : 16 / h->scale;
    hh = (h->lastY == y) ? h->remY : 8 / h->scale;
    tiles = TILES();
    out = h->out;
    xe = x + w;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            if (((i - x) & 1) == 0) {
                YCC_CHROMA(*scb++, *scr++);
            }
            YCC_PIXEL(*sy++);
            PUT_PIXEL(i, j);
        }
        sy += 16 - w;
        scb += 8 - (w + 1) / 2;
        scr += 8 - (w + 1) / 2;
    }
}

void jpgdOutRGBA8_420(JPEGDecContext* ctx, u32 x, u32 y) {
    JPEGDecHandle* h;
    u8* sy;
    u8* scb;
    u8* scr;
    s32 w;
    s32 hh;
    s32 i;
    s32 j;
    s32 k;
    s32 t;
    u32 tiles;
    s32 cra;
    s32 cga;
    s32 cba;
    s32 r;
    s32 g;
    s32 b;
    s32 cb;
    s32 cr;
    s32 yy;
    u8* out;

    sy = ctx->pix.y;
    scb = ctx->pix.cb;
    scr = ctx->pix.cr;
    h = ctx->handle;
    w = 16 / h->scale;
    hh = 16 / h->scale;
    tiles = TILES();
    out = h->out;
    for (j = y; j < (s32)(y + hh); j++) {
        for (i = x; i < (s32)(x + w); i += 2) {
            YCC_CHROMA(*scb++, *scr++);
            YCC_PIXEL(sy[0]);
            PUT_PIXEL(i, j);
            YCC_PIXEL(sy[1]);
            PUT_PIXEL(i + 1, j);
            sy += 2;
        }
        sy += 16 - w;
        if (j & 1) {
            scb += (16 - w) >> 1;
            scr += (16 - w) >> 1;
        } else {
            scb -= (w + 1) >> 1;
            scr -= (w + 1) >> 1;
        }
    }
}

void jpgdOutRGBA8_420Edge(JPEGDecContext* ctx, u32 x, u32 y) {
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
    s32 k;
    s32 t;
    u32 tiles;
    s32 cra;
    s32 cga;
    s32 cba;
    s32 r;
    s32 g;
    s32 b;
    s32 cb;
    s32 cr;
    s32 yy;
    u8* out;

    sy = ctx->pix.y;
    scb = ctx->pix.cb;
    scr = ctx->pix.cr;
    h = ctx->handle;
    w = (h->lastX == x) ? h->remX : 16 / h->scale;
    hh = (h->lastY == y) ? h->remY : 16 / h->scale;
    tiles = TILES();
    out = h->out;
    xe = x + w;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            if (((i - x) & 1) == 0) {
                YCC_CHROMA(*scb++, *scr++);
            }
            YCC_PIXEL(*sy++);
            PUT_PIXEL(i, j);
        }
        sy += 16 - w;
        if (j & 1) {
            scb += (16 - w) >> 1;
            scr += (16 - w) >> 1;
        } else {
            scb -= (w + 1) >> 1;
            scr -= (w + 1) >> 1;
        }
    }
}

void jpgdOutRGBA8_444(JPEGDecContext* ctx, u32 x, u32 y) {
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
    s32 k;
    s32 t;
    u32 tiles;
    s32 cra;
    s32 cga;
    s32 cba;
    s32 r;
    s32 g;
    s32 b;
    s32 cb;
    s32 cr;
    s32 yy;
    u8* out;

    sy = ctx->pix.y;
    scb = ctx->pix.y + 0x40;
    scr = ctx->pix.y + 0x80;
    h = ctx->handle;
    w = 8 / h->scale;
    hh = 8 / h->scale;
    tiles = TILES();
    out = h->out;
    xe = x + w;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i += 1) {
            YCC_CHROMA(*scb++, *scr++);
            YCC_PIXEL(sy[0]);
            PUT_PIXEL(i, j);
            sy += 1;
        }
        sy += 8 - w;
        scb += 8 - w;
        scr += 8 - w;
    }
}

void jpgdOutRGBA8_444Edge(JPEGDecContext* ctx, u32 x, u32 y) {
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
    s32 k;
    s32 t;
    u32 tiles;
    s32 cra;
    s32 cga;
    s32 cba;
    s32 r;
    s32 g;
    s32 b;
    s32 cb;
    s32 cr;
    s32 yy;
    u8* out;

    sy = ctx->pix.y;
    scb = ctx->pix.y + 0x40;
    scr = ctx->pix.y + 0x80;
    h = ctx->handle;
    w = (h->lastX == x) ? h->remX : 8 / h->scale;
    hh = (h->lastY == y) ? h->remY : 8 / h->scale;
    tiles = TILES();
    out = h->out;
    xe = x + w;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            YCC_CHROMA(*scb++, *scr++);
            YCC_PIXEL(*sy++);
            PUT_PIXEL(i, j);
        }
        sy += 8 - w;
        scb += 8 - w;
        scr += 8 - w;
    }
}

void jpgdOutRGBA8_Gray(JPEGDecContext* ctx, u32 x, u32 y) {
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
    s32 k;
    s32 t;
    u32 tiles;
    s32 cra;
    s32 cga;
    s32 cba;
    s32 r;
    s32 g;
    s32 b;
    s32 cb;
    s32 cr;
    s32 yy;
    u8* out;

    sy = ctx->pix.y;
    h = ctx->handle;
    w = 8 / h->scale;
    hh = w;
    tiles = TILES();
    out = h->out;
    xe = x + w;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            r = *sy++;
            g = r;
            b = r;
            if ((b | r | g) >> 8) {
                CLAMP255(b);
                CLAMP255(g);
                CLAMP255(r);
            }
            PUT_PIXEL(i, j);
        }
        sy += 8 - w;
    }
}

void jpgdOutRGBA8_GrayEdge(JPEGDecContext* ctx, u32 x, u32 y) {
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
    s32 k;
    s32 t;
    u32 tiles;
    s32 cra;
    s32 cga;
    s32 cba;
    s32 r;
    s32 g;
    s32 b;
    s32 cb;
    s32 cr;
    s32 yy;
    u8* out;

    sy = ctx->pix.y;
    h = ctx->handle;
    w = (h->lastX == x) ? h->remX : 8 / h->scale;
    hh = (h->lastY == y) ? h->remY : 8 / h->scale;
    tiles = TILES();
    out = h->out;
    xe = x + w;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            r = *sy++;
            g = r;
            b = r;
            if ((b | r | g) >> 8) {
                CLAMP255(b);
                CLAMP255(g);
                CLAMP255(r);
            }
            PUT_PIXEL(i, j);
        }
        sy += 8 - w;
    }
}

