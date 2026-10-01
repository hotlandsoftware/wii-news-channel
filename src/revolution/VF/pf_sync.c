#include <revolution/vf.h>

// Not in ogws (dead-stripped there); the name is a guess from the PrFILE2 API file order.
int VFipf2_sync(s8 drv_char, u32 mode) {
    s32 err;
    err = VFiPFAPI_convertReturnValue(VFiPFVOL_sync(drv_char, mode));
    return err;
}
