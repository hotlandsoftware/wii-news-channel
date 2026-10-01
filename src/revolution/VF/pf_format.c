#include <revolution/vf.h>

// Not in ogws (dead-stripped there); the name is a guess from the PrFILE2 API file order.
int VFipf2_format(s8 drv_char, const u8* param) {
    s32 err;
    err = VFiPFAPI_convertReturnValue(VFiPFVOL_format(drv_char, param));
    return err;
}
