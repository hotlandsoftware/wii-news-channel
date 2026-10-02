#include <string.h>
#include "jpgd_internal.h"

static s32 jpgdSetParam(JPEGDecHandle* h, JPEGDecParam* p) {
    if (p->noEoiCheck != 0) {
        return -1;
    }
    if (p->format != 0 && p->format != 1 && p->format != 2) {
        return -1;
    }
    h->noEoiCheck = p->noEoiCheck;
    h->scale = 1;
    h->format = p->format;
    return 0;
}

static s32 jpgdReadHeader(JPEGDecContext* ctx) {
    s32 ret = fn_80081C6C(ctx);
    if (ret < 0) {
        return ret;
    }
    if (ctx->frame.width == 0) {
        return -2;
    }
    ctx->frame.alignedWidth = ctx->frame.mcuWidth * ctx->frame.mcusX;
    ctx->frame.alignedHeight = ctx->frame.mcuHeight * ctx->frame.mcusY;
    return ret;
}

static s32 jpgdStartScan(JPEGDecContext* ctx) {
    s32 ret = fn_80081EB8(ctx);
    if (ret < 0) {
        return ret;
    }
    if (ctx->frame.eoi == 1) {
        return 0;
    }
    return ret;
}

s32 fn_80081348(JPEGDecHandle* h, JPEGDecParam* p) {
    JPEGDecContext* ctx = p->work;
    s32 ret;

    if (ctx == NULL) {
        return -1;
    }
    memset(h, 0, sizeof(JPEGDecHandle));
    memset(ctx, 0, sizeof(JPEGDecContext));
    ctx->handle = h;
    h->ctx = ctx;

    ret = jpgdSetParam(h, p);
    if (ret >= 0) {
        ret = fn_8007FE28(&ctx->stream, &p->source);
        if (ret < 0) {
            return ret;
        }
        ret = jpgdReadHeader(ctx);
        if (ret >= 0) {
            ret = jpgdStartScan(ctx);
            if (ret >= 0 && ctx->frame.eoi == 0) {
                ret = fn_80081794(ctx);
                if (ret >= 0) {
                    switch (h->format) {
                    case 0:
                        ret = fn_80086E68(ctx);
                        if (ret < 0) {
                            goto error;
                        }
                        break;
                    case 1:
                        ret = fn_800882F8(ctx);
                        if (ret < 0) {
                            goto error;
                        }
                        break;
                    case 2:
                        ret = fn_80083C60(ctx);
                        if (ret < 0) {
                            goto error;
                        }
                        break;
                    }
                    h->readSize = fn_80080354(&ctx->stream);
                    return h->numMcus;
                }
            }
        }
    }
error:
    return ret < 0 ? ret : -2;
}

s32 fn_80081554(JPEGDecHandle* h, s32 count, void* out) {
    JPEGDecContext* ctx = h->ctx;
    s32 work[99];
    s32 n;
    u16 x;
    u16 y;
    u16 mcusX;
    u16 mcusY;
    u8 mcuW;
    u8 mcuH;
    s32 ret;

    if (ctx == NULL) {
        return -1;
    }
    if (h->format != 0) {
        return -4;
    }
    h->out = out;
    n = 0;
    x = h->mcuX;
    y = h->mcuY;
    mcusX = h->mcusX;
    mcusY = h->mcusY;
    mcuW = h->mcuWidth;
    mcuH = h->mcuHeight;
    while (y < mcusY && n < count) {
        u32 py = y * mcuH;
        while (x < mcusX && n < count) {
            ret = fn_80081AA8(x * mcuW, py, ctx, work);
            if (ret < 0) {
                if (ctx->frame.restartInterval != 0 && ret != -0xF0) {
                    h->error = ret;
                    ret = fn_80082E4C(ctx);
                }
                return ret;
            }
            x++;
            n++;
        }
        if (mcusX == x) {
            x = 0;
            y++;
        }
    }
    if (mcusY == y && x == 0) {
        ret = fn_80081DFC(ctx);
        if (ret < 0) {
            return ret;
        }
    }
    h->mcuX = x;
    h->mcuY = y;
    h->readSize = fn_80080354(&ctx->stream);
    return h->numMcus - y * mcusX - x;
}

s32 fn_800816B4(JPEGDecHandle* h, u8 scale) {
    JPEGDecContext* ctx;
    s32 ret;

    h->scale = scale;
    ctx = h->ctx;
    if (scale != 1 && scale != 2 && scale != 4 && scale != 8) {
        return -1;
    }
    ret = fn_80081794(ctx);
    if (ret < 0) {
        return ret;
    }
    switch (h->format) {
    case 0:
        ret = fn_80086E68(ctx);
        if (ret < 0) {
            return ret;
        }
        break;
    case 1:
        ret = fn_800882F8(ctx);
        if (ret < 0) {
            return ret;
        }
        break;
    case 2:
        ret = fn_80083C60(ctx);
        if (ret < 0) {
            return ret;
        }
        break;
    }
    return 0;
}


