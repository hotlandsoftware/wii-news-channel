#ifndef GXINIT_H
#define GXINIT_H

#include <revolution/gx/GXFifo.h>

#ifdef __cplusplus
extern "C" {
#endif

GXFifoObj* GXInit(void*, u32);
void __GXInitGX(void);

#ifdef __cplusplus
}
#endif

#endif // GXINIT_H
