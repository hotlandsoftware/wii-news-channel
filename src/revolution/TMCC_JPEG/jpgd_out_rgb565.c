#include <string.h>
#include "jpgd_internal.h"

s32 jpgdSetupOutputRGB565(JPEGDecContext* ctx) {

    JPEGPixelBuffer* pix = &ctx->pix;
    JPEGDecHandle* h = ctx->handle;

    switch (ctx->frame.sampling) {
    case 0:
        ctx->output = jpgdOutRGB565_411;
        ctx->outputEdge = jpgdOutRGB565_411Edge;
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
        ctx->output = jpgdOutRGB565_422;
        ctx->outputEdge = jpgdOutRGB565_422Edge;
        ctx->blockOut[0] = pix->y;
        ctx->blockOut[1] = ctx->blockOut[0] + ctx->blockSize;
        ctx->blockOut[5] = pix->y + 0x80;
        ctx->blockOut[6] = pix->y + 0xC0;
        ctx->stride = 16;
        ctx->unk19E0 = 0;
        break;
    case 2:
        ctx->output = jpgdOutRGB565_420;
        ctx->outputEdge = jpgdOutRGB565_420Edge;
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
        ctx->output = jpgdOutRGB565_444;
        ctx->outputEdge = jpgdOutRGB565_444Edge;
        ctx->blockOut[0] = pix->y;
        ctx->blockOut[5] = pix->y + 0x40;
        ctx->blockOut[6] = pix->y + 0x80;
        ctx->stride = 8;
        ctx->unk19E0 = 0;
        break;
    case 4:
        ctx->output = jpgdOutRGB565_Gray;
        ctx->outputEdge = jpgdOutRGB565_GrayEdge;
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

#define PUT_PIXEL(x, y)                                                                   \
    ((u16*)(out + (((y) & 3) << 3)))[((x) & 3) + (((x) >> 2) + ((y) >> 2) * tiles) * 16] = \
        ((g << 3) & 0x7E0) + (((b & 0xF8) >> 3) + ((r << 8) & 0xF800))
#define TILES() (h->strideY >> 2)

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

void jpgdOutRGB565_411(JPEGDecContext* ctx, u32 x, u32 y) {
    u8* out;
    s32 cb;
    s32 g;
    u8* sy;
    s32 k;
    s32 r;
    u8* scb;
    u8* scr;
    s32 cr;
    s32 b;
    s32 i;
    s32 xe;
    JPEGDecHandle* h;
    s32 t;
    s32 hh;
    s32 ye;
    s32 cga;
    u32 tiles;
    s32 w;
    s32 j;
    s32 cra;
    s32 cba;
    s32 yy;

    sy = ctx->pix.y;
    h = ctx->handle;
    scb = ctx->pix.cb;
    scr = ctx->pix.cr;
    w = 32 / h->scale;
    hh = 8 / h->scale;
    tiles = TILES();
    ye = y + hh;
    xe = x + w;
    out = h->out;
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

void jpgdOutRGB565_411Edge(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 xe;
    s32 ye;
    u8* sy;
    s32 k;
    JPEGDecHandle* h;
    s32 j;
    u8* scb;
    u8* scr;
    s32 i;
    s32 cra;
    s32 cba;
    s32 hh;
    u32 tiles;
    s32 cb;
    s32 b;
    s32 cr;
    s32 t;
    s32 cga;
    s32 r;
    u8* out;
    s32 g;
    s32 w;
    s32 yy;

    sy = ctx->pix.y;
    scb = ctx->pix.cb;
    h = ctx->handle;
    out = h->out;
    scr = ctx->pix.cr;
    tiles = TILES();
    hh = (h->lastY == y) ? h->remY : 8 / h->scale;
    xe = x + w;
    w = (h->lastX == x) ? h->remX : 32 / h->scale;
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

void jpgdOutRGB565_422(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 b;
    s32 ye;
    s32 g;
    s32 xe;
    u8* sy;
    s32 cra;
    s32 hh;
    u8* scr;
    s32 k;
    s32 i;
    u8* scb;
    s32 r;
    s32 cr;
    JPEGDecHandle* h;
    u32 tiles;
    s32 cba;
    s32 t;
    s32 w;
    s32 cga;
    u8* out;
    s32 yy;
    s32 cb;
    s32 j;

    scb = ctx->pix.y + 0x80;
    scr = ctx->pix.y + 0xC0;
    h = ctx->handle;
    tiles = TILES();
    sy = ctx->pix.y;
    w = 16 / h->scale;
    hh = 8 / h->scale;
    out = h->out;
    ye = y + hh;
    xe = x + w;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i += 2) {
            YCC_CHROMA_R(*scb++, *scr++);
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

void jpgdOutRGB565_422Edge(JPEGDecContext* ctx, u32 x, u32 y) {
    u8* out;
    u8* scr;
    s32 i;
    s32 g;
    u8* scb;
    u8* sy;
    s32 xe;
    JPEGDecHandle* h;
    s32 ye;
    u32 tiles;
    s32 cga;
    s32 cra;
    s32 hh;
    s32 cba;
    s32 cr;
    s32 cb;
    s32 j;
    s32 k;
    s32 yy;
    s32 b;
    s32 w;
    s32 r;
    s32 t;

    scr = ctx->pix.y + 0xC0;
    h = ctx->handle;
    scb = ctx->pix.y + 0x80;
    out = h->out;
    tiles = TILES();
    sy = ctx->pix.y;
    w = (h->lastX == x) ? h->remX : 16 / h->scale;
    hh = (h->lastY == y) ? h->remY : 8 / h->scale;
    ye = y + hh;
    xe = x + w;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            if (((i - x) & 1) == 0) {
                YCC_CHROMA_R(*scb++, *scr++);
            }
            YCC_PIXEL(*sy++);
            PUT_PIXEL(i, j);
        }
        sy += 16 - w;
        scb += 8 - (w + 1) >> 1;
        scr += 8 - (w + 1) >> 1;
    }
}

void jpgdOutRGB565_420(JPEGDecContext* ctx, u32 x, u32 y) {
    u32 tiles;
    s32 b;
    u8* sy;
    s32 i;
    s32 cga;
    u8* scr;
    u8* out;
    s32 xe;
    s32 k;
    s32 hh;
    s32 j;
    s32 ye;
    s32 cb;
    u8* scb;
    s32 cra;
    s32 cba;
    s32 t;
    s32 r;
    s32 g;
    s32 cr;
    s32 w;
    s32 yy;
    JPEGDecHandle* h;

    sy = ctx->pix.y;
    scb = ctx->pix.cb;
    h = ctx->handle;
    out = h->out;
    hh = 16 / h->scale;
    scr = ctx->pix.cr;
    tiles = TILES();
    w = 16 / h->scale;
    xe = x + w;
    ye = y + hh;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i += 2) {
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

void jpgdOutRGB565_420Edge(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 cb;
    s32 t;
    u32 tiles;
    s32 k;
    u8* sy;
    u8* scb;
    u8* scr;
    s32 w;
    u8* out;
    s32 hh;
    JPEGDecHandle* h;
    s32 j;
    s32 xe;
    s32 ye;
    s32 i;
    s32 cga;
    s32 cra;
    s32 cba;
    s32 g;
    s32 r;
    s32 b;
    s32 cr;
    s32 yy;

    sy = ctx->pix.y;
    scb = ctx->pix.cb;
    h = ctx->handle;
    scr = ctx->pix.cr;
    tiles = TILES();
    out = h->out;
    w = (h->lastX == x) ? h->remX : 16 / h->scale;
    hh = (h->lastY == y) ? h->remY : 16 / h->scale;
    ye = y + hh;
    xe = x + w;
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

void jpgdOutRGB565_444(JPEGDecContext* ctx, u32 x, u32 y) {
    u8* out;
    u8* sy;
    u8* scr;
    u8* scb;
    s32 g;
    s32 w;
    s32 cra;
    s32 i;
    s32 ye;
    s32 j;
    s32 cba;
    s32 cga;
    s32 t;
    s32 xe;
    u32 tiles;
    s32 k;
    s32 b;
    s32 r;
    s32 cb;
    s32 cr;
    s32 hh;
    JPEGDecHandle* h;
    s32 yy;

    sy = ctx->pix.y;
    scr = ctx->pix.y + 0x80;
    h = ctx->handle;
    scb = ctx->pix.y + 0x40;
    tiles = TILES();
    hh = 8 / h->scale;
    out = h->out;
    w = 8 / h->scale;
    ye = y + hh;
    xe = x + w;
    for (j = y; j < ye; j++) {
        for (i = x; i < xe; i++) {
            YCC_CHROMA(*scb++, *scr++);
            YCC_PIXEL(*sy++);
            ((u16*)(out + ((j & 3) << 3)))[(i & 3) + ((i >> 2) + (j >> 2) * tiles) * 16] =
                ((g << 3) & 0x7E0) + (((b & 0xF8) >> 3) + ((r << 8) & 0xF800));
        }
        sy += 8 - w;
        scb += 8 - w;
        scr += 8 - w;
    }
}

void jpgdOutRGB565_444Edge(JPEGDecContext* ctx, u32 x, u32 y) {
    u32 tiles;
    u8* sy;
    u8* scb;
    u8* scr;
    s32 cra;
    JPEGDecHandle* h;
    s32 cr;
    s32 hh;
    s32 i;
    s32 k;
    s32 cga;
    s32 xe;
    s32 j;
    u8* out;
    s32 ye;
    s32 r;
    s32 t;
    s32 cba;
    s32 w;
    s32 cb;
    s32 yy;
    s32 b;
    s32 g;

    sy = ctx->pix.y;
    scb = ctx->pix.y + 0x40;
    scr = ctx->pix.y + 0x80;
    h = ctx->handle;
    tiles = TILES();
    out = h->out;
    w = (h->lastX == x) ? h->remX : 8 / h->scale;
    hh = (h->lastY == y) ? h->remY : 8 / h->scale;
    ye = y + hh;
    xe = x + w;
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

void jpgdOutRGB565_Gray(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 j;
    s32 r;
    u8* out;
    u8* sy;
    s32 i;
    JPEGDecHandle* h;
    s32 hh;
    s32 w;
    s32 g;
    s32 xe;
    s32 ye;
    s32 yy;
    s32 b;
    u32 tiles;

    h = ctx->handle;
    sy = ctx->pix.y;
    tiles = TILES();
    w = 8 / h->scale;
    xe = x + w;
    hh = w;
    out = h->out;
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

void jpgdOutRGB565_GrayEdge(JPEGDecContext* ctx, u32 x, u32 y) {
    s32 g;
    u8* scb;
    u8* out;
    u8* sy;
    s32 t;
    s32 b;
    s32 i;
    s32 cra;
    JPEGDecHandle* h;
    s32 cr;
    u8* scr;
    s32 r;
    s32 k;
    s32 cga;
    s32 w;
    s32 cb;
    s32 cba;
    s32 xe;
    s32 j;
    s32 ye;
    u32 tiles;
    s32 yy;
    s32 hh;

    h = ctx->handle;
    tiles = TILES();
    out = h->out;
    sy = ctx->pix.y;
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

