#ifndef REVOLUTION_HBM_COMMON_H
#define REVOLUTION_HBM_COMMON_H

// Private umbrella header of the HOME Menu library (homebuttonLib, May 16 2007).
// Ported from doldecomp/ogws homebuttonMiniLib (May 7 2007, same revision).

#include <revolution/hbm.h>
#include <revolution/hbm/HBMAnmController.h>
#include <revolution/hbm/HBMController.h>
#include <revolution/hbm/HBMFrameController.h>
#include <revolution/hbm/HBMGUIManager.h>
#include <revolution/hbm/HBMHomeButton.h>
#include <revolution/hbm/HBMRemoteSpk.h>

void* HBMAllocMem(u32 size);
void HBMFreeMem(void* pBlock);

#endif
