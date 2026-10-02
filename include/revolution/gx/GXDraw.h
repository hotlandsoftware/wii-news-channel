#ifndef GXDRAW_H
#define GXDRAW_H

// Added for nw4r::ef (Task 9): the GXDraw.c shapes (debug drawing of the
// emitter forms).
#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

void GXDrawCylinder(u8 sides);
void GXDrawTorus(f32 rc, u8 numc, u8 numt);
void GXDrawSphere(u32 stacks, u32 sectors);
void GXDrawCube(void);

#ifdef __cplusplus
}
#endif

#endif // GXDRAW_H
