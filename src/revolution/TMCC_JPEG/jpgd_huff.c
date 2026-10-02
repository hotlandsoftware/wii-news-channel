#include <string.h>
#include "jpgd_internal.h"

const u8 lbl_801AADF0[20] = {
    4, 1, 1, 0,
    2, 1, 1, 0,
    2, 1, 1, 0,
    1, 1, 1, 0,
    1, 0, 0, 0,
};

const u8 lbl_801AAE04[20] = {
    1, 1, 1, 0,
    1, 1, 1, 0,
    2, 1, 1, 0,
    1, 1, 1, 0,
    1, 0, 0, 0,
};

const u8 lbl_801AAE18[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
};

const s32 lbl_801AAE58[64] = {
    0x11, 0x12, 0x22, 0x32, 0x32, 0x33, 0x34, 0x34,
    0x34, 0x44, 0x54, 0x54, 0x54, 0x54, 0x55, 0x56,
    0x56, 0x56, 0x56, 0x56, 0x66, 0x76, 0x76, 0x76,
    0x76, 0x76, 0x76, 0x77, 0x78, 0x78, 0x78, 0x78,
    0x78, 0x78, 0x78, 0x88, 0x88, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
};

const u8 lbl_80359068[5] = {3, 3, 3, 3, 1};

#define FILL_BITS(s, n)                     \
    if ((s)->numBits <= (n)) {              \
        ret = fn_80080234(s);               \
        if (ret < 0) {                      \
            return ret;                     \
        }                                   \
    }

static inline s32 jpgdHuffDecodeSlow(JPEGHuffCode* codes, u8* vals, JPEGStream* s) {
    JPEGHuffCode* c;
    s32 ret;
    u32 code;
    JPEGHuffCode hc;
    JPEGHuffCode hc2;
    u32 l;
    u32 idx;

    FILL_BITS(s, 17);
    s->numBits -= 9;
    code = (s->bits >> s->numBits) & 0x1FF;
    c = &codes[9];
    l = 9;
    while (hc = *c, hc2 = hc, code > hc2.code) {
        l++;
        c++;
        if (l > 16) {
            return -0x64;
        }
        s->numBits--;
        code <<= 1;
        code |= (s->bits >> s->numBits) & 1;
    }
    idx = code - hc.code;
    idx += hc.index;
    return vals[idx];
}

s32 fn_800898F4(s32* coef, s32* quant, s32* dcPred, JPEGDecContext* ctx) {
    s32 n;
    s32 rs;
    const u8* zz;
    s32 k;
    JPEGHuffLookup* lk;
    s32 ret;
    s32 q;
    JPEGStream* s = &ctx->stream;
    s32 sz;
    JPEGHuffCode* codes;
    JPEGHuffLookup e;
    u8* vals;
    s32 v;
    u8 pos;

    lk = ctx->tables.dc.lookup;
    FILL_BITS(s, 8);
    e = lk[(s->bits >> (s->numBits - 8)) & 0xFF];
    if (e.len != 0) {
        s->numBits -= e.len;
        rs = e.val;
    } else {
        rs = jpgdHuffDecodeSlow(ctx->tables.dc.codes, ctx->tables.dc.vals, s);
        if (rs < 0) {
            return rs;
        }
    }
    q = quant[0];
    if (rs != 0) {
        FILL_BITS(s, rs);
        n = 1 << rs;
        s->numBits -= rs;
        v = (n - 1) & (s->bits >> s->numBits);
        if ((n >> 1) > v) {
            v -= n - 1;
        }
        *dcPred += v;
    }
    coef[0] = *dcPred * q;

    lk = ctx->tables.ac.lookup;
    codes = ctx->tables.ac.codes;
    vals = ctx->tables.ac.vals;
    memset(&coef[1], 0, 63 * sizeof(s32));
    k = 1;
    FILL_BITS(s, 8);
    zz = lbl_801AAE18;
    e = lk[(s->bits >> (s->numBits - 8)) & 0xFF];
    do {
        if (e.len != 0) {
            s->numBits -= e.len;
            rs = e.val;
        } else {
            rs = jpgdHuffDecodeSlow(codes, vals, s);
            if (rs < 0) {
                return rs;
            }
        }
        sz = rs & 0xF;
        if (sz != 0) {
            k += rs >> 4;
            pos = zz[k];
            if (k >= 64) {
                return -0x64;
            }
            q = quant[pos];
            FILL_BITS(s, sz + 8);
            n = 1 << sz;
            s->numBits -= sz;
            e = lk[(s->bits >> (s->numBits - 8)) & 0xFF];
            v = (n - 1) & (s->bits >> s->numBits);
            if ((n >> 1) > v) {
                v -= n - 1;
            }
            coef[pos] = v * q;
            k++;
        } else {
            if (rs == 0) {
                break;
            }
            FILL_BITS(s, 8);
            k += 16;
            e = lk[(s->bits >> (s->numBits - 8)) & 0xFF];
        }
    } while (k < 64);
    return lbl_801AAE58[(u32)(k - 1)];
}

