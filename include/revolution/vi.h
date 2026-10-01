#ifndef REVOLUTION_VI_H
#define REVOLUTION_VI_H

#include <types.h>
#include <revolution/gx.h>

#ifdef __cplusplus
extern "C" {
#endif

void VIWaitForRetrace(void);
void VIConfigure(const GXRenderModeObj* rm);
void VIFlush(void);
void VISetNextFrameBuffer(void* fb);
void VISetBlack(BOOL black);

#ifdef __cplusplus
}
#endif

#endif
