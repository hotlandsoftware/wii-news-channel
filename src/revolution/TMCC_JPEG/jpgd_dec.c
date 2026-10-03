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


s32 jpgdSetupScale(JPEGDecContext* ctx) {

    JPEGDecHandle* h = ctx->handle;

    switch (h->scale) {
    case 1:
        ctx->coefLimit = 64;
        ctx->coefLimit2 = 64;
        ctx->idctY = jpgdIdct8x8Y;
        ctx->idctC = jpgdIdct8x8C;
        ctx->decodeBlock = jpgdDecodeBlock;
        ctx->blockSize = 8;
        break;
    case 2:
        ctx->coefLimit = 25;
        ctx->coefLimit2 = 108;
        ctx->idctY = jpgdIdct4x4Y;
        ctx->idctC = jpgdIdct4x4C;
        ctx->decodeBlock = jpgdDecodeBlockScaled;
        ctx->blockSize = 4;
        break;
    case 4:
        ctx->coefLimit = 5;
        ctx->coefLimit2 = 36;
        ctx->idctY = jpgdIdct2x2Y;
        ctx->idctC = jpgdIdct2x2C;
        ctx->decodeBlock = jpgdDecodeBlockScaled;
        ctx->blockSize = 2;
        break;
    case 8:
        ctx->coefLimit = 1;
        ctx->coefLimit2 = 0;
        ctx->idctY = jpgdIdct1x1Y;
        ctx->idctC = jpgdIdct1x1C;
        ctx->decodeBlock = jpgdDecodeBlockScaled;
        ctx->blockSize = 1;
        break;
    default:
        return -0x70;
    }

    h->sampling = ctx->frame.sampling;
    h->numMcus = ctx->frame.numMcus;
    h->mcusX = ctx->frame.mcusX;
    h->mcusY = ctx->frame.mcusY;
    h->mcuWidth = (h->scale + ctx->frame.mcuWidth - 1) / h->scale;
    h->mcuHeight = (h->scale + ctx->frame.mcuHeight - 1) / h->scale;
    h->width = (ctx->frame.width + h->scale - 1) / h->scale;
    h->height = (h->scale + ctx->frame.height - 1) / h->scale;
    h->remX = (h->scale + ctx->frame.remX - 1) / h->scale;
    h->remY = (h->scale + ctx->frame.remY - 1) / h->scale;
    h->lastX = (h->remX != 0) ? (h->mcusX - 1) * h->mcuWidth : (h->mcusX + 1) * h->mcuWidth;
    h->lastY = (h->remY != 0) ? (h->mcusY - 1) * h->mcuHeight : (h->mcusY + 1) * h->mcuHeight;

    switch (ctx->frame.sampling) {
    case 3:
        h->cWidth = h->width;
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
    case 0:
        h->cWidth = (h->width + 3) / 4;
        h->cHeight = h->height;
        break;
    case 4:
        break;
    default:
        return -0x70;
    }
    return 0;
}


s32 jpgdDecodeMcu(u32 x, u32 y, JPEGDecContext* ctx, s32* work) {
    s32* dcPred;
    u8* scan;
    JPEGDecHandle* h;
    u8** outC;
    JPEGScan* sc;
    u8** pOut;
    s32* quant;
    s32 i;
    s32 ret;
    s32 comp;
    s32 j;
    s32 coef[64];
    u16 stride;
    JPEGFrame* frame;
    u8* nBlocks;
    JPEGTables* tables;
    u8** out;
    JPEGIdctFunc idctY;
    JPEGIdctFunc idctC;
    JPEGBlockFunc decodeBlock;

    sc = &ctx->scan;
    frame = &ctx->frame;
    scan = sc->comp;
    tables = &ctx->tables;
    out = ctx->blockOut;
    dcPred = sc->dcPred;
    i = 0;
    h = ctx->handle;
    decodeBlock = ctx->decodeBlock;
    idctY = ctx->idctY;
    idctC = ctx->idctC;
    stride = ctx->stride;
    for (; i < frame->scanComps; dcPred++, i++, scan++) {
        comp = *scan;
        quant = tables->quant[sc->quantSel[comp]];
        jpgdSelectHuffTables(tables, sc->dcSel[comp], sc->acSel[comp]);
        pOut = out;
        outC = &out[comp];
        nBlocks = &frame->blocks[i];
        for (j = 0; j < *nBlocks; pOut++, j++) {
            ret = decodeBlock(coef, quant, dcPred, ctx);
            if (ret < 0) {
                return ret;
            }
            if (comp == 0) {
                idctY(coef, *pOut, stride, ret);
            } else {
                idctC(coef, outC[4], stride, ret);
            }
        }
    }

    if (x != h->lastX && y != h->lastY) {
        ctx->output(ctx, x, y);
    } else {
        ctx->outputEdge(ctx, x, y);
    }
    if (frame->restartInterval != 0) {
        ret = jpgdCheckRestart(ctx, x, y);
        if (ret < 0) {
            return ret;
        }
    }
    return 0;
}

