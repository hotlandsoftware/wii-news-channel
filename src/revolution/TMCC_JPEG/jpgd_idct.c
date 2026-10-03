#include <string.h>
#include "jpgd_internal.h"

#define DESCALE8(x) ((x) >> 8)
#define CLAMP_S8(v) (((v) < 128 && (v) > -129) ? (v) : ((v) > 0 ? 127 : -128))
#define CLAMP_U8(x) (((x) >> 19) == 0 ? ((x) >> 11) : ((x) < 0 ? 0 : 255))

void jpgdIdct8x8Y(s32* in, u8* out, u16 stride, s32 extent) {
    s32 tmp10;
    s32 d4;
    s32 tmp5;
    s32 tmp7;
    s32 rows;
    s32 d1;
    s32 tmp3;
    s32 tmp1;
    s32 tmp0;
    s32* wp;
    s32 d6;
    s32 z10;
    s32 d2;
    s32 d0;
    s32 d5;
    s32 tmp11;
    s32 z11;
    s32 nz;
    s32 ws[64];
    s32 i;
    s32 z13;
    s32 tmp6;
    s32 z12;
    s32 d3;
    s32 tmp13;
    s32 z5;
    s32 d7;
    s32 tmp12;
    s32 tmp2;
    s32 tmp4;

    wp = ws;
    rows = (extent >> 4) * 8;

    for (i = 0; i < rows; i += 8) {
        d1 = in[1];
        d2 = in[2];
        d4 = in[4];
        d6 = in[6];
        nz = (u32)d6 | (u32)d4;
        d3 = in[3];
        nz = (u32)d2 | nz;
        d7 = in[7];
        d5 = in[5];
        nz = (u32)d1 | nz;
        nz = (u32)d7 | nz;
        nz = (u32)d5 | nz;
        nz = (u32)d3 | nz;
        if (nz == 0) {
            d0 = in[0];
            wp[7] = d0;
            wp[6] = d0;
            wp[5] = d0;
            wp[4] = d0;
            wp[3] = d0;
            wp[2] = d0;
            wp[1] = d0;
            wp[0] = d0;
        } else {
            d0 = in[0];
            tmp12 = DESCALE8((d2 - d6) * 181);
            tmp13 = tmp12 + (d6 + d2);
            tmp11 = d0 - d4;
            tmp10 = d0 + d4;
            tmp1 = tmp11 + tmp12;
            tmp0 = tmp10 + tmp13;
            tmp3 = tmp10 - tmp13;
            tmp2 = tmp11 - tmp12;

            z11 = d1 + d7;
            z13 = d5 + d3;
            z12 = d1 - d7;
            z10 = d5 - d3;

            tmp11 = DESCALE8((z11 - z13) * 181);
            z5 = DESCALE8((z10 + z12) * 98);
            tmp10 = DESCALE8(z12 * 334) - z5;
            tmp12 = z5 + DESCALE8(139 * z10);

            tmp6 = tmp11 + tmp10;
            tmp7 = z11 + (z13 + tmp10);
            tmp4 = tmp12;
            tmp5 = tmp12 + tmp11;

            wp[0] = tmp0 + tmp7;
            wp[7] = tmp0 - tmp7;
            wp[1] = tmp1 + tmp6;
            wp[6] = tmp1 - tmp6;
            wp[2] = tmp5 + tmp2;
            wp[5] = tmp2 - tmp5;
            wp[3] = tmp3 + tmp4;
            wp[4] = tmp3 - tmp4;
        }
        in = in + 8;
        wp += 8;
    }
    for (; i <= 56; i += 8) {
        memset(&ws[i], 0, 32);
    }

    wp = &ws[7];
    for (i = 7; i >= 0; i--) {
        u8* o = out + i;
        d6 = wp[48];
        d5 = wp[40];
        d4 = wp[32];
        d3 = wp[24];
        d1 = wp[8];
        d2 = wp[16];
        nz = (u32)d6 | (u32)d4;
        d7 = wp[56];
        nz = (u32)d2 | nz;
        nz = (u32)d1 | nz;
        nz = nz | (u32)d7;
        nz = (u32)d5 | nz;
        nz |= (u32)d3;
        if (nz == 0) {
            s32 c = (wp[0] >> 11) + 0x80;
            c = (c < 256 && c > -1) ? c : (c < 0 ? 0 : 255);
            o[stride * 7] = c;
            o[stride * 6] = c;
            o[stride * 5] = c;
            o[stride * 4] = c;
            o[stride * 3] = c;
            o[stride * 2] = c;
            o[stride] = c;
            o[0] = c;
        } else {
            s32 x;

            d0 = wp[0] + 0x40000;
            tmp12 = DESCALE8((d2 - d6) * 181);
            tmp13 = tmp12 + (d6 + d2);
            tmp10 = d4 + d0;
            tmp11 = d0 - d4;
            tmp0 = tmp10 + tmp13;
            tmp3 = tmp10 - tmp13;
            tmp2 = tmp11 - tmp12;
            tmp1 = tmp12 + tmp11;

            z13 = d5 + d3;
            z10 = d5 - d3;
            z11 = d1 + d7;
            z12 = d1 - d7;

            tmp11 = DESCALE8((z11 - z13) * 181);
            z5 = DESCALE8((z10 + z12) * 98);
            tmp10 = DESCALE8(z12 * 334) - z5;
            tmp12 = z5 + DESCALE8(139 * z10);

            tmp7 = z11 + (tmp10 + z13);
            tmp5 = tmp12 + tmp11;
            tmp6 = tmp11 + tmp10;
            tmp4 = tmp12;

            x = tmp7 + tmp0;
            o[0] = CLAMP_U8(x);
            x = tmp0 - tmp7;
            o[stride * 7] = CLAMP_U8(x);
            x = tmp1 + tmp6;
            o[stride] = CLAMP_U8(x);
            x = tmp1 - tmp6;
            o[stride * 6] = CLAMP_U8(x);
            x = tmp2 + tmp5;
            o[stride * 2] = CLAMP_U8(x);
            x = tmp2 - tmp5;
            o[stride * 5] = CLAMP_U8(x);
            x = tmp3 + tmp4;
            o[stride * 3] = CLAMP_U8(x);
            x = tmp3 - tmp4;
            o[stride * 4] = CLAMP_U8(x);
        }
        wp--;
    }
}

