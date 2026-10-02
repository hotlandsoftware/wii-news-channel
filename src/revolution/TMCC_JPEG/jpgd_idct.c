#include <string.h>
#include "jpgd_internal.h"

#define DESCALE8(x) ((x) >> 8)
#define CLAMP_U8(x) (((x) >> 19) == 0 ? ((x) >> 11) : ((x) < 0 ? 0 : 255))

void fn_8008082C(s32* in, u8* out, u16 stride, s32 extent) {
    s32 ws[64];
    s32* wp;
    s32 i;
    s32 rows;
    s32 d0, d1, d2, d3, d4, d5, d6, d7;
    s32 tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
    s32 tmp10, tmp11, tmp12, tmp13;
    s32 z5, z10, z11, z12, z13;
    s32 nz;

    rows = (extent >> 4) * 8;
    wp = ws;
    for (i = 0; i < rows; i += 8) {
        nz = in[4];
        nz |= in[6];
        nz |= in[2];
        nz |= in[1];
        nz |= in[7];
        nz |= in[5];
        nz |= in[3];
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
            d1 = in[1];
            d2 = in[2];
            d3 = in[3];
            d4 = in[4];
            d5 = in[5];
            d6 = in[6];
            d7 = in[7];
            tmp12 = DESCALE8((d2 - d6) * 181);
            tmp13 = tmp12 + (d6 + d2);
            tmp10 = d0 + d4;
            tmp11 = d0 - d4;
            tmp0 = tmp10 + tmp13;
            tmp3 = tmp10 - tmp13;
            tmp1 = tmp11 + tmp12;
            tmp2 = tmp11 - tmp12;

            z13 = d5 + d3;
            z10 = d5 - d3;
            z11 = d1 + d7;
            z12 = d1 - d7;

            tmp11 = DESCALE8((z11 - z13) * 181);
            z5 = DESCALE8((z10 + z12) * 98);
            tmp10 = DESCALE8(z12 * 334) - z5;
            tmp12 = z5 + DESCALE8(z10 * 139);

            tmp7 = z11 + (z13 + tmp10);
            tmp6 = tmp11 + tmp10;
            tmp5 = tmp12 + tmp11;
            tmp4 = tmp12;

            wp[0] = tmp0 + tmp7;
            wp[7] = tmp0 - tmp7;
            wp[1] = tmp1 + tmp6;
            wp[6] = tmp1 - tmp6;
            wp[2] = tmp2 + tmp5;
            wp[5] = tmp2 - tmp5;
            wp[3] = tmp3 + tmp4;
            wp[4] = tmp3 - tmp4;
        }
        in += 8;
        wp += 8;
    }
    for (; i <= 56; i += 8) {
        memset(&ws[i], 0, 32);
    }

    wp = &ws[7];
    for (i = 7; i >= 0; i--) {
        u8* o = out + i;
        d4 = wp[32];
        d6 = wp[48];
        d2 = wp[16];
        d1 = wp[8];
        d7 = wp[56];
        d5 = wp[40];
        d3 = wp[24];
        if ((d4 | d6 | d2 | d1 | d7 | d5 | d3) == 0) {
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
            tmp10 = d0 + d4;
            tmp11 = d0 - d4;
            tmp0 = tmp10 + tmp13;
            tmp3 = tmp10 - tmp13;
            tmp1 = tmp11 + tmp12;
            tmp2 = tmp11 - tmp12;

            z13 = d5 + d3;
            z10 = d5 - d3;
            z11 = d1 + d7;
            z12 = d1 - d7;

            tmp11 = DESCALE8((z11 - z13) * 181);
            z5 = DESCALE8((z10 + z12) * 98);
            tmp10 = DESCALE8(z12 * 334) - z5;
            tmp12 = z5 + DESCALE8(z10 * 139);

            tmp7 = z11 + (z13 + tmp10);
            tmp6 = tmp11 + tmp10;
            tmp5 = tmp12 + tmp11;
            tmp4 = tmp12;

            x = tmp0 + tmp7;
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