s32 fn_80089D2C(s32* coef, s32* quant, s32* dcPred, JPEGDecContext* ctx) {
    s32 one;
    s32 sz;
    s32 rs;
    s32 k;
    s32 limit;
    s32 ret;
    u32 n;
    s32 pos;
    JPEGHuffCode* codes;
    u32 idx;
    u8* vals;
    u32 v;
    JPEGHuffLookup* lk;
    const u8* zz;

    lk = ctx->tables.dc.lookup;
    FILL_BITS(&ctx->stream, 8);
    idx = (ctx->stream.bits >> (ctx->stream.numBits - 8)) & 0xFF;
    if (lk[idx].len != 0) {
        rs = lk[idx].val;
        ctx->stream.numBits -= lk[idx].len;
    } else {
        rs = fn_80089FB8(ctx->tables.dc.codes, ctx->tables.dc.vals, ctx);
        if (rs < 0) {
            return rs;
        }
    }
    if (rs != 0) {
        FILL_BITS(&ctx->stream, rs);
        n = 1 << rs;
        ctx->stream.numBits -= rs;
        v = (n - 1) & (ctx->stream.bits >> ctx->stream.numBits);
        if ((n >> 1) > v) {
            v -= n - 1;
        }
        *dcPred += v;
    }
    coef[0] = *dcPred * quant[0];

    lk = ctx->tables.ac.lookup;
    codes = ctx->tables.ac.codes;
    vals = ctx->tables.ac.vals;
    limit = ctx->coefLimit;
    memset(&coef[1], 0, ctx->coefLimit2);
    zz = lbl_801AAE18;
    k = 1;
    one = 1;
    do {
        FILL_BITS(&ctx->stream, 8);
        idx = (ctx->stream.bits >> (ctx->stream.numBits - 8)) & 0xFF;
        if (lk[idx].len != 0) {
            rs = lk[idx].val;
            ctx->stream.numBits -= lk[idx].len;
        } else {
            rs = fn_80089FB8(codes, vals, ctx);
            if (rs < 0) {
                return rs;
            }
        }
        sz = rs & 0xF;
        if (sz != 0) {
            k += rs >> 4;
            FILL_BITS(&ctx->stream, sz);
            if (k > limit) {
                k++;
                ctx->stream.numBits -= sz;
            } else {
                n = one << sz;
                ctx->stream.numBits -= sz;
                v = (n - 1) & (ctx->stream.bits >> ctx->stream.numBits);
                if ((n >> 1) > v) {
                    v -= n - 1;
                }
                if (k >= 64) {
                    return -0x64;
                }
                pos = zz[k];
                k++;
                coef[pos] = v * quant[pos];
            }
        } else {
            if (rs == 0) {
                return 0;
            }
            k += 16;
        }
    } while (k < 64);
    return 0;
}

s32 fn_80089FB8(JPEGHuffCode* codes, u8* vals, JPEGDecContext* ctx) {
    JPEGHuffCode* c;
    JPEGStream* s = &ctx->stream;
    s32 ret;
    u32 code;
    JPEGHuffCode hc;
    JPEGHuffCode hc2;
    u32 l;
    u32 idx;

    FILL_BITS(s, 17);
    s->numBits -= 9;
    code = (s->bits >> s->numBits) & 0x1FF;
    c = &codes[9];
    l = 9;
    while (hc = *c, hc2 = hc, code > hc2.code) {
        l++;
        c++;
        if (l > 16) {
            return -0x64;
        }
        s->numBits--;
        code <<= 1;
        code |= (s->bits >> s->numBits) & 1;
    }
    idx = code - hc.code;
    idx += hc.index;
    return vals[idx];
}
