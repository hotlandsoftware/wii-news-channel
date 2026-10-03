#include <string.h>
#include "jpgd_internal.h"

#define CLAMP_U8(v) (((v) < 256 && (v) > -1) ? (v) : ((v) < 0 ? 0 : 255))
#define CLAMP_S8(v) (((v) < 128 && (v) > -129) ? (v) : ((v) > 0 ? 127 : -128))

void jpgdIdct4x4Y(s32* in, u8* out, u16 stride, s32 extent) {
    s32 d0;
    s32* wp;
    s32 d1;
    s32 e1;
    s32* ip;
    s32 d3;
    s32 d2;
    s32 e0;
    s32 i;
    s32 t;
    s32 e13;
    s32 ws[64];

    for (i = 3; i >= 0; i--) {
        d1 = in[i * 8 + 1];
        d3 = in[i * 8 + 3];
        d0 = in[i * 8 + 0];
        d2 = in[i * 8 + 2];
        t = ((d1 - d3) * 181) >> 8;
        e13 = t + (d1 + d3);
        e0 = d0 + d2;
        e1 = d0 - d2;
        ws[i * 8 + 0] = e0 + e13;
        ws[i * 8 + 1] = e1 + t;
        ws[i * 8 + 2] = e1 - t;
        ws[i * 8 + 3] = e0 - e13;
    }

    for (i = 3; i >= 0; i--) {
        u8* o = out + i;
        d1 = ws[8 + i];
        d3 = ws[24 + i];
        d0 = ws[i] + 0x40000;
        d2 = ws[16 + i];
        t = ((d1 - d3) * 181) >> 8;
        e13 = t + (d3 + d1);
        e0 = d0 + d2;
        e1 = d0 - d2;
        o[0] = CLAMP_U8((e0 + e13) >> 11);
        o[stride] = CLAMP_U8((e1 + t) >> 11);
        o[stride * 2] = CLAMP_U8((e1 - t) >> 11);
        o[stride * 3] = CLAMP_U8((e0 - e13) >> 11);
    }
}

void jpgdIdct2x2Y(s32* in, u8* out, u16 stride, s32 extent) {
    s32 a;
    s32 t1;
    s32 t0;
    s32 t3;
    s32 t2;

    a = in[0] + 0x40000;
    t0 = in[1] + a;
    t1 = a - in[1];
    t2 = in[9] + in[8];
    t3 = in[8] - in[9];
    out[0] = CLAMP_U8((t0 + t2) >> 11);
    out[stride] = CLAMP_U8((t0 - t2) >> 11);
    out[1] = CLAMP_U8((t1 + t3) >> 11);
    out[stride + 1] = CLAMP_U8((t1 - t3) >> 11);
}

void jpgdIdct1x1Y(s32* in, u8* out, u16 stride, s32 extent) {
    out[0] = CLAMP_U8((in[0] >> 11) + 0x80);
}

void jpgdIdct4x4C(s32* in, u8* out, u16 stride, s32 extent) {
    s32 d0;
    s32 d1;
    s32 e1;
    s32 d3;
    s32 d2;
    s32 e0;
    s32 i;
    s32 t;
    s32 e13;
    s32 ws[64];

    for (i = 3; i >= 0; i--) {
        d1 = in[i * 8 + 1];
        d3 = in[i * 8 + 3];
        d0 = in[i * 8 + 0];
        d2 = in[i * 8 + 2];
        t = ((d1 - d3) * 181) >> 8;
        e13 = t + (d1 + d3);
        e0 = d0 + d2;
        e1 = d0 - d2;
        ws[i * 8 + 0] = e0 + e13;
        ws[i * 8 + 1] = e1 + t;
        ws[i * 8 + 2] = e1 - t;
        ws[i * 8 + 3] = e0 - e13;
    }

    for (i = 3; i >= 0; i--) {
        s8* o = (s8*)(out + i);
        d1 = ws[8 + i];
        d3 = ws[24 + i];
        d0 = ws[i];
        d2 = ws[16 + i];
        t = ((d1 - d3) * 181) >> 8;
        e13 = t + (d3 + d1);
        e0 = d0 + d2;
        e1 = d0 - d2;
        o[0] = CLAMP_S8((e0 + e13) >> 11);
        o[8] = CLAMP_S8((e1 + t) >> 11);
        o[16] = CLAMP_S8((e1 - t) >> 11);
        o[24] = CLAMP_S8((e0 - e13) >> 11);
    }
}

void jpgdIdct2x2C(s32* in, u8* out, u16 stride, s32 extent) {
    s32 t1;
    s32 t3;
    s32 t0;
    s32 t2;

    s8* o = (s8*)out;

    t0 = in[0] + in[1];
    t1 = in[0] - in[1];
    t2 = in[8] + in[9];
    t3 = in[8] - in[9];
    o[0] = CLAMP_S8((t0 + t2) >> 11);
    o[8] = CLAMP_S8((t0 - t2) >> 11);
    o[1] = CLAMP_S8((t1 + t3) >> 11);
    o[9] = CLAMP_S8((t1 - t3) >> 11);
}

void jpgdIdct1x1C(s32* in, u8* out, u16 stride, s32 extent) {
    *(s8*)out = CLAMP_S8(in[0] >> 11);
}
