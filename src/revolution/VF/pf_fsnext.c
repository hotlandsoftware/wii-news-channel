#include <revolution/vf.h>

// Not in ogws (dead-stripped there); the name is a guess from the PrFILE2 API file order.
int VFipf2_fsnext(struct PF_DIRENT* p_dirent) {
    s32 err;
    err = VFiPFAPI_convertReturnValue(VFiPFDIR_fsnext(p_dirent));
    return err;
}
