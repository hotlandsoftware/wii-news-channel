#include <revolution/vf.h>

// Not in ogws (dead-stripped there); the name is a guess.
int VFipf2_unmount2(s8 drive, u32 mode) {
    s32 err;
    err = VFiPFAPI_convertReturnValue(VFiPFVOL_unmount2(drive, mode));
    return err;
}

int VFipf2_unmount(s8 drive, u32 mode) {
    s32 err;
    err = VFiPFAPI_convertReturnValue4unmount(VFiPFVOL_unmount(drive, mode));
    return err;
}