static s32 jpgdParseSOF(JPEGDecContext* ctx) {
    s32 ret = jpgdReadSOF(ctx);
    if (ret < 0) {
        return ret;
    }
    return 0;
}

s32 jpgdReadHeader(JPEGDecContext* ctx) {
    s32 ret;

    const u8* zz = jpgdZigzag;
    u16 marker;
    s32 i;

    ctx->frame.restartInterval = 0;
    ctx->frame.eoi = 0;
    for (i = 0; i < 64; i++) {
        ctx->tables.zigzag[i] = *zz++ * 4;
    }
    ret = jpgdGetWord(&marker, &ctx->stream);
    if (ret < 0) {
        return ret;
    }
    if (marker != 0xFFD8) {
        return -0x20;
    }
    marker = 0;
    ret = jpgdNextMarker(&marker, ctx);
    if (ret < 0) {
        return ret;
    }
    if (marker != 0xFFC0) {
        return -0x10;
    }
    return jpgdParseSOF(ctx);
}

s32 jpgdCheckEOI(JPEGDecContext* ctx) {
    u16 marker;
    s32 ret;

    if (ctx->handle->noEoiCheck == 1) {
        return 0;
    }
    if (ctx->frame.eoi == 0) {
        if (ctx->stream.bitsEnd == 0) {
            ret = jpgdUnreadBits(&ctx->stream);
            if (ret < 0) {
                return ret;
            }
            ret = jpgdGetWord(&marker, &ctx->stream);
            if (ret < 0) {
                return ret;
            }
        } else {
            ret = jpgdGetWord(&marker, &ctx->stream);
            if (ret < 0) {
                return ret;
            }
        }
        if (marker != 0xFFD9) {
            ctx->handle->error = -0x21;
            return 0;
        }
    }
    return 0;
}

s32 jpgdStartScan(JPEGDecContext* ctx) {
    u16 marker;
    s32 ret;

    marker = 0;
    ret = jpgdNextMarker(&marker, ctx);
    if (ret < 0) {
        return ret;
    }
    if (marker == 0xFFD9) {
        return 0;
    }
    if (marker != 0xFFDA) {
        return -0x22;
    }
    ret = jpgdReadSOS(ctx);
    if (ret < 0) {
        return ret;
    }
    ret = jpgdSetupMcu(ctx);
    if (ret < 0) {
        return ret;
    }
    ctx->scan.dcPred[0] = 0;
    ctx->scan.dcPred[1] = 0;
    ctx->scan.dcPred[2] = 0;
    ctx->scan.dcPred[3] = 0;
    ctx->scan.restartCount = 0;
    ret = jpgdResetBits(&ctx->stream);
    if (ret < 0) {
        return ret;
    }
    ctx->scan.nextRestart = 0;
    ctx->scan.mcuPos = 0;
    return 0;
}

static void jpgdCalcMcus(JPEGFrame* f) {
    f->remX = f->width % f->mcuWidth;
    f->remY = f->height % f->mcuHeight;
    f->mcusX += (f->remX != 0);
    f->mcusY += (f->remY != 0);
    f->numMcus = f->mcusX * f->mcusY;
}

