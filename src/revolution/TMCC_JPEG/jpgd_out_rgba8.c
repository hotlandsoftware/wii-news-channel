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

#define YCC_CHROMA_R(cbv, crv)                  \
    cb = (s8)(cbv);                             \
    cr = (s8)(crv);                             \
    cra = (cr * 0x167) >> 8;                    \
    cga = -(cb * 0x58 + cr * 0xB7) >> 8;        \
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
    u8* sy;
    s32 xe;
    s32 k;
    u8* out;
    u8* scb;
    s32 w;
    s32 i;
    s32 g;
    s32 j;
    s32 cb;
    s32 cga;
    JPEGDecHandle* h;
    s32 cr;
    s32 ye;
    s32 r;
    u32 tiles;
    s32 yy;
    s32 cba;
    s32 hh;
    s32 t;
    s32 b;
    s32 cra;
    u8* scr;
    sy = ctx->pix.y;
    scr = ctx->pix.cr;
    scb = ctx->pix.cb;

    h = ctx->handle;
    tiles = TILES();
    out = h->out;
    w = 32 / h->scale;
    hh = 8 / h->scale;
    xe = x + w;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i += 4) {
            YCC_CHROMA_R(*scb++, *scr++);
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
        scb += (32 - w) >> 2;
        scr += (32 - w) >> 2;
    }
}

void jpgdOutRGBA8_411Edge(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 cra;
    s32 i;
    u8* sy;
    u8* scb;
    u32 tiles;
    u8* out;
    s32 cb;
    s32 xe;
    s32 t;
    JPEGDecHandle* h;
    s32 ye;
    s32 yy;
    u8* scr;
    s32 r;
    s32 cga;
    s32 cba;
    s32 hh;
    s32 g;
    s32 b;
    s32 j;
    s32 cr;
    s32 w;
    s32 k;

    h = ctx->handle;
    sy = ctx->pix.y;
    scb = ctx->pix.cb;
    tiles = TILES();
    scr = ctx->pix.cr;
    out = h->out;
    w = (h->lastX == x) ? h->remX : 32 / h->scale;
    xe = x + w;
    hh = (h->lastY == y) ? h->remY : 8 / h->scale;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            if (((i - x) & 3) == 0) {
                YCC_CHROMA_R(*scb++, *scr++);
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
    s32 yy;
    u8* sy;
    u8* scb;
    s32 cra;
    s32 cga;
    s32 hh;
    s32 j;
    JPEGDecHandle* h;
    s32 i;
    s32 t;
    s32 xe;
    s32 cb;
    s32 ye;
    s32 w;
    u8* scr;
    s32 r;
    u8* out;
    s32 cba;
    s32 k;
    s32 cr;
    s32 g;
    s32 b;
    u32 tiles;
    sy = ctx->pix.y;
    scb = ctx->pix.y + 0x80;
    scr = ctx->pix.y + 0xC0;

    h = ctx->handle;
    tiles = TILES();
    out = h->out;
    w = 16 / h->scale;
    hh = 8 / h->scale;
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
        scb += (16 - w) >> 1;
        scr += (16 - w) >> 1;
    }
}