void jpgdIdct8x8C(s32* in, u8* out, u16 stride, s32 extent) {
    s32 tmp1;
    u32 nz;
    s32 tmp4;
    s32 d2;
    s32 rows;
    s32 d1;
    s32 tmp7;
    s32* wp;
    s32 d4;
    s32 d5;
    s32 d0;
    s32 i;
    s32 tmp11;
    s32 d6;
    s32 tmp5;
    s32 d3;
    s32 d7;
    s32 z12;
    s32 z13;
    s32 ws[64];
    s32 tmp10;
    s32 tmp12;
    s32 tmp13;
    s32 tmp3;
    s32 z11;
    s32 tmp0;
    s32 tmp6;
    s32 tmp2;
    s32 z5;
    s32 z10;

    if (extent == 0x11) {
        memset(out, (s8)CLAMP_S8(in[0] >> 11), 64);
        return;
    }
    rows = (extent >> 4) * 8;
    if ((0xF & extent) <= 2) {
        wp = ws;
        for (i = 0; i < rows; i += 8) {
            d0 = in[0];
            d1 = in[1];
            tmp11 = DESCALE8(181 * d1);
            z5 = DESCALE8(98 * d1);
            tmp10 = DESCALE8(d1 * 334) - z5;
            tmp7 = d1 + tmp10;
            tmp6 = tmp11 + tmp10;
            tmp5 = z5 + tmp11;
            wp[0] = d0 + tmp7;
            wp[7] = d0 - tmp7;
            wp[1] = d0 + tmp6;
            wp[6] = d0 - tmp6;
            wp[2] = d0 + tmp5;
            wp[5] = d0 - tmp5;
            wp[3] = d0 + z5;
            wp[4] = d0 - z5;
            in = 8 + in;
            wp += 8;
        }
        for (; i <= 56; i += 8) {
            memset(&ws[i], 0, 32);
        }
    } else {
        wp = ws;
        for (i = 0; i < rows; i += 8) {
            d7 = in[7];
            d4 = in[4];
            d6 = in[6];
            nz = (u32)d6 | (u32)d4;
            d1 = in[1];
            d2 = in[2];
            d3 = in[3];
            nz |= (u32)d2;
            nz = (u32)d1 | nz;
            nz = (u32)d7 | nz;
            d5 = in[5];
            nz = (u32)d5 | nz;
            nz = (u32)d3 | nz;
            if (nz == 0) {
                d0 = in[0];
                wp[7] = d0;
                wp[6] = d0;
                wp[5] = d0;
                wp[4] = d0;
                wp[3] = d0;
                wp[2] = d0;
                wp[1] = d0;
                wp[0] = d0;
            } else {
                d0 = in[0];
                tmp12 = DESCALE8((d2 - d6) * 181);
                tmp11 = d0 - d4;
                tmp13 = tmp12 + (d6 + d2);
                tmp10 = d4 + d0;
                tmp0 = tmp13 + tmp10;
                tmp1 = tmp11 + tmp12;
                tmp3 = tmp10 - tmp13;
                tmp2 = tmp11 - tmp12;

                z11 = d7 + d1;
                z10 = d5 - d3;
                z12 = d1 - d7;
                z13 = d3 + d5;

                tmp11 = DESCALE8((z11 - z13) * 181);
                z5 = DESCALE8((z10 + z12) * 98);
                tmp10 = DESCALE8(z12 * 334) - z5;
                tmp12 = z5 + DESCALE8(139 * z10);

                tmp6 = tmp11 + tmp10;
                tmp5 = tmp12 + tmp11;
                tmp7 = z11 + (z13 + tmp10);
                tmp4 = tmp12;

                wp[0] = tmp0 + tmp7;
                wp[7] = tmp0 - tmp7;
                wp[1] = tmp6 + tmp1;
                wp[6] = tmp1 - tmp6;
                wp[2] = tmp5 + tmp2;
                wp[5] = tmp2 - tmp5;
                wp[3] = tmp3 + tmp4;
                wp[4] = tmp3 - tmp4;
            }
            in = 8 + in;
            wp = wp + 8;
        }
        for (; i <= 56; i += 8) {
            memset(&ws[i], 0, 32);
        }
    }

    wp = &ws[7];
    for (i = 7; i >= 0; i--) {
        s8* o = (s8*)out + i;
        d1 = wp[8];
        d4 = wp[32];
        d2 = wp[16];
        d3 = wp[24];
        d6 = wp[48];
        nz = (u32)d4 | (u32)d6;
        d7 = wp[56];
        nz |= (u32)d2;
        d5 = wp[40];
        nz = nz | (u32)d1;
        nz = (u32)d7 | nz;
        nz = (u32)d5 | nz;
        nz = nz | (u32)d3;
        if (nz == 0) {
            s32 c = CLAMP_S8(wp[0] >> 11);
            o[56] = c;
            o[48] = c;
            o[40] = c;
            o[32] = c;
            o[24] = c;
            o[16] = c;
            o[8] = c;
            o[0] = c;
        } else {
            s32 x;

            d0 = wp[0];
            tmp12 = DESCALE8((d2 - d6) * 181);
            tmp13 = tmp12 + (d6 + d2);
            tmp11 = d0 - d4;
            tmp10 = d4 + d0;
            tmp1 = tmp11 + tmp12;
            tmp3 = tmp10 - tmp13;
            tmp0 = tmp10 + tmp13;
            tmp2 = tmp11 - tmp12;

            z12 = d1 - d7;
            z11 = d1 + d7;
            z10 = d5 - d3;
            z13 = d3 + d5;

            tmp11 = DESCALE8((z11 - z13) * 181);
            z5 = DESCALE8((z10 + z12) * 98);
            tmp10 = DESCALE8(334 * z12) - z5;
            tmp12 = z5 + DESCALE8(z10 * 139);

            tmp4 = tmp12;
            tmp5 = tmp12 + tmp11;
            tmp7 = z11 + (tmp10 + z13);
            tmp6 = tmp11 + tmp10;

            o[0] = CLAMP_S8((tmp0 + tmp7) >> 11);
            o[56] = CLAMP_S8((tmp0 - tmp7) >> 11);
            o[8] = CLAMP_S8((tmp1 + tmp6) >> 11);
            o[48] = CLAMP_S8((tmp1 - tmp6) >> 11);
            o[16] = CLAMP_S8((tmp2 + tmp5) >> 11);
            o[40] = CLAMP_S8((tmp2 - tmp5) >> 11);
            o[24] = CLAMP_S8((tmp3 + tmp4) >> 11);
            o[32] = CLAMP_S8((tmp3 - tmp4) >> 11);
        }
        wp--;
    }
}
