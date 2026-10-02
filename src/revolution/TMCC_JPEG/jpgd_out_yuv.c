#include <string.h>
#include "jpgd_internal.h"

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
