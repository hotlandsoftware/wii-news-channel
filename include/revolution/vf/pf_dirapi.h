#ifndef RVL_SDK_VF_PF_DIRAPI_H
#define RVL_SDK_VF_PF_DIRAPI_H
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

// PrFILE2 API wrappers that Wii Sports dead-strips (so ogws has no source for them).
// pf_format.c, pf_fsfirst.c, pf_fsnext.c, pf_mkdir.c. Names except VFipf2_mkdir are guesses.
struct PF_DIRENT;

int VFipf2_format(s8 drv_char, const u8* param);
int VFipf2_fsfirst(const s8* path, u8 attr, struct PF_DIRENT* p_dirent);
int VFipf2_fsnext(struct PF_DIRENT* p_dirent);
int VFipf2_mkdir(const s8* path);

#ifdef __cplusplus
}
#endif
#endif
