// HBM (the HOME Menu library) and vcmv (the Operations Guide viewer)
// placeholder: neither ever opens.
//
// TODO(milestone 7): the HOME Menu is portable C++ on NW4R
// (src/revolution/HBM) and can be compiled natively once layouts draw and
// sound plays. The Operations Guide viewer runs a PowerPC module (Opera) and
// needs a decision of its own. Every function is weak (PC_NOOP).
//
// Behaviour of the placeholder:
//
// - The game opens the HOME Menu when the HOME button is pressed and then
//   calls HBMCalc() every frame until it returns a selection. HBMCalc()
//   answers HBM_SELECT_HOMEBTN at once ("HOME pressed again: close, nothing
//   chosen"), so the game is back in its normal loop on the next frame.
// - The viewer cannot load its library: VCMVLoadLibrary() fails and the game
//   skips the guide.

#include <revolution/hbm.h>
#include <vcmv/vcmv.h>

#include "pc_noop.h"

extern "C" {

// --- HBM -----------------------------------------------------------------------

PC_NOOP void HBMCreate(const HBMDataInfo* info) {}
PC_NOOP void HBMDelete(void) {}
PC_NOOP void HBMInit(void) {}
PC_NOOP void HBMDraw(void) {}
PC_NOOP void HBMSetAdjustFlag(BOOL flag) {}
PC_NOOP void HBMStartBlackOut(void) {}

PC_NOOP HBMSelectBtnNum HBMCalc(const HBMControllerData* controller) {
    return HBM_SELECT_HOMEBTN;
}

PC_NOOP HBMSelectBtnNum HBMGetSelectBtnNum(void) {
    return HBM_SELECT_HOMEBTN;
}

PC_NOOP void HBMCreateSound(void* soundData, void* memBuf, u32 memSize) {}
PC_NOOP void HBMDeleteSound(void) {}
PC_NOOP void HBMUpdateSound(void) {}

// --- vcmv ------------------------------------------------------------------------

PC_NOOP void VCMVInit(MEMAllocator* mem1, MEMAllocator* mem2) {}
PC_NOOP void VCMVLoadCursor(HBMDataInfo* info) {}

// 0 = loaded. The Opera module is PowerPC code: it cannot be loaded.
PC_NOOP s32 VCMVLoadLibrary(void) {
    return -1;
}

PC_NOOP void VCMVUnloadLibrary(void) {}
PC_NOOP void VCMVSetArchive(void* arc) {}
PC_NOOP void VCMVSetRenderMode(GXRenderModeObj* rmode1, GXRenderModeObj* rmode2, BOOL flag) {}
PC_NOOP void VCMVSetFontSize(u16 size) {}
PC_NOOP void VCMVSetStartUrl(const char* url) {}

PC_NOOP BOOL VCMVCreateSurface(s32 width, s32 height) {
    return FALSE;
}

PC_NOOP void VCMVDestroySurface(void) {}

PC_NOOP BOOL VCMVCreateHeap(u32 size) {
    return FALSE;
}

PC_NOOP void VCMVDestroyHeap(void) {}

// Runs the viewer until the user leaves it and returns the page it was on.
// Never reached (the library does not load); leaves at once, on the same page.
PC_NOOP const char* VCMVRun(vcmvDrawCallback callback, const char* url, u8 chan) {
    return url;
}

PC_NOOP void VCMVQuit(s32 frames) {}

} // extern "C"