s32 jpgdSetupMcu(JPEGDecContext* ctx) {
    s32 i;
    JPEGFrame* f = &ctx->frame;

    ctx->frame.sampling |= 0x100;
    if (ctx->frame.scanComps == 1) {
        u8 c = ctx->scan.comp[0];
        f->mcuWidth = (f->maxH * 8) / f->hSamp[c];
        f->mcuHeight = (f->maxV * 8) / f->vSamp[c];
        f->mcusX = f->width / f->mcuWidth;
        f->mcusY = f->height / f->mcuHeight;
    } else {
        f->mcuWidth = f->maxH * 8;
        f->mcuHeight = f->maxV * 8;
        f->mcusX = f->width / f->mcuWidth;
        f->mcusY = f->height / f->mcuHeight;
    }
    jpgdCalcMcus(f);

    for (i = 0; i < f->scanComps; i++) {
        u8 c = ctx->scan.comp[i];
        f->blocks[i] = (f->scanComps == 1) ? 1 : f->hSamp[c] * f->vSamp[c];
    }
    for (; i < 4; i++) {
        f->blocks[i] = 0;
    }
    return 0;
}

s32 jpgdCheckRestart(JPEGDecContext* ctx, u32 x, u32 y) {
    JPEGDecHandle* h;
    u16 marker;
    u16 n;
    u16 mcusX;
    u16 row;
    u16 col;
    s32 ret;

    ctx->scan.restartCount++;
    h = ctx->handle;
    ret = 0;
    if (ctx->scan.restartCount == ctx->frame.restartInterval) {
        ret = jpgdUnreadBits(&ctx->stream);
        if (ret < 0) {
            return ret;
        }
        ret = jpgdGetWord(&marker, &ctx->stream);
        if (ret < 0) {
            return ret;
        }
        if (marker >= 0xFFC0 && (marker < 0xFFD0 || marker > 0xFFD7)) {
            ret = jpgdSkipBytes(-2, &ctx->stream);
            if (ret < 0) {
                return ret;
            }
        } else if (marker != ctx->scan.nextRestart + 0xFFD0) {
            return -0x23;
        }
        ctx->scan.nextRestart = (ctx->scan.nextRestart + 1) & 7;
        mcusX = h->mcusX;
        row = y / h->mcuHeight;
        col = x / h->mcuWidth;
        n = col + row * mcusX + 1;
        ctx->scan.dcPred[0] = 0;
        ctx->scan.dcPred[1] = 0;
        ctx->scan.dcPred[2] = 0;
        ctx->scan.dcPred[3] = 0;
        ctx->scan.restartCount = 0;
        ctx->scan.mcuPos = ((n % mcusX) << 16) + n / mcusX;
        ret = jpgdResetBits(&ctx->stream);
    }
    return ret;
}

static s32 jpgdSkip(s32 n, JPEGStream* s) {
    s32 ret = jpgdSkipBytes(n, s);
    if (ret < 0) {
        return ret;
    }
    return 0;
}

static s32 jpgdSkipAPP(JPEGDecContext* ctx) {
    u16 len;
    s32 ret = jpgdGetWord(&len, &ctx->stream);
    if (ret < 0) {
        return ret;
    }
    if (len < 2) {
        return -0x45;
    }
    len -= 2;
    ret = jpgdSkip(len, &ctx->stream);
    if (ret < 0) {
        return ret;
    }
    return 0;
}

static s32 jpgdReadDRI(JPEGDecContext* ctx) {
    u16 v;
    s32 ret = jpgdGetWord(&v, &ctx->stream);
    if (ret < 0) {
        return ret;
    }
    if (v != 4) {
        return -0x42;
    }
    ret = jpgdGetWord(&v, &ctx->stream);
    if (ret < 0) {
        return ret;
    }
    ctx->frame.restartInterval = v;
    return 0;
}

static s32 jpgdReadDNL(JPEGDecContext* ctx) {
    u16 v;
    s32 ret = jpgdGetWord(&v, &ctx->stream);
    if (ret < 0) {
        return ret;
    }
    if (v != 4) {
        return -0x43;
    }
    ret = jpgdGetWord(&v, &ctx->stream);
    if (ret < 0) {
        return ret;
    }
    ctx->frame.height = v;
    return 0;
}

static s32 jpgdSkipCOM(JPEGDecContext* ctx) {
    u16 len;
    s32 ret = jpgdGetWord(&len, &ctx->stream);
    if (ret < 0) {
        return ret;
    }
    if (len < 2) {
        return -0x44;
    }
    len -= 2;
    ret = jpgdSkip(len, &ctx->stream);
    if (ret < 0) {
        return ret;
    }
    return 0;
}

