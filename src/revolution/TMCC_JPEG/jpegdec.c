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

static s32 jpgdParseHeader(JPEGDecContext* ctx) {
    s32 ret = jpgdReadHeader(ctx);
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

static s32 jpgdBeginScan(JPEGDecContext* ctx) {
    s32 ret = jpgdStartScan(ctx);
    if (ret < 0) {
        return ret;
    }
    if (ctx->frame.eoi == 1) {
        return 0;
    }
    return ret;
}

s32 JPEGDecInit(JPEGDecHandle* h, JPEGDecParam* p) {
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
        ret = jpgdStreamInit(&ctx->stream, &p->source);
        if (ret < 0) {
            return ret;
        }
        ret = jpgdParseHeader(ctx);
        if (ret >= 0) {
            ret = jpgdBeginScan(ctx);
            if (ret >= 0 && ctx->frame.eoi == 0) {
                ret = jpgdSetupScale(ctx);
                if (ret >= 0) {
                    switch (h->format) {
                    case 0:
                        ret = jpgdSetupOutputRGB565(ctx);
                        if (ret < 0) {
                            goto error;
                        }
                        break;
                    case 1:
                        ret = jpgdSetupOutputRGBA8(ctx);
                        if (ret < 0) {
                            goto error;
                        }
                        break;
                    case 2:
                        ret = jpgdSetupOutputYUV(ctx);
                        if (ret < 0) {
                            goto error;
                        }
                        break;
                    }
                    h->readSize = jpgdGetReadSize(&ctx->stream);
                    return h->numMcus;
                }
            }
        }
    }
error:
    return ret < 0 ? ret : -2;
}

s32 JPEGDecDecode(JPEGDecHandle* h, s32 count, void* out) {
    s32 work[95];
    u32 py;
    JPEGDecContext* ctx = h->ctx;
    u16 x;
    u16 mcusX;
    u16 y;
    u16 mcusY;
    u8 mcuW;
    u8 mcuH;
    s32 n;
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
        py = y * mcuH;
        while (x < mcusX && n < count) {
            ret = jpgdDecodeMcu(x * mcuW, py, ctx, work);
            if (ret < 0) {
                goto error;
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
        ret = jpgdCheckEOI(ctx);
        if (ret < 0) {
            return ret;
        }
    }
    h->mcuX = x;
    h->mcuY = y;
    h->readSize = jpgdGetReadSize(&ctx->stream);
    return h->numMcus - y * mcusX - x;

error:
    if (ctx->frame.restartInterval != 0 && ret != -0xF0) {
        h->error = ret;
        ret = jpgdResync(ctx);
    }
    return ret;
}

s32 JPEGDecSetScale(JPEGDecHandle* h, s32 scale) {
    JPEGDecContext* ctx;
    s32 ret;

    h->scale = scale;
    ctx = h->ctx;
    if ((u8)scale != 1 && (u8)scale != 2 && (u8)scale != 4 && (u8)scale != 8) {
        return -1;
    }
    ret = jpgdSetupScale(ctx);
    if (ret < 0) {
        return ret;
    }
    switch (h->format) {
    case 0:
        ret = jpgdSetupOutputRGB565(ctx);
        if (ret < 0) {
            return ret;
        }
        break;
    case 1:
        ret = jpgdSetupOutputRGBA8(ctx);
        if (ret < 0) {
            return ret;
        }
        break;
    case 2:
        ret = jpgdSetupOutputYUV(ctx);
        if (ret < 0) {
            return ret;
        }
        break;
    }
    return 0;
}