#define DIV_CEIL(a, b) (((a) + (b) - 1) / (b))

s32 fn_80081794(JPEGDecContext* ctx) {
    JPEGDecHandle* h = ctx->handle;

    switch (h->scale) {
    case 1:
        ctx->coefLimit = 64;
        ctx->coefLimit2 = 64;
        ctx->idctY = fn_8008082C;
        ctx->idctC = fn_80080C30;
        ctx->decodeBlock = fn_800898F4;
        ctx->blockSize = 8;
        break;
    case 2:
        ctx->coefLimit = 25;
        ctx->coefLimit2 = 108;
        ctx->idctY = fn_80083554;
        ctx->idctC = fn_800838D4;
        ctx->decodeBlock = fn_80089D2C;
        ctx->blockSize = 4;
        break;
    case 4:
        ctx->coefLimit = 5;
        ctx->coefLimit2 = 36;
        ctx->idctY = fn_80083774;
        ctx->idctC = fn_80083AF8;
        ctx->decodeBlock = fn_80089D2C;
        ctx->blockSize = 2;
        break;
    case 8:
        ctx->coefLimit = 1;
        ctx->coefLimit2 = 0;
        ctx->idctY = fn_80083890;
        ctx->idctC = fn_80083C1C;
        ctx->decodeBlock = fn_80089D2C;
        ctx->blockSize = 1;
        break;
    default:
        return -0x70;
    }

    h->sampling = ctx->frame.sampling;
    h->numMcus = ctx->frame.numMcus;
    h->mcusX = ctx->frame.mcusX;
    h->mcusY = ctx->frame.mcusY;
    h->mcuWidth = DIV_CEIL(ctx->frame.mcuWidth, h->scale);
    h->mcuHeight = DIV_CEIL(ctx->frame.mcuHeight, h->scale);
    h->width = DIV_CEIL(ctx->frame.width, h->scale);
    h->height = DIV_CEIL(ctx->frame.height, h->scale);
    h->remX = DIV_CEIL(ctx->frame.remX, h->scale);
    h->remY = DIV_CEIL(ctx->frame.remY, h->scale);
    if (h->remX != 0) {
        h->lastX = (h->mcusX - 1) * h->mcuWidth;
    } else {
        h->lastX = (h->mcusX + 1) * h->mcuWidth;
    }
    if (h->remY != 0) {
        h->lastY = (h->mcusY - 1) * h->mcuHeight;
    } else {
        h->lastY = (h->mcusY + 1) * h->mcuHeight;
    }

    switch (ctx->frame.sampling) {
    case 0:
        h->cWidth = (h->width + 3) / 4;
        h->cHeight = h->height;
        break;
    case 1:
        h->cWidth = (h->width + 1) / 2;
        h->cHeight = h->height;
        break;
    case 2:
        h->cWidth = (h->width + 1) / 2;
        h->cHeight = (h->height + 1) / 2;
        break;
    case 3:
        h->cWidth = h->width;
        h->cHeight = h->height;
        break;
    case 4:
        break;
    default:
        return -0x70;
    }
    return 0;
}


s32 fn_80081AA8(u32 x, u32 y, JPEGDecContext* ctx, s32* work) {
    s32 coef[64];
    JPEGDecHandle* h;
    u8* scan;
    s32* dcPred;
    JPEGTables* tables;
    u8** out;
    JPEGFrame* frame;
    JPEGBlockFunc decodeBlock;
    JPEGIdctFunc idctY;
    JPEGIdctFunc idctC;
    u16 stride;
    s32 i;
    s32 j;
    u8 comp;
    s32* quant;
    s32 ret;
    u8** pOut;

    scan = ctx->scanComp;
    frame = &ctx->frame;
    tables = &ctx->tables;
    out = ctx->blockOut;
    dcPred = ctx->dcPred;
    i = 0;
    h = ctx->handle;
    decodeBlock = ctx->decodeBlock;
    idctY = ctx->idctY;
    idctC = ctx->idctC;
    stride = ctx->stride;
    for (; i < frame->scanComps; i++, dcPred++, scan++) {
        comp = *scan;
        quant = tables->quant[ctx->quantSel[comp]];
        fn_80083000(tables, ctx->dcSel[comp], ctx->acSel[comp]);
        pOut = out;
        for (j = 0; j < frame->blocks[i]; j++, pOut++) {
            ret = decodeBlock(coef, quant, dcPred, ctx);
            if (ret < 0) {
                return ret;
            }
            if (comp == 0) {
                idctY(coef, *pOut, stride, ret);
            } else {
                idctC(coef, out[comp + 4], stride, ret);
            }
        }
    }

    if (x != h->lastX && y != h->lastY) {
        ctx->output(ctx, x, y);
    } else {
        ctx->outputEdge(ctx, x, y);
    }
    if (frame->restartInterval != 0) {
        ret = fn_8008218C(ctx, x, y);
        if (ret < 0) {
            return ret;
        }
    }
    return 0;
}