s32 jpgdNextMarker(u16* outMarker, JPEGDecContext* ctx) {
    u16 marker;
    u8 c;
    BOOL done;
    u16 first;
    s32 ret;

    done = FALSE;
    first = *outMarker;
    do {
        ret = jpgdGetWord(&marker, &ctx->stream);
        if (ret < 0) {
            return ret;
        }
        while (marker == 0xFFFF) {
            ret = jpgdGetByte(&c, &ctx->stream);
            if (ret < 0) {
                return ret;
            }
            marker = c | 0xFF00;
            if (ctx->stream.cur >= ctx->stream.end && ctx->stream.remain == 0) {
                break;
            }
        }
        if (marker >= 0xFFE0 && marker <= 0xFFEF) {
            ret = jpgdSkipAPP(ctx);
        } else {
            switch (marker) {
            case 0xFFC4:
                ret = jpgdReadDHT(first, ctx);
                break;
            case 0xFFDB:
                ret = jpgdReadDQT(ctx);
                break;
            case 0xFFDD:
                ret = jpgdReadDRI(ctx);
                break;
            case 0xFFDC:
                ret = jpgdReadDNL(ctx);
                break;
            case 0xFFFE:
                ret = jpgdSkipCOM(ctx);
                break;
            case 0xFFC0:
                done = TRUE;
                break;
            case 0xFFC2:
                done = TRUE;
                break;
            case 0xFFDA:
                done = TRUE;
                break;
            case 0xFFD9:
                ctx->frame.eoi = 1;
                done = TRUE;
                break;
            default:
                ret = -0x2F;
                break;
            }
        }
        if (ret < 0) {
            done = TRUE;
        }
    } while (!done);
    *outMarker = marker;
    return ret;
}

s32 jpgdReadDHT(u16 marker, JPEGDecContext* ctx) {
    u8 vals[256];
    u8 count;
    u8* p;
    s32 cls;
    s32 id;
    s32 ret;
    u16 len;
    u8 bits[17];
    JPEGTables* tables;
    u8 c;
    s32 i;
    JPEGHuffTable table;

    tables = &ctx->tables;
    memset(bits, 0, sizeof(bits));
    ret = jpgdGetWord(&len, &ctx->stream);
    if (ret < 0) {
        return ret;
    }
    len -= 2;
    do {
        len -= 17;
        ret = jpgdGetByte(&c, &ctx->stream);
        if (ret < 0) {
            return ret;
        }
        cls = c >> 4;
        id = c & 0xF;
        if (cls >= 2 || id >= 2) {
            return -0x40;
        }
        bits[0] = 0;
        p = &bits[1];
        for (i = 1; i <= 16; i++) {
            ret = jpgdGetByte(&c, &ctx->stream);
            if (ret < 0) {
                return ret;
            }
            *p++ = c;
            len -= c;
        }
        count = 0;
        for (i = 1; i <= 16; i++) {
            count = count + bits[i];
        }
        ret = jpgdGetBytes(vals, count, &ctx->stream);
        if (ret < 0) {
            return ret;
        }
        table.count = count;
        jpgdInitHuffTable(&table, cls, id, tables);
        ret = jpgdBuildHuffTable(bits, vals, &table);
        if (ret < 0) {
            return ret;
        }
    } while (len != 0);
    return 0;
}

s32 jpgdReadDQT(JPEGDecContext* ctx) {
    s32 scale[64] = {
        256, 185, 196, 218, 256, 326, 473, 928,
        185, 133, 141, 157, 185, 235, 341, 669,
        196, 141, 150, 167, 196, 249, 362, 710,
        218, 157, 167, 185, 218, 277, 402, 789,
        256, 185, 196, 218, 256, 326, 473, 928,
        326, 235, 249, 277, 326, 415, 602, 1181,
        473, 341, 362, 402, 473, 602, 874, 1714,
        928, 669, 710, 789, 928, 1181, 1714, 3363,
    };
    u8 id;
    u8 c;
    u16 len;
    JPEGTables* tables;
    const u8* zz;
    s32 i;
    s32 pos;
    s32 q;
    s32 ret;

    tables = &ctx->tables;
    ret = jpgdGetWord(&len, &ctx->stream);
    if (ret < 0) {
        return ret;
    }
    len -= 2;
    do {
        len -= 65;
        ret = jpgdGetByte(&id, &ctx->stream);
        if (ret < 0) {
            return ret;
        }
        if (id > 4) {
            return -0x41;
        }
        tables->quantDefined[id] = 1;
        zz = jpgdZigzag;
        for (i = 0; i < 64; i++) {
            pos = *zz;
            ret = jpgdGetByte(&c, &ctx->stream);
            if (ret < 0) {
                return ret;
            }
            q = c * scale[pos];
            tables->quant[id][pos] = q;
            if (q == 0) {
                return -0x41;
            }
            zz++;
        }
    } while (len != 0);
    return 0;
}

