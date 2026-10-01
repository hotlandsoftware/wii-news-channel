#ifndef GXDISPLIST_H
#define GXDISPLIST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>
#include <macros.h>

void GXBeginDisplayList(void *, u32);
u32 GXEndDisplayList(void);
void GXCallDisplayList(const void *, u32);

/* Added for g3d (Task 10), as in ogws GXDisplayList.h */
#include <revolution/gx/GXVert.h>
static inline void GXFastCallDisplayList(const void* list, u32 nbytes) {
    GXWGFifo.u8 = 0x40; /* GX_FIFO_CMD_CALL_DL */
    GXWGFifo.u32 = (u32)list;
    GXWGFifo.u32 = nbytes;
}

#ifdef __cplusplus
}
#endif

#endif // GXDISPLIST_H