void jpgdOutRGBA8_422Edge(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 w;
    s32 cra;
    s32 cb;
    u8* sy;
    u8* scr;
    JPEGDecHandle* h;
    u8* scb;
    s32 i;
    s32 xe;
    s32 hh;
    u32 tiles;
    s32 ye;
    s32 k;
    s32 j;
    s32 t;
    s32 r;
    u8* out;
    s32 cba;
    s32 cga;
    s32 g;
    s32 cr;
    s32 b;
    s32 yy;

    sy = ctx->pix.y;
    scb = ctx->pix.y + 0x80;
    scr = ctx->pix.y + 0xC0;
    h = ctx->handle;
    tiles = TILES();
    out = h->out;
    w = (h->lastX == x) ? h->remX : 16 / h->scale;
    hh = (h->lastY == y) ? h->remY : 8 / h->scale;
    xe = x + w;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            if (((i - x) & 1) == 0) {
                YCC_CHROMA_R(*scb++, *scr++);
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
    u8* scb;
    u8* scr;
    s32 cba;
    JPEGDecHandle* h;
    s32 hh;
    s32 i;
    u8* out;
    s32 b;
    s32 w;
    s32 cr;
    s32 t;
    s32 cb;
    s32 k;
    u32 tiles;
    u8* sy;
    s32 cga;
    s32 cra;
    s32 g;
    s32 r;
    s32 yy;
    s32 j;
    scr = ctx->pix.cr;
    sy = ctx->pix.y;
    scb = ctx->pix.cb;

    h = ctx->handle;
    tiles = TILES();
    out = h->out;
    w = 16 / h->scale;
    hh = 16 / h->scale;
    for (j = y; j < (s32)(y + hh); j++) {
        for (i = x; i < (s32)(x + w); i += 2) {
            YCC_CHROMA_R(*scb++, *scr++);
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
    s32 w;
    JPEGDecHandle* h;
    s32 k;
    u8* out;
    u8* sy;
    s32 hh;
    s32 cr;
    s32 cb;
    u8* scb;
    s32 r;
    s32 yy;
    u8* scr;
    s32 i;
    s32 t;
    s32 ye;
    u32 tiles;
    s32 cra;
    s32 cga;
    s32 cba;
    s32 g;
    s32 j;
    s32 b;
    s32 xe;
    sy = ctx->pix.y;
    scb = ctx->pix.cb;
    scr = ctx->pix.cr;

    h = ctx->handle;
    tiles = TILES();
    out = h->out;
    w = (h->lastX == x) ? h->remX : 16 / h->scale;
    hh = (h->lastY == y) ? h->remY : 16 / h->scale;
    xe = x + w;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            if (((i - x) & 1) == 0) {
                YCC_CHROMA_R(*scb++, *scr++);
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
    s32 cga;
    s32 t;
    u8* sy;
    u8* scr;
    s32 j;
    s32 cb;
    u8* scb;
    s32 cba;
    s32 i;
    u8* out;
    s32 k;
    JPEGDecHandle* h;
    s32 yy;
    s32 hh;
    s32 ye;
    u32 tiles;
    s32 cra;
    s32 w;
    s32 r;
    s32 g;
    s32 b;
    s32 cr;

    h = ctx->handle;
    sy = ctx->pix.y;
    tiles = TILES();
    out = h->out;
    scr = ctx->pix.y + 0x80;
    hh = 8 / h->scale;
    w = 8 / h->scale;
    scb = ctx->pix.y + 0x40;
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
    s32 ye;
    u8* sy;
    s32 t;
    u8* scr;
    u8* scb;
    s32 i;
    s32 yy;
    s32 cra;
    s32 hh;
    s32 cba;
    s32 xe;
    s32 cga;
    s32 k;
    s32 w;
    u8* out;
    s32 j;
    u32 tiles;
    s32 r;
    s32 g;
    s32 b;
    s32 cb;
    s32 cr;
    JPEGDecHandle* h;
    sy = ctx->pix.y;
    scb = ctx->pix.y + 0x40;
    scr = ctx->pix.y + 0x80;

    h = ctx->handle;
    tiles = TILES();
    out = h->out;
    w = (h->lastX == x) ? h->remX : 8 / h->scale;
    hh = (h->lastY == y) ? h->remY : 8 / h->scale;
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
    s32 cba;
    u8* out;
    u8* scb;
    s32 yy;
    s32 k;
    s32 w;
    s32 j;
    s32 cr;
    s32 cra;
    s32 t;
    s32 hh;
    s32 cb;
    s32 cga;
    u8* sy;
    s32 i;
    u8* scr;
    s32 xe;
    s32 r;
    s32 g;
    s32 ye;
    u32 tiles;
    JPEGDecHandle* h;
    s32 b;
    sy = ctx->pix.y;

    h = ctx->handle;
    tiles = TILES();
    out = h->out;
    w = 8 / h->scale;
    hh = w;
    xe = x + w;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            yy = *sy++;
            if (yy >> 8) {
                b = yy; g = yy; r = yy;
                CLAMP255(b);
                CLAMP255(g);
                CLAMP255(r);
            } else {
                b = yy; g = yy; r = yy;
            }
            PUT_PIXEL(i, j);
        }
        sy += 8 - w;
    }
}

void jpgdOutRGBA8_GrayEdge(JPEGDecContext* ctx, u32 x, u32 y) {
    u8* scr;
    s32 cb;
    s32 cra;
    s32 j;
    s32 cr;
    u8* out;
    s32 t;
    s32 yy;
    u8* sy;
    s32 i;
    s32 xe;
    s32 cga;
    s32 r;
    s32 ye;
    JPEGDecHandle* h;
    u32 tiles;
    s32 w;
    s32 k;
    s32 cba;
    s32 b;
    u8* scb;
    s32 hh;
    s32 g;

    h = ctx->handle;
    out = h->out;
    sy = ctx->pix.y;
    tiles = TILES();
    w = (h->lastX == x) ? h->remX : 8 / h->scale;
    hh = (h->lastY == y) ? h->remY : 8 / h->scale;
    ye = y + hh;
    xe = x + w;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            yy = *sy++;
            if (yy >> 8) {
                b = yy; g = yy; r = yy;
                CLAMP255(b);
                CLAMP255(g);
                CLAMP255(r);
            } else {
                b = yy; g = yy; r = yy;
            }
            PUT_PIXEL(i, j);
        }
        sy += 8 - w;
    }
}

