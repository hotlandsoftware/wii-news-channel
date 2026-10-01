#ifndef PF_FREAD_H
#define PF_FREAD_H

#include <types.h>
#include <macros.h>
#include <revolution/vf/vf_struct.h>

s32 VFipf2_fread(u8* p_buf, u32 size, u32 count, PF_FILE* p_file);

#endif
