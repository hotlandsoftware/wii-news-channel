#include <revolution/vf.h>

extern PF_VOLUME_SET VFipf_vol_set;

// Not in ogws (dead-stripped there). Petari's d_vf_sys.c calls it VFipf2_mkdir.
int VFipf2_mkdir(const s8* path) {
    s32 err;
    struct PF_STR path_str;

    err = VFiPFSTR_InitStr(&path_str, (const s8*)path, 1);
    if (err == 0) {
        err = VFiPFDIR_mkdir(&path_str);
    } else {
        VFipf_vol_set.last_error = err;
    }

    return VFiPFAPI_convertReturnValue(err);
}