static s32 jpgdCheckComps(JPEGDecContext* ctx) {
    JPEGFrame* f = &ctx->frame;
    JPEGScan* sc = &ctx->scan;
    s32 i;

    for (i = 0; i < ctx->frame.numComps; i++) {
        if (f->hSamp[i] < 1 || f->hSamp[i] > 4) {
            return -0x50;
        }
        if (f->vSamp[i] < 1 || f->vSamp[i] > 4) {
            return -0x50;
        }
        if (sc->quantSel[i] > 4) {
            return -0x50;
        }
    }
    return 0;
}

s32 jpgdReadSOF(JPEGDecContext* ctx) {
    const u8* a;
    s32 maxH;
    const u8* pn;
    s32 ret;
    s32 i;
    s32 hh;
    const u8* ph;
    const u8* pv;
    u8 c;
    const u8* bb;
    u16 maxV;
    u16 v;
    JPEGFrame* f;
    s32 t;
    JPEGScan* sc;
    s32 j;

    f = &ctx->frame;
    sc = &ctx->scan;
    ret = jpgdGetWord(&v, &ctx->stream);
    if (ret < 0) {
        return ret;
    }
    if (v < 2) {
        return -0x50;
    }
    ret = jpgdGetByte(&c, &ctx->stream);
    if (ret < 0) {
        return ret;
    }
    if (c != 8) {
        return -0x50;
    }
    ret = jpgdGetWord(&v, &ctx->stream);
    if (ret < 0) {
        return ret;
    }
    f->height = v;
    ret = jpgdGetWord(&v, &ctx->stream);
    if (ret < 0) {
        return ret;
    }
    f->width = v;
    ret = jpgdGetByte(&c, &ctx->stream);
    if (ret < 0) {
        return ret;
    }
    {
        s32 n = c;
        f->numComps = n;
        if (n <= 0 || n > 4) {
            return -0x50;
        }
    }

    maxV = 0;
    maxH = 0;
    for (i = 0; i < f->numComps; i++) {
        u8 b;
        u8 h;
        ret = jpgdGetByte(&c, &ctx->stream);
        if (ret < 0) {
            return ret;
        }
        sc->compId[i] = c;
        ret = jpgdGetByte(&c, &ctx->stream);
        if (ret < 0) {
            return ret;
        }
        b = c;
        hh = b >> 4;
        f->hSamp[i] = hh;
        f->vSamp[i] = b & 0xF;
        if (hh > maxH) {
            maxH = hh;
        }
        if (f->vSamp[i] > maxV) {
            maxV = f->vSamp[i];
        }
        ret = jpgdGetByte(&c, &ctx->stream);
        if (ret < 0) {
            return ret;
        }
        sc->quantSel[i] = c;
    }
    if ((u16)maxH == 0 || maxV == 0) {
        return -0x50;
    }

    f->maxH = maxH;
    f->maxV = maxV;
    f->sampling = 5;
    pn = jpgdSampComps;
    ph = jpgdSampTableH;
    pv = jpgdSampTableV;
    for (t = 0; t < 5; t++) {
        if (f->numComps == *pn) {
            a = ph;
            bb = pv;
            for (j = 0; j < f->numComps; j++) {
                if (f->hSamp[j] != *a || f->vSamp[j] != *bb) {
                    goto next;
                }
                bb++;
                a++;
            }
            f->sampling = t;
        }
    next:
        ph += 4;
        pv += 4;
        pn++;
    }
    if (f->sampling == 5) {
        return -0x70;
    }
    f->mcuWidth = maxH * 8;
    f->mcuHeight = maxV * 8;
    f->mcusX = f->width / f->mcuWidth;
    f->mcusY = f->height / f->mcuHeight;
    jpgdCalcMcus(f);
    return jpgdCheckComps(ctx);
}

static s32 jpgdSkip3(JPEGStream* s) {
    s32 ret = jpgdSkipBytes(3, s);
    if (ret < 0) {
        return ret;
    }
    return 0;
}

