#include <string.h>
#include "jpgd_internal.h"

s32 fn_800882F8(JPEGDecContext* ctx) {
    JPEGPixelBuffer* pix = &ctx->pix;
    JPEGDecHandle* h = ctx->handle;

    switch (ctx->frame.sampling) {
    case 0:
        ctx->output = fn_80088544;
        ctx->outputEdge = fn_8008890C;
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
        ctx->output = fn_80088ACC;
        ctx->outputEdge = fn_80088D1C;
        ctx->blockOut[0] = pix->y;
        ctx->blockOut[1] = ctx->blockOut[0] + ctx->blockSize;
        ctx->blockOut[5] = pix->y + 0x80;
        ctx->blockOut[6] = pix->y + 0xC0;
        ctx->stride = 16;
        ctx->unk19E0 = 0;
        break;
    case 2:
        ctx->output = fn_80088EE0;
        ctx->outputEdge = fn_80089144;
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
        ctx->output = fn_80089324;
        ctx->outputEdge = fn_800894A8;
        ctx->blockOut[0] = pix->y;
        ctx->blockOut[5] = pix->y + 0x40;
        ctx->blockOut[6] = pix->y + 0x80;
        ctx->stride = 8;
        ctx->unk19E0 = 0;
        break;
    case 4:
        ctx->output = fn_80089660;
        ctx->outputEdge = fn_80089790;
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
