#ifndef REVOLUTION_MTX_H
#define REVOLUTION_MTX_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Vec2 {
    f32 x, y;
} Vec2;

typedef struct Vec {
    f32 x, y, z;
} Vec;

typedef f32 Mtx[3][4];
typedef f32 Mtx44[4][4];

void PSMTXIdentity(Mtx m);
void PSMTXCopy(const Mtx src, Mtx dst);
void PSMTXConcat(const Mtx a, const Mtx b, Mtx ab);
void PSMTXTrans(Mtx m, f32 xT, f32 yT, f32 zT);
void PSMTXMultVec(const Mtx m, const Vec* src, Vec* dst);
void C_MTXOrtho(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f);

#define MTXIdentity PSMTXIdentity
#define MTXOrtho C_MTXOrtho

#ifdef __cplusplus
}
#endif

#endif