s32 jpgdReadSOS(JPEGDecContext* ctx) {
    s32 dc;
    u16 len;
    s32 i;
    JPEGScan* sc;
    s32 ret;
    s32 j;
    u8 c;
    s32 ac;
    JPEGTables* tables;

    sc = &ctx->scan;
    tables = &ctx->tables;
    ret = jpgdGetWord(&len, &ctx->stream);
    if (ret < 0) {
        return ret;
    }
    if (len < 2) {
        return -0x51;
    }
    ret = jpgdGetByte(&c, &ctx->stream);
    if (ret < 0) {
        return ret;
    }
    ctx->frame.scanComps = c;
    if (ctx->frame.scanComps > 4 || ctx->frame.scanComps != ctx->frame.numComps) {
        return -0x51;
    }
    for (i = 0; i < ctx->frame.scanComps; i++) {
        ret = jpgdGetByte(&c, &ctx->stream);
        if (ret < 0) {
            return ret;
        }
        for (j = 0; j < ctx->frame.numComps; j++) {
            if ((s32)c == sc->compId[j]) {
                sc->comp[i] = j;
                goto found;
            }
        }
        return -0x51;
    found:
        ret = jpgdGetByte(&c, &ctx->stream);
        if (ret < 0) {
            return ret;
        }
        dc = c >> 4;
        ac = c & 0xF;
        if (dc > 1 || ac > 1) {
            return -0x51;
        }
        sc->dcSel[sc->comp[i]] = dc;
        sc->acSel[sc->comp[i]] = ac;
        if (tables->dcDefined[dc] != 1) {
            return -0x40;
        }
        if (tables->acDefined[ac] != 1) {
            return -0x40;
        }
        if (tables->quantDefined[sc->quantSel[i]] != 1) {
            return -0x41;
        }
    }
    return jpgdSkip3(&ctx->stream);
}

s32 jpgdResync(JPEGDecContext* ctx) {
    s32 n;
    u16 x;
    u8 c;
    u16 mcusX;
    u16 y;
    s32 ret;
    JPEGDecHandle* h;
    u32 pos;
    u8 skip;

    h = ctx->handle;
    ret = jpgdUnreadBits(&ctx->stream);
    if (ret < 0) {
        return ret;
    }
    c = *ctx->stream.cur;
    while (1) {
        while (c != 0xFF) {
            ret = jpgdGetByte(&c, &ctx->stream);
            if (ret < 0) {
                return ret;
            }
            if (ctx->stream.cur > ctx->stream.end) {
                break;
            }
        }
        ret = jpgdGetByte(&c, &ctx->stream);
        if (ret < 0) {
            return ret;
        }
        if (ctx->stream.cur > ctx->stream.end) {
            return 0;
        }
        if (c >= 0xD0 && c <= 0xD7) {
            break;
        }
    }

    {
        s32 expect = ctx->scan.nextRestart + 0xCF;
        skip = c + 8 - expect;
        if (c > expect) {
            skip = c - expect;
        }
        skip *= ctx->frame.restartInterval;
        ctx->scan.nextRestart = (c + 1) & 7;
        mcusX = h->mcusX;
        pos = ctx->scan.mcuPos;
        ctx->scan.dcPred[0] = 0;
        ctx->scan.dcPred[1] = 0;
        ctx->scan.dcPred[2] = 0;
        ctx->scan.dcPred[3] = 0;
        ctx->scan.restartCount = 0;
        n = (u8)pos * mcusX + (skip + (pos >> 16));
        y = n / mcusX;
        x = n % mcusX;
        ctx->scan.mcuPos = (x << 16) + y;
        ret = jpgdResetBits(&ctx->stream);
        if (ret < 0) {
            return ret;
        }
        h->mcuX = x;
        h->mcuY = y;
        h->readSize = jpgdGetReadSize(&ctx->stream);
        return h->numMcus - h->mcuY * h->mcusX - h->mcuX;
    }
}

void jpgdSelectHuffTables(JPEGTables* t, s32 dc, s32 ac) {
    switch (dc) {
    case 0:
        t->dc.lookup = t->dcLookup[0];
        t->dc.vals = t->dcVals[0];
        t->dc.codes = t->dcCodes[0];
        break;
    case 1:
        t->dc.lookup = t->dcLookup[1];
        t->dc.vals = t->dcVals[1];
        t->dc.codes = t->dcCodes[1];
        break;
    }
    switch (ac) {
    case 0:
        t->ac.lookup = t->acLookup[0];
        t->ac.vals = t->acVals[0];
        t->ac.codes = t->acCodes[0];
        break;
    case 1:
        t->ac.lookup = t->acLookup[1];
        t->ac.vals = t->acVals[1];
        t->ac.codes = t->acCodes[1];
        break;
    }
}

