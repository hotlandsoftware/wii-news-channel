#ifndef RVL_SDK_VF_PF_DIR_H
#define RVL_SDK_VF_PF_DIR_H
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

void VFiPFDIR_FinalizeAllDirs(struct PF_VOLUME* p_vol);

// Not in ogws; names are guesses (from the VFipf2_* wrappers).
s32 VFiPFDIR_fsfirst(struct PF_STR* p_path, u32 attr, struct PF_DIRENT* p_dirent);
s32 VFiPFDIR_fsnext(struct PF_DIRENT* p_dirent);
s32 VFiPFDIR_mkdir(struct PF_STR* p_path);

#ifdef __cplusplus
}
#endif
#endif
