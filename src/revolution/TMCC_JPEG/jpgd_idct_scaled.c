#include <string.h>
#include "jpgd_internal.h"

#define CLAMP_U8(v) (((v) < 256 && (v) > -1) ? (v) : ((v) < 0 ? 0 : 255))
#define CLAMP_S8(v) (((v) < 128 && (v) > -129) ? (v) : ((v) > 0 ? 127 : -128))

void fn_80083554(s32* in, u8* out, u16 stride, s32 extent) {
    s32 ws[64];
    s32* ip;
    s32* wp;
    s32 i;
    s32 t;
    s32 e0;
    s32 e1;
    s32 e13;

    ip = in + 24;
    wp = ws + 24;
    for (i = 0; i < 4; i++) {
        t = ((ip[1] - ip[3]) * 181) >> 8;
        e13 = t + (ip[3] + ip[1]);
        e0 = ip[0] + ip[2];
        e1 = ip[0] - ip[2];
        wp[0] = e0 + e13;
        wp[1] = e1 + t;
        wp[2] = e1 - t;
        wp[3] = e0 - e13;
        ip -= 8;
        wp -= 8;
    }

    wp = &ws[3];
    for (i = 3; i >= 0; i--) {
        u8* o = out + i;
        t = ((wp[8] - wp[24]) * 181) >> 8;
        e13 = t + (wp[24] + wp[8]);
        e0 = (wp[0] + 0x40000) + wp[16];
        e1 = (wp[0] + 0x40000) - wp[16];
        o[0] = CLAMP_U8((e0 + e13) >> 11);
        o[stride] = CLAMP_U8((e1 + t) >> 11);
        o[stride * 2] = CLAMP_U8((e1 - t) >> 11);
        o[stride * 3] = CLAMP_U8((e0 - e13) >> 11);
        wp--;
    }
}

void fn_80083774(s32* in, u8* out, u16 stride, s32 extent) {
    s32 a;
    s32 t0;
    s32 t1;
    s32 t2;
    s32 t3;

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

void fn_80083890(s32* in, u8* out, u16 stride, s32 extent) {
    out[0] = CLAMP_U8((in[0] >> 11) + 0x80);
}

void fn_800838D4(s32* in, u8* out, u16 stride, s32 extent) {
    s32 ws[64];
    s32* ip;
    s32* wp;
    s32 i;
    s32 t;
    s32 e0;
    s32 e1;
    s32 e13;

    ip = in + 24;
    wp = ws + 24;
    for (i = 0; i < 4; i++) {
        t = ((ip[1] - ip[3]) * 181) >> 8;
        e13 = t + (ip[3] + ip[1]);
        e0 = ip[0] + ip[2];
        e1 = ip[0] - ip[2];
        wp[0] = e0 + e13;
        wp[1] = e1 + t;
        wp[2] = e1 - t;
        wp[3] = e0 - e13;
        ip -= 8;
        wp -= 8;
    }

    wp = &ws[3];
    for (i = 3; i >= 0; i--) {
        s8* o = (s8*)out + i;
        t = ((wp[8] - wp[24]) * 181) >> 8;
        e13 = t + (wp[24] + wp[8]);
        e0 = wp[0] + wp[16];
        e1 = wp[0] - wp[16];
        o[0] = CLAMP_S8((e0 + e13) >> 11);
        o[8] = CLAMP_S8((e1 + t) >> 11);
        o[16] = CLAMP_S8((e1 - t) >> 11);
        o[24] = CLAMP_S8((e0 - e13) >> 11);
        wp--;
    }
}

void fn_80083AF8(s32* in, u8* out, u16 stride, s32 extent) {
    s32 t0;
    s32 t1;
    s32 t2;
    s32 t3;
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

void fn_80083C1C(s32* in, u8* out, u16 stride, s32 extent) {
    *(s8*)out = CLAMP_S8(in[0] >> 11);
}