s32 jpgdBuildHuffTable(u8* bits, u8* vals, JPEGHuffTable* t) {
    u32* ps;
    JPEGHuffLookup* lookup;
    JPEGHuffCode* codes;
    s32 shift;
    u8* dstVals;
    s32 k;
    u32 huffsize[256];
    s32 i;
    u32 huffcode[256];
    s32 m;
    s32 l;
    u16 code;
    s32 j;
    u8 si;
    s32 n;
    u8 count;
    s32 base;

    lookup = t->lookup;
    codes = t->codes;
    dstVals = t->vals;
    count = t->count;

    i = 1;
    j = 1;
    ps = huffsize;
    k = 0;
    for (;;) {
        if (j > bits[i]) {
            i++;
            j = 1;
            if (i <= 16) {
                continue;
            }
            huffsize[k] = 0;
            break;
        }
        *ps++ = i;
        k++;
        j++;
    }

    ps = huffsize;
    code = 0;
    m = 0;
    si = huffsize[0];
    while (*ps != 0) {
        while ((u8)*ps == si) {
            ps++;
            huffcode[m++] = code;
            code++;
        }
        if (code >= (1 << si)) {
            return -0x40;
        }
        code <<= 1;
        si++;
    }

    memset(codes, 0, sizeof(JPEGHuffCode) * 17);
    for (i = 0; i < count; i++) {
        if (huffsize[i] <= 16) {
            JPEGHuffCode* c = &codes[huffsize[i]];
            c->index = i;
            c->code = huffcode[i];
        }
    }

    i = 0;
    for (j = 1; j <= 8; j++) {
        shift = 8 - j;
        for (m = 1; m <= bits[j]; m++, i++) {
            n = 1 << shift;
            base = huffcode[i] << shift;
            for (; n > 0; n--) {
                lookup[base].len = j;
                lookup[base].val = vals[i];
                base++;
            }
        }
    }
    memcpy(dstVals, vals, count);
    return 0;
}

void jpgdInitHuffTable(JPEGHuffTable* t, s32 cls, s32 id, JPEGTables* tables) {
    if (cls == 0) {
        switch (id) {
        case 0:
            t->lookup = tables->dcLookup[0];
            t->vals = tables->dcVals[0];
            t->codes = tables->dcCodes[0];
            tables->dcDefined[0] = 1;
            memset(tables->dcLookup[0], 0, sizeof(tables->dcLookup[0]));
            memset(tables->dcVals[0], 0, sizeof(tables->dcVals[0]));
            memset(tables->dcCodes[0], 0, sizeof(tables->dcCodes[0]));
            break;
        case 1:
            t->lookup = tables->dcLookup[1];
            t->vals = tables->dcVals[1];
            t->codes = tables->dcCodes[1];
            tables->dcDefined[1] = 1;
            memset(tables->dcLookup[1], 0, sizeof(tables->dcLookup[1]));
            memset(tables->dcVals[1], 0, sizeof(tables->dcVals[1]));
            memset(tables->dcCodes[1], 0, sizeof(tables->dcCodes[1]));
            break;
        }
    } else {
        switch (id) {
        case 0:
            t->lookup = tables->acLookup[0];
            t->vals = tables->acVals[0];
            t->codes = tables->acCodes[0];
            tables->acDefined[0] = 1;
            memset(tables->acLookup[0], 0, sizeof(tables->acLookup[0]));
            memset(tables->acVals[0], 0, sizeof(tables->acVals[0]));
            memset(tables->acCodes[0], 0, sizeof(tables->acCodes[0]));
            break;
        case 1:
            t->lookup = tables->acLookup[1];
            t->vals = tables->acVals[1];
            t->codes = tables->acCodes[1];
            tables->acDefined[1] = 1;
            memset(tables->acLookup[1], 0, sizeof(tables->acLookup[1]));
            memset(tables->acVals[1], 0, sizeof(tables->acVals[1]));
            memset(tables->acCodes[1], 0, sizeof(tables->acCodes[1]));
            break;
        }
    }
}

