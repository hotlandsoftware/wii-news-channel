#include "revolution/vf/d_common.h"
#include "revolution/vf/d_vf_sys.h"
#include "revolution/vf/nand_drv.h"
#include "macros.h"
#include "revolution/nand.h"
#include "revolution/vf/vf_struct.h"
#include "revolution/vf/pf_clib.h"

extern struct PDM_DISK_SET VFipdm_disk_set;

#ifndef NON_MATCHING
void _savegpr_22(void);
void _restgpr_22(void);
#endif
s32 VF_nand_sleep_msec;
s32 VF_nand_retry_max;

static struct {
    s32 (*create)(const char*, u8, u8);
    s32 (*open)(const char*, struct NANDFileInfo*, u8);
    s32 (*createDir)(const char*, u8, u8);
    s32 (*delete)(const char*);
} l_nandFunc[26];

static const struct PDM_FUNCTBL l_nand_func = {nanddrv_init,
                                               nanddrv_finalize,
                                               nanddrv_mount,
                                               nanddrv_unmount,
                                               (s32(*)(struct PDM_DISK*, u8*))nanddrv_format,
                                               nanddrv_pread,
                                               (s32(*)(struct PDM_DISK*, u8*, u32, u32, u32*))nanddrv_pwrite,
                                               (s32(*)(struct PDM_DISK*, struct PDM_DISK_INFO*))nanddrv_get_disk_info};

static inline void _SleepAfewMiliSec(void) {
    OSSleepTicks((s64)VF_nand_sleep_msec * ((*(u32*)0x800000F8 / 4) / 1000));
}

s32 VFi_NandCreate(const char* path, u8 perm, u8 attr) {
    s32 challenge;
    s32 error;

    challenge = VF_nand_retry_max;
    error = 0;
    while (challenge-- > 0) {
        error = NANDCreate(path, perm, attr);
        if (error != NAND_RESULT_BUSY && error != NAND_RESULT_ALLOC_FAILED) {
            return error;
        } else {
            _SleepAfewMiliSec();
        }
    }
    return error;
}

s32 VFi_NandPrivateCreate(const char* path, u8 perm, u8 attr) {
    s32 challenge;
    s32 error;

    challenge = VF_nand_retry_max;
    error = 0;
    while (challenge-- > 0) {
        error = NANDPrivateCreate(path, perm, attr);
        if (error != NAND_RESULT_BUSY && error != NAND_RESULT_ALLOC_FAILED) {
            return error;
        } else {
            _SleepAfewMiliSec();
        }
    }
    return error;
}

s32 VFi_NandDelete(const char* path) {
    s32 challenge;
    s32 error;

    challenge = VF_nand_retry_max;
    error = 0;
    while (challenge-- > 0) {
        error = NANDDelete(path);
        if (error != NAND_RESULT_BUSY && error != NAND_RESULT_ALLOC_FAILED) {
            return error;
        } else {
            _SleepAfewMiliSec();
        }
    }
    return error;
}

s32 VFi_NandPrivateDelete(const char* path) {
    s32 challenge;
    s32 error;

    challenge = VF_nand_retry_max;
    error = 0;
    while (challenge-- > 0) {
        error = NANDPrivateDelete(path);
        if (error != NAND_RESULT_BUSY && error != NAND_RESULT_ALLOC_FAILED) {
            return error;
        } else {
            _SleepAfewMiliSec();
        }
    }
    return error;
}

s32 VFi_NandClose(struct NANDFileInfo* info) {
    s32 challenge;
    s32 error;

    challenge = VF_nand_retry_max;
    error = 0;
    while (challenge-- > 0) {
        error = NANDClose(info);
        if (error != NAND_RESULT_BUSY && error != NAND_RESULT_ALLOC_FAILED) {
            return error;
        } else {
            _SleepAfewMiliSec();
        }
    }
    return error;
}

s32 VFi_NandOpen(const char* path, struct NANDFileInfo* info, u8 accType) {
    s32 challenge;
    s32 error;

    challenge = VF_nand_retry_max;
    error = 0;
    while (challenge-- > 0) {
        error = NANDOpen(path, info, accType);
        if (error != NAND_RESULT_BUSY && error != NAND_RESULT_ALLOC_FAILED) {
            return error;
        } else {
            _SleepAfewMiliSec();
        }
    }
    return error;
}

s32 VFi_NandPrivateOpen(const char* path, struct NANDFileInfo* info, u8 accType) {
    s32 challenge;
    s32 error;

    challenge = VF_nand_retry_max;
    error = 0;
    while (challenge-- > 0) {
        error = NANDPrivateOpen(path, info, accType);
        if (error != NAND_RESULT_BUSY && error != NAND_RESULT_ALLOC_FAILED) {
            return error;
        } else {
            _SleepAfewMiliSec();
        }
    }
    return error;
}

static s32 VFi_NandWrite(struct NANDFileInfo* info, void* buf, u32 length) {
    s32 challenge;
    s32 error;

    challenge = VF_nand_retry_max;
    error = 0;
    while (challenge-- > 0) {
        error = NANDWrite(info, buf, length);
        if (error != NAND_RESULT_BUSY && error != NAND_RESULT_ALLOC_FAILED) {
            return error;
        } else {
            _SleepAfewMiliSec();
        }
    }
    return error;
}

static s32 VFi_NandSeek(struct NANDFileInfo* info, s32 offset, s32 whence) {
    s32 challenge;
    s32 error;

    challenge = VF_nand_retry_max;
    error = 0;
    while (challenge-- > 0) {
        error = NANDSeek(info, offset, whence);
        if (error != NAND_RESULT_BUSY && error != NAND_RESULT_ALLOC_FAILED) {
            return error;
        } else {
            _SleepAfewMiliSec();
        }
    }
    return error;
}

s32 VFi_NandRead(struct NANDFileInfo* info, void* buf, u32 length) {
    s32 challenge;
    s32 error;

    challenge = VF_nand_retry_max;
    error = 0;
    while (challenge-- > 0) {
        error = NANDRead(info, buf, length);
        if (error != NAND_RESULT_BUSY && error != NAND_RESULT_ALLOC_FAILED) {
            return error;
        } else {
            _SleepAfewMiliSec();
        }
    }
    return error;
}

s32 VFi_NandCreateDir(const char* path, u8 perm, u8 attr) {
    s32 challenge;
    s32 error;

    challenge = VF_nand_retry_max;
    error = 0;
    while (challenge-- > 0) {
        error = NANDCreateDir(path, perm, attr);
        if (error != NAND_RESULT_BUSY && error != NAND_RESULT_ALLOC_FAILED) {
            return error;
        } else {
            _SleepAfewMiliSec();
        }
    }
    return error;
}

s32 VFi_NandPrivateCreateDir(const char* path, u8 perm, u8 attr) {
    s32 challenge;
    s32 error;

    challenge = VF_nand_retry_max;
    error = 0;
    while (challenge-- > 0) {
        error = NANDPrivateCreateDir(path, perm, attr);
        if (error != NAND_RESULT_BUSY && error != NAND_RESULT_ALLOC_FAILED) {
            return error;
        } else {
            _SleepAfewMiliSec();
        }
    }
    return error;
}

s32 VFi_NandGetLength(struct NANDFileInfo* info, u32* length) {
    s32 challenge;
    s32 error;

    challenge = VF_nand_retry_max;
    error = 0;
    while (challenge-- > 0) {
        error = NANDGetLength(info, length);
        if (error != NAND_RESULT_BUSY && error != NAND_RESULT_ALLOC_FAILED) {
            return error;
        } else {
            _SleepAfewMiliSec();
        }
    }
    return error;
}

s32 VFi_NandOpenSp(const char* path, void* info, u8 accType, u32 i_handleIdx) {
    struct NANDFileInfo* info_p;
    s32 (*tmpOpen)(const char*, struct NANDFileInfo*, u8);

    info_p = (struct NANDFileInfo*)info;

    if (i_handleIdx < 26) {
        tmpOpen = (s32(*)(const char*, struct NANDFileInfo*, u8))l_nandFunc[i_handleIdx].open;
        if (tmpOpen != 0) {
            return tmpOpen(path, info_p, accType);
        }
        return VFi_NandOpen(path, info_p, accType);
    } else if (i_handleIdx == 0xFFFFFFF6) {
        return VFi_NandOpen(path, info_p, accType);
    } else {
        return VFi_NandPrivateOpen(path, info_p, accType);
    }
}

void VFi_NandSetNANDFuncNormal(u32 i_handleIdx) {
    if (i_handleIdx < 26) {
        l_nandFunc[i_handleIdx].create = VFi_NandCreate;
        l_nandFunc[i_handleIdx].open = VFi_NandOpen;
        l_nandFunc[i_handleIdx].createDir = VFi_NandCreateDir;
        l_nandFunc[i_handleIdx].delete = VFi_NandDelete;
    }
}

void VFi_NandSetNANDFuncEx(u32 i_handleIdx) {
    if (i_handleIdx < 26) {
        l_nandFunc[i_handleIdx].create = VFi_NandPrivateCreate;
        l_nandFunc[i_handleIdx].open = VFi_NandPrivateOpen;
        l_nandFunc[i_handleIdx].createDir = VFi_NandPrivateCreateDir;
        l_nandFunc[i_handleIdx].delete = VFi_NandPrivateDelete;
    }
}

s32 A32_NANDRead(struct NANDFileInfo* i_fileInfo_p, void* i_buf, u32 i_size) {
    u8 work[32] ATTRIBUTE_ALIGN(64);
    s32 NANDError;
    void* p_2nd;
    void* p_3rd;
    u32 size_1st;
    u32 size_2nd;
    u32 size_3rd;

    if ((i_size & 0x1F) != 0) {
        return 0;
    }

    dCommon_DevideBuff32(i_buf, i_size, &size_1st, &p_2nd, &size_2nd, &p_3rd, &size_3rd);

    if (size_1st == 0) {
        NANDError = VFi_NandRead(i_fileInfo_p, i_buf, i_size);
        if (NANDError < 0) {
            return NANDError;
        }
    } else {
        NANDError = VFi_NandRead(i_fileInfo_p, work, 32);
        if (NANDError < 0) {
            return NANDError;
        }

        VFipf_memcpy(i_buf, work, size_1st);

        NANDError = VFi_NandSeek(i_fileInfo_p, -(s32)size_3rd, 1);
        if (NANDError < 0) {
            return NANDError;
        }

        if (size_2nd != 0) {
            NANDError = VFi_NandRead(i_fileInfo_p, p_2nd, size_2nd);
            if (NANDError < 0) {
                return NANDError;
            }
        }

        if (size_3rd != 0) {
            NANDError = VFi_NandRead(i_fileInfo_p, work, 32);
            if (NANDError < 0) {
                return NANDError;
            }

            VFipf_memcpy(p_3rd, work, size_3rd);
        }
    }
    return i_size;
}

s32 A32_NANDWrite(struct NANDFileInfo* i_fileInfo_p, void* i_buf, u32 i_size, struct PDM_DISK* p_disk) {
    u8 work[32] ATTRIBUTE_ALIGN(64);
    s32 NANDError;
    void* p_2nd;
    void* p_3rd;
    u32 size_1st;
    u32 size_2nd;
    u32 size_3rd;
    u32 handleIdx;
    struct VF_HANDLE_TYPE* handle_p;

    handleIdx = dCommon_getHandleIdxFromDisk(p_disk);
    handle_p = (struct VF_HANDLE_TYPE*)VFSysGetHandleP(handleIdx);

    if ((i_size & 0x1F) != 0) {
        return 0;
    }

    dCommon_DevideBuff32(i_buf, i_size, &size_1st, &p_2nd, &size_2nd, &p_3rd, &size_3rd);

    if (size_1st == 0) {
        NANDError = VFi_NandWrite(i_fileInfo_p, i_buf, i_size);
        if (NANDError < 0) {
            return NANDError;
        }
    } else {
        VFipf_memcpy(work, i_buf, size_1st);

        NANDError = VFi_NandWrite(i_fileInfo_p, work, size_1st);
        if (NANDError < 0) {
            return NANDError;
        }

        NANDError = VFi_NandWrite(i_fileInfo_p, p_2nd, size_2nd + size_3rd);
        if (NANDError < 0) {
            return NANDError;
        }
    }

    return i_size;
}

static inline s32 VFi_NandCreate2(const char* path, u8 perm, u8 attr) {
    s32 error;
    s32 challenge;

    challenge = VF_nand_retry_max;
    error = 0;
    while (challenge-- > 0) {
        error = NANDCreate(path, perm, attr);
        if (error != NAND_RESULT_BUSY && error != NAND_RESULT_ALLOC_FAILED) {
            return error;
        } else {
            _SleepAfewMiliSec();
        }
    }
    return error;
}

static inline s32 VFi_NandCreateSp(const char* path, u8 perm, u8 attr, u32 i_handleIdx) {
    s32 (*tmpCreate)(const char*, u8, u8);

    if (i_handleIdx < 26) {
        tmpCreate = l_nandFunc[i_handleIdx].create;
        if (tmpCreate != 0) {
            return tmpCreate(path, perm, attr);
        }
        return VFi_NandCreate(path, perm, attr);
    } else if (i_handleIdx == 0xFFFFFFF6) {
        return VFi_NandCreate2(path, perm, attr);
    } else {
        return VFi_NandPrivateCreate(path, perm, attr);
    }
}

static inline s32 VFi_NandDeleteSp(const char* path, u32 i_handleIdx) {
    s32 (*tmpDelete)(const char*);

    if (i_handleIdx < 26) {
        tmpDelete = l_nandFunc[i_handleIdx].delete;
        if (tmpDelete != 0) {
            return tmpDelete(path);
        }
        return VFi_NandDelete(path);
    } else if (i_handleIdx == 0xFFFFFFF6) {
        return VFi_NandDelete(path);
    } else {
        return VFi_NandPrivateDelete(path);
    }
}

static inline u8 _ConvertPerm(u32 i_perm) {
    u8 perm;

    perm = 0;
    if (i_perm & 0x01) {
        perm |= NAND_PERM_OWNER_READ;
    }
    if (i_perm & 0x02) {
        perm |= NAND_PERM_OWNER_WRITE;
    }
    if (i_perm & 0x04) {
        perm |= NAND_PERM_RGRP;
    }
    if (i_perm & 0x08) {
        perm |= NAND_PERM_WGRP;
    }
    if (i_perm & 0x10) {
        perm |= NAND_PERM_ROTH;
    }
    if (i_perm & 0x20) {
        perm |= NAND_PERM_WOTH;
    }

    return perm;
}

#ifdef NON_MATCHING
s32 VFi_NandCreatePrfFileEx(u32 i_size, const s8* i_path_p, u32 i_version, u32 i_perm, u32 i_handleIdx) {
    struct NANDFileInfo fileInfo ATTRIBUTE_ALIGN(32);
    u8 buf[0x200] ATTRIBUTE_ALIGN(64);
    u8 perm;
    s16 createErr;
    s32 err;
    u32 rest;

    if ((i_size & 0x1F) != 0) {
        return -8;
    }

    perm = _ConvertPerm(i_perm);

    createErr = VFi_NandCreateSp((const char*)i_path_p, perm, 0, i_handleIdx);
    if (createErr != 0) {
        return createErr;
    }

    err = VFi_NandOpenSp((const char*)i_path_p, &fileInfo, NAND_ACCESS_WRITE, i_handleIdx);
    if (err != 0) {
        return (s16)err;
    }

    dCommon_CopyPrfFileHeader(buf, i_size, i_version, 0);
    err = VFi_NandWrite(&fileInfo, buf, 0x20);
    rest = i_size - 0x20;
    if (err < 0) {
        VFi_NandClose(&fileInfo);
        VFi_NandDeleteSp((const char*)i_path_p, i_handleIdx);
        return err;
    }

    VFipf_memset(buf, 0, 0x200);
    for (; rest >= 0x200; rest -= 0x200) {
        err = VFi_NandWrite(&fileInfo, buf, 0x200);
        if (err < 0) {
            VFi_NandClose(&fileInfo);
            VFi_NandDeleteSp((const char*)i_path_p, i_handleIdx);
            return err;
        }
    }

    if (rest != 0) {
        err = VFi_NandWrite(&fileInfo, buf, rest);
        if (err < 0) {
            VFi_NandClose(&fileInfo);
            VFi_NandDeleteSp((const char*)i_path_p, i_handleIdx);
            return err;
        }
    }

    VFi_NandClose(&fileInfo);
    return 0;
}
#else
// Register allocation differs from the C version above.
asm s32 VFi_NandCreatePrfFileEx(u32 i_size, const s8* i_path_p, u32 i_version, u32 i_perm, u32 i_handleIdx) {
    nofralloc
    clrlwi r11, r1, 26
    mr r12, r1
    subfic r11, r11, -0x340
    stwux r1, r1, r11
    mflr r0
    mr r11, r12
    stw r0, 0x4(r12)
    bl _savegpr_22
    clrlwi. r0, r3, 27
    mr r28, r3
    mr r29, r4
    mr r30, r5
    mr r31, r7
    beq L_80072618
    li r3, -0x8
    b L_80073320
L_80072618:
    clrlwi. r0, r6, 31
    li r26, 0x0
    beq L_80072628
    ori r26, r26, 0x10
L_80072628:
    rlwinm. r0, r6, 0, 30, 30
    beq L_80072638
    ori r0, r26, 0x20
    clrlwi r26, r0, 24
L_80072638:
    rlwinm. r0, r6, 0, 29, 29
    beq L_80072648
    ori r0, r26, 0x4
    clrlwi r26, r0, 24
L_80072648:
    rlwinm. r0, r6, 0, 28, 28
    beq L_80072658
    ori r0, r26, 0x8
    clrlwi r26, r0, 24
L_80072658:
    rlwinm. r0, r6, 0, 27, 27
    beq L_80072668
    ori r0, r26, 0x1
    clrlwi r26, r0, 24
L_80072668:
    rlwinm. r0, r6, 0, 26, 26
    beq L_80072678
    ori r0, r26, 0x2
    clrlwi r26, r0, 24
L_80072678:
    cmplwi r7, 0x1a
    bge L_8007273C
    lis r3, l_nandFunc@ha
    slwi r0, r7, 4
    addi r3, r3, l_nandFunc@l
    lwzx r12, r3, r0
    cmpwi r12, 0x0
    beq L_800726B0
    mr r3, r29
    clrlwi r4, r26, 24
    li r5, 0x0
    mtctr r12
    bctrl
    b L_8007285C
L_800726B0:
    lis r3, 0x1062
    lwz r24, VF_nand_retry_max(r13)
    addi r27, r3, 0x4dd3
    li r25, 0x0
    lis r23, 0x8000
    li r22, 0x0
    b L_80072728
L_800726CC:
    mr r3, r29
    clrlwi r4, r26, 24
    li r5, 0x0
    bl NANDCreate
    cmpwi r3, -0x3
    mr r25, r3
    beq L_800726F4
    cmpwi r3, -0x2
    beq L_800726F4
    b L_80072734
L_800726F4:
    lwz r0, 0xf8(r23)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r27, r0
    srawi r0, r6, 31
    mullw r4, r22, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_80072728:
    cmpwi r24, 0x0
    subi r24, r24, 0x1
    bgt L_800726CC
L_80072734:
    mr r3, r25
    b L_8007285C
L_8007273C:
    addis r0, r7, 0x1
    cmplwi r0, 0xfff6
    bne L_800727D4
    lis r3, 0x1062
    lwz r25, VF_nand_retry_max(r13)
    addi r27, r3, 0x4dd3
    li r24, 0x0
    lis r23, 0x8000
    li r22, 0x0
    b L_800727C0
L_80072764:
    mr r3, r29
    clrlwi r4, r26, 24
    li r5, 0x0
    bl NANDCreate
    cmpwi r3, -0x3
    mr r24, r3
    beq L_8007278C
    cmpwi r3, -0x2
    beq L_8007278C
    b L_800727CC
L_8007278C:
    lwz r0, 0xf8(r23)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r27, r0
    srawi r0, r6, 31
    mullw r4, r22, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_800727C0:
    cmpwi r25, 0x0
    subi r25, r25, 0x1
    bgt L_80072764
L_800727CC:
    mr r3, r24
    b L_8007285C
L_800727D4:
    lis r3, 0x1062
    lwz r25, VF_nand_retry_max(r13)
    addi r27, r3, 0x4dd3
    li r24, 0x0
    lis r23, 0x8000
    li r22, 0x0
    b L_8007284C
L_800727F0:
    mr r3, r29
    clrlwi r4, r26, 24
    li r5, 0x0
    bl NANDPrivateCreate
    cmpwi r3, -0x3
    mr r24, r3
    beq L_80072818
    cmpwi r3, -0x2
    beq L_80072818
    b L_80072858
L_80072818:
    lwz r0, 0xf8(r23)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r27, r0
    srawi r0, r6, 31
    mullw r4, r22, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_8007284C:
    cmpwi r25, 0x0
    subi r25, r25, 0x1
    bgt L_800727F0
L_80072858:
    mr r3, r24
L_8007285C:
    extsh. r3, r3
    beq L_80072868
    b L_80073320
L_80072868:
    cmplwi r31, 0x1a
    addi r26, r1, 0x40
    bge L_80072934
    lis r3, l_nandFunc@ha
    slwi r0, r31, 4
    addi r3, r3, l_nandFunc@l
    add r3, r3, r0
    lwz r12, 0x4(r3)
    cmpwi r12, 0x0
    beq L_800728A8
    mr r3, r29
    mr r4, r26
    li r5, 0x2
    mtctr r12
    bctrl
    b L_80072A54
L_800728A8:
    lis r3, 0x1062
    lwz r24, VF_nand_retry_max(r13)
    addi r27, r3, 0x4dd3
    li r25, 0x0
    lis r23, 0x8000
    li r22, 0x0
    b L_80072920
L_800728C4:
    mr r3, r29
    mr r4, r26
    li r5, 0x2
    bl NANDOpen
    cmpwi r3, -0x3
    mr r25, r3
    beq L_800728EC
    cmpwi r3, -0x2
    beq L_800728EC
    b L_8007292C
L_800728EC:
    lwz r0, 0xf8(r23)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r27, r0
    srawi r0, r6, 31
    mullw r4, r22, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_80072920:
    cmpwi r24, 0x0
    subi r24, r24, 0x1
    bgt L_800728C4
L_8007292C:
    mr r3, r25
    b L_80072A54
L_80072934:
    addis r0, r31, 0x1
    cmplwi r0, 0xfff6
    bne L_800729CC
    lis r3, 0x1062
    lwz r24, VF_nand_retry_max(r13)
    addi r27, r3, 0x4dd3
    li r25, 0x0
    lis r23, 0x8000
    li r22, 0x0
    b L_800729B8
L_8007295C:
    mr r3, r29
    mr r4, r26
    li r5, 0x2
    bl NANDOpen
    cmpwi r3, -0x3
    mr r25, r3
    beq L_80072984
    cmpwi r3, -0x2
    beq L_80072984
    b L_800729C4
L_80072984:
    lwz r0, 0xf8(r23)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r27, r0
    srawi r0, r6, 31
    mullw r4, r22, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_800729B8:
    cmpwi r24, 0x0
    subi r24, r24, 0x1
    bgt L_8007295C
L_800729C4:
    mr r3, r25
    b L_80072A54
L_800729CC:
    lis r3, 0x1062
    lwz r24, VF_nand_retry_max(r13)
    addi r27, r3, 0x4dd3
    li r25, 0x0
    lis r22, 0x8000
    li r23, 0x0
    b L_80072A44
L_800729E8:
    mr r3, r29
    mr r4, r26
    li r5, 0x2
    bl NANDPrivateOpen
    cmpwi r3, -0x3
    mr r25, r3
    beq L_80072A10
    cmpwi r3, -0x2
    beq L_80072A10
    b L_80072A50
L_80072A10:
    lwz r0, 0xf8(r22)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r27, r0
    srawi r0, r6, 31
    mullw r4, r23, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_80072A44:
    cmpwi r24, 0x0
    subi r24, r24, 0x1
    bgt L_800729E8
L_80072A50:
    mr r3, r25
L_80072A54:
    cmpwi r3, 0x0
    beq L_80072A64
    extsh r3, r3
    b L_80073320
L_80072A64:
    mr r4, r28
    mr r5, r30
    addi r3, r1, 0x100
    li r6, 0x0
    bl dCommon_CopyPrfFileHeader
    lis r3, 0x1062
    lwz r24, VF_nand_retry_max(r13)
    addi r30, r3, 0x4dd3
    li r25, 0x0
    lis r27, 0x8000
    li r26, 0x0
    b L_80072AF0
L_80072A94:
    addi r3, r1, 0x40
    addi r4, r1, 0x100
    li r5, 0x20
    bl NANDWrite
    cmpwi r3, -0x3
    mr r25, r3
    beq L_80072ABC
    cmpwi r3, -0x2
    beq L_80072ABC
    b L_80072AFC
L_80072ABC:
    lwz r0, 0xf8(r27)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r30, r0
    srawi r0, r6, 31
    mullw r4, r26, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_80072AF0:
    cmpwi r24, 0x0
    subi r24, r24, 0x1
    bgt L_80072A94
L_80072AFC:
    cmpwi r25, 0x0
    subi r26, r28, 0x20
    bge L_80072D28
    lis r3, 0x1062
    lwz r24, VF_nand_retry_max(r13)
    addi r28, r3, 0x4dd3
    lis r27, 0x8000
    li r26, 0x0
    b L_80072B70
L_80072B20:
    addi r3, r1, 0x40
    bl NANDClose
    cmpwi r3, -0x3
    beq L_80072B3C
    cmpwi r3, -0x2
    beq L_80072B3C
    b L_80072B7C
L_80072B3C:
    lwz r0, 0xf8(r27)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r28, r0
    srawi r0, r6, 31
    mullw r4, r26, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_80072B70:
    cmpwi r24, 0x0
    subi r24, r24, 0x1
    bgt L_80072B20
L_80072B7C:
    cmplwi r31, 0x1a
    bge L_80072C28
    lis r3, l_nandFunc@ha
    slwi r0, r31, 4
    addi r3, r3, l_nandFunc@l
    add r3, r3, r0
    lwz r12, 0xc(r3)
    cmpwi r12, 0x0
    beq L_80072BB0
    mr r3, r29
    mtctr r12
    bctrl
    b L_80072D20
L_80072BB0:
    lis r3, 0x1062
    lwz r22, VF_nand_retry_max(r13)
    addi r27, r3, 0x4dd3
    lis r26, 0x8000
    li r24, 0x0
    b L_80072C18
L_80072BC8:
    mr r3, r29
    bl NANDDelete
    cmpwi r3, -0x3
    beq L_80072BE4
    cmpwi r3, -0x2
    beq L_80072BE4
    b L_80072D20
L_80072BE4:
    lwz r0, 0xf8(r26)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r27, r0
    srawi r0, r6, 31
    mullw r4, r24, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_80072C18:
    cmpwi r22, 0x0
    subi r22, r22, 0x1
    bgt L_80072BC8
    b L_80072D20
L_80072C28:
    addis r0, r31, 0x1
    cmplwi r0, 0xfff6
    bne L_80072CAC
    lis r3, 0x1062
    lwz r22, VF_nand_retry_max(r13)
    addi r27, r3, 0x4dd3
    lis r26, 0x8000
    li r24, 0x0
    b L_80072C9C
L_80072C4C:
    mr r3, r29
    bl NANDDelete
    cmpwi r3, -0x3
    beq L_80072C68
    cmpwi r3, -0x2
    beq L_80072C68
    b L_80072D20
L_80072C68:
    lwz r0, 0xf8(r26)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r27, r0
    srawi r0, r6, 31
    mullw r4, r24, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_80072C9C:
    cmpwi r22, 0x0
    subi r22, r22, 0x1
    bgt L_80072C4C
    b L_80072D20
L_80072CAC:
    lis r3, 0x1062
    lwz r22, VF_nand_retry_max(r13)
    addi r27, r3, 0x4dd3
    lis r26, 0x8000
    li r24, 0x0
    b L_80072D14
L_80072CC4:
    mr r3, r29
    bl NANDPrivateDelete
    cmpwi r3, -0x3
    beq L_80072CE0
    cmpwi r3, -0x2
    beq L_80072CE0
    b L_80072D20
L_80072CE0:
    lwz r0, 0xf8(r26)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r27, r0
    srawi r0, r6, 31
    mullw r4, r24, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_80072D14:
    cmpwi r22, 0x0
    subi r22, r22, 0x1
    bgt L_80072CC4
L_80072D20:
    mr r3, r25
    b L_80073320
L_80072D28:
    addi r3, r1, 0x100
    li r4, 0x0
    li r5, 0x200
    bl VFipf_memset
    lis r3, 0x1062
    lis r28, 0x8000
    addi r30, r3, 0x4dd3
    li r27, 0x0
    b L_80072FEC
L_80072D4C:
    lwz r24, VF_nand_retry_max(r13)
    li r25, 0x0
    b L_80072DB4
L_80072D58:
    addi r3, r1, 0x40
    addi r4, r1, 0x100
    li r5, 0x200
    bl NANDWrite
    cmpwi r3, -0x3
    mr r25, r3
    beq L_80072D80
    cmpwi r3, -0x2
    beq L_80072D80
    b L_80072DC0
L_80072D80:
    lwz r0, 0xf8(r28)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r30, r0
    srawi r0, r6, 31
    mullw r4, r27, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_80072DB4:
    cmpwi r24, 0x0
    subi r24, r24, 0x1
    bgt L_80072D58
L_80072DC0:
    cmpwi r25, 0x0
    bge L_80072FE8
    lis r3, 0x1062
    lwz r24, VF_nand_retry_max(r13)
    addi r28, r3, 0x4dd3
    lis r27, 0x8000
    li r26, 0x0
    b L_80072E30
L_80072DE0:
    addi r3, r1, 0x40
    bl NANDClose
    cmpwi r3, -0x3
    beq L_80072DFC
    cmpwi r3, -0x2
    beq L_80072DFC
    b L_80072E3C
L_80072DFC:
    lwz r0, 0xf8(r27)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r28, r0
    srawi r0, r6, 31
    mullw r4, r26, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_80072E30:
    cmpwi r24, 0x0
    subi r24, r24, 0x1
    bgt L_80072DE0
L_80072E3C:
    cmplwi r31, 0x1a
    bge L_80072EE8
    lis r3, l_nandFunc@ha
    slwi r0, r31, 4
    addi r3, r3, l_nandFunc@l
    add r3, r3, r0
    lwz r12, 0xc(r3)
    cmpwi r12, 0x0
    beq L_80072E70
    mr r3, r29
    mtctr r12
    bctrl
    b L_80072FE0
L_80072E70:
    lis r3, 0x1062
    lwz r22, VF_nand_retry_max(r13)
    addi r27, r3, 0x4dd3
    lis r26, 0x8000
    li r24, 0x0
    b L_80072ED8
L_80072E88:
    mr r3, r29
    bl NANDDelete
    cmpwi r3, -0x3
    beq L_80072EA4
    cmpwi r3, -0x2
    beq L_80072EA4
    b L_80072FE0
L_80072EA4:
    lwz r0, 0xf8(r26)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r27, r0
    srawi r0, r6, 31
    mullw r4, r24, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_80072ED8:
    cmpwi r22, 0x0
    subi r22, r22, 0x1
    bgt L_80072E88
    b L_80072FE0
L_80072EE8:
    addis r0, r31, 0x1
    cmplwi r0, 0xfff6
    bne L_80072F6C
    lis r3, 0x1062
    lwz r22, VF_nand_retry_max(r13)
    addi r27, r3, 0x4dd3
    lis r26, 0x8000
    li r24, 0x0
    b L_80072F5C
L_80072F0C:
    mr r3, r29
    bl NANDDelete
    cmpwi r3, -0x3
    beq L_80072F28
    cmpwi r3, -0x2
    beq L_80072F28
    b L_80072FE0
L_80072F28:
    lwz r0, 0xf8(r26)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r27, r0
    srawi r0, r6, 31
    mullw r4, r24, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_80072F5C:
    cmpwi r22, 0x0
    subi r22, r22, 0x1
    bgt L_80072F0C
    b L_80072FE0
L_80072F6C:
    lis r3, 0x1062
    lwz r22, VF_nand_retry_max(r13)
    addi r27, r3, 0x4dd3
    lis r26, 0x8000
    li r24, 0x0
    b L_80072FD4
L_80072F84:
    mr r3, r29
    bl NANDPrivateDelete
    cmpwi r3, -0x3
    beq L_80072FA0
    cmpwi r3, -0x2
    beq L_80072FA0
    b L_80072FE0
L_80072FA0:
    lwz r0, 0xf8(r26)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r27, r0
    srawi r0, r6, 31
    mullw r4, r24, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_80072FD4:
    cmpwi r22, 0x0
    subi r22, r22, 0x1
    bgt L_80072F84
L_80072FE0:
    mr r3, r25
    b L_80073320
L_80072FE8:
    subi r26, r26, 0x200
L_80072FEC:
    cmplwi r26, 0x200
    bge L_80072D4C
    cmpwi r26, 0x0
    beq L_800732A8
    lis r3, 0x1062
    lwz r24, VF_nand_retry_max(r13)
    addi r30, r3, 0x4dd3
    li r25, 0x0
    lis r28, 0x8000
    li r27, 0x0
    b L_80073074
L_80073018:
    mr r5, r26
    addi r3, r1, 0x40
    addi r4, r1, 0x100
    bl NANDWrite
    cmpwi r3, -0x3
    mr r25, r3
    beq L_80073040
    cmpwi r3, -0x2
    beq L_80073040
    b L_80073080
L_80073040:
    lwz r0, 0xf8(r28)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r30, r0
    srawi r0, r6, 31
    mullw r4, r27, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_80073074:
    cmpwi r24, 0x0
    subi r24, r24, 0x1
    bgt L_80073018
L_80073080:
    cmpwi r25, 0x0
    bge L_800732A8
    lis r3, 0x1062
    lwz r24, VF_nand_retry_max(r13)
    addi r28, r3, 0x4dd3
    lis r27, 0x8000
    li r26, 0x0
    b L_800730F0
L_800730A0:
    addi r3, r1, 0x40
    bl NANDClose
    cmpwi r3, -0x3
    beq L_800730BC
    cmpwi r3, -0x2
    beq L_800730BC
    b L_800730FC
L_800730BC:
    lwz r0, 0xf8(r27)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r28, r0
    srawi r0, r6, 31
    mullw r4, r26, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_800730F0:
    cmpwi r24, 0x0
    subi r24, r24, 0x1
    bgt L_800730A0
L_800730FC:
    cmplwi r31, 0x1a
    bge L_800731A8
    lis r3, l_nandFunc@ha
    slwi r0, r31, 4
    addi r3, r3, l_nandFunc@l
    add r3, r3, r0
    lwz r12, 0xc(r3)
    cmpwi r12, 0x0
    beq L_80073130
    mr r3, r29
    mtctr r12
    bctrl
    b L_800732A0
L_80073130:
    lis r3, 0x1062
    lwz r22, VF_nand_retry_max(r13)
    addi r27, r3, 0x4dd3
    lis r26, 0x8000
    li r24, 0x0
    b L_80073198
L_80073148:
    mr r3, r29
    bl NANDDelete
    cmpwi r3, -0x3
    beq L_80073164
    cmpwi r3, -0x2
    beq L_80073164
    b L_800732A0
L_80073164:
    lwz r0, 0xf8(r26)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r27, r0
    srawi r0, r6, 31
    mullw r4, r24, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_80073198:
    cmpwi r22, 0x0
    subi r22, r22, 0x1
    bgt L_80073148
    b L_800732A0
L_800731A8:
    addis r0, r31, 0x1
    cmplwi r0, 0xfff6
    bne L_8007322C
    lis r3, 0x1062
    lwz r22, VF_nand_retry_max(r13)
    addi r27, r3, 0x4dd3
    lis r26, 0x8000
    li r24, 0x0
    b L_8007321C
L_800731CC:
    mr r3, r29
    bl NANDDelete
    cmpwi r3, -0x3
    beq L_800731E8
    cmpwi r3, -0x2
    beq L_800731E8
    b L_800732A0
L_800731E8:
    lwz r0, 0xf8(r26)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r27, r0
    srawi r0, r6, 31
    mullw r4, r24, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_8007321C:
    cmpwi r22, 0x0
    subi r22, r22, 0x1
    bgt L_800731CC
    b L_800732A0
L_8007322C:
    lis r3, 0x1062
    lwz r22, VF_nand_retry_max(r13)
    addi r27, r3, 0x4dd3
    lis r26, 0x8000
    li r24, 0x0
    b L_80073294
L_80073244:
    mr r3, r29
    bl NANDPrivateDelete
    cmpwi r3, -0x3
    beq L_80073260
    cmpwi r3, -0x2
    beq L_80073260
    b L_800732A0
L_80073260:
    lwz r0, 0xf8(r26)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r27, r0
    srawi r0, r6, 31
    mullw r4, r24, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_80073294:
    cmpwi r22, 0x0
    subi r22, r22, 0x1
    bgt L_80073244
L_800732A0:
    mr r3, r25
    b L_80073320
L_800732A8:
    lis r3, 0x1062
    lwz r22, VF_nand_retry_max(r13)
    addi r26, r3, 0x4dd3
    lis r25, 0x8000
    li r24, 0x0
    b L_80073310
L_800732C0:
    addi r3, r1, 0x40
    bl NANDClose
    cmpwi r3, -0x3
    beq L_800732DC
    cmpwi r3, -0x2
    beq L_800732DC
    b L_8007331C
L_800732DC:
    lwz r0, 0xf8(r25)
    lwz r6, VF_nand_sleep_msec(r13)
    srwi r0, r0, 2
    mulhwu r3, r26, r0
    srawi r0, r6, 31
    mullw r4, r24, r6
    srwi r5, r3, 6
    mulhwu r3, r5, r6
    mullw r0, r5, r0
    add r3, r3, r4
    mullw r4, r5, r6
    add r3, r3, r0
    bl OSSleepTicks
L_80073310:
    cmpwi r22, 0x0
    subi r22, r22, 0x1
    bgt L_800732C0
L_8007331C:
    li r3, 0x0
L_80073320:
    lwz r10, 0x0(r1)
    mr r11, r10
    bl _restgpr_22
    lwz r0, 0x4(r10)
    mtlr r0
    mr r1, r10
    blr
}
#endif

s32 VFi_NandCreatePrfFile(u32 i_size, const s8* i_path_p, u32 i_version) {
    return VFi_NandCreatePrfFileEx(i_size, i_path_p, i_version, 0x3F, -11);
}

s32 VFi_NandFlushNANDFromHandleIdx(s32 i_handleIdx, int i_setLastDeviceError) {
    s32 NANDError;
    struct VF_HANDLE_DRIVE* drive_p;
    struct VF_HANDLE_TYPE* handle_p;
    const char* sys_name_p;
    struct NANDFileInfo* fileInfo_p;

    drive_p = (struct VF_HANDLE_DRIVE*)VFSysGetDriveP(i_handleIdx);
    handle_p = (struct VF_HANDLE_TYPE*)VFSysGetHandleP(i_handleIdx);

    if (handle_p != 0 && handle_p->device_p != 0 && handle_p->device_p->sync_mode == 1) {
        return 0;
    }

    if (drive_p != 0) {
        sys_name_p = (const char*)drive_p->pf_filename;
        fileInfo_p = drive_p->file_p;
        NANDError = VFi_NandClose(fileInfo_p);

        if (NANDError < 0) {
            if (i_setLastDeviceError != 0) {
                dCommon_setLastDeviceErrorToDisk2(i_handleIdx, NANDError);
            }
            return NANDError;
        }

        if ((u32)i_handleIdx < 26) {
            s32 (*tmpOpen)(const char*, struct NANDFileInfo*, u8) = l_nandFunc[i_handleIdx].open;
            if (tmpOpen != 0) {
                NANDError = tmpOpen(sys_name_p, fileInfo_p, 3);
            } else {
                NANDError = VFi_NandOpen(sys_name_p, fileInfo_p, 3);
            }
        } else if (i_handleIdx == 0xFFFFFFF6) {
            NANDError = VFi_NandOpen(sys_name_p, fileInfo_p, 3);
        } else {
            NANDError = VFi_NandPrivateOpen(sys_name_p, fileInfo_p, 3);
        }

        if (NANDError < 0) {
            if (i_setLastDeviceError != 0) {
                dCommon_setLastDeviceErrorToDisk2(i_handleIdx, NANDError);
            }
            return NANDError;
        }
    }
    return 0;
}

s32 _MountPrfFile(struct PDM_DISK* p_disk, s8* i_fullpath_p) {
    struct PR_BINHEADER header ATTRIBUTE_ALIGN(64);
    struct VF_HANDLE_DRIVE* drive_p;
    struct NANDFileInfo* fileInfo_p;
    s32 nandError;
    u32 handleIdx;
    u32 fileSize;
    u32 dataSize;
    u32 SPU;  // Present in DWARF but unused here.
    enum FatType fatType;

    drive_p = (struct VF_HANDLE_DRIVE*)VFSysPDMDisk2DriveP(p_disk);
    fileInfo_p = 0;
    nandError = 0;
    handleIdx = dCommon_getHandleIdxFromDisk(p_disk);

    if (drive_p == 0) {
        return -20;
    }

    fileInfo_p = drive_p->file_p;

    nandError = VFi_NandOpenSp((const char*)i_fullpath_p, fileInfo_p, 1, handleIdx);
    if (nandError == 0) {
        VFipf_memset(&header, 0, 0x20);
        nandError = A32_NANDRead(fileInfo_p, &header, 0x20);
        if (nandError < 0) {
            VFi_NandClose(fileInfo_p);
            dCommon_setLastDeviceErrorToDisk(p_disk, nandError);
            return nandError;
        }

        dCommon_PrintSignature(&header);
        if (dCommon_IsPrfFile(&header) == 0) {
            VFi_NandClose(fileInfo_p);
            return -1;
        }

        dCommon_setFileSizeToDisk(p_disk, (header.fileSize[0] << 24) | (header.fileSize[1] << 16) | (header.fileSize[2] << 8) | header.fileSize[3]);
        fileSize = dCommon_getFileSizeFromDisk(p_disk);
        dataSize = (fileSize - 0x1F) >> 9;
        fatType = dCommon_GetNiceFatType(0, dataSize, 1, 0x200);
        dCommon_setFatTypeToDisk(p_disk, fatType);
        dCommon_setResvSecNumToDisk(p_disk, dCommon_GetReservedSecFromFatType(fatType));
        dCommon_setRootEntNumToDisk(p_disk, dCommon_GetRootEntNumFromFatType(fatType));

        VFi_NandClose(fileInfo_p);
    } else {
        dCommon_setLastDeviceErrorToDisk(p_disk, nandError);
        return nandError;
    }

    nandError = VFi_NandOpenSp((const char*)i_fullpath_p, fileInfo_p, 3, handleIdx);
    if (nandError == 0) {
        return 0;
    }

    dCommon_setLastDeviceErrorToDisk(p_disk, nandError);
    return nandError;
}

static u16 _UnmountPrfFile(struct PDM_DISK* p_disk) {
    struct VF_HANDLE_DRIVE* drive_p;
    s32 nandError;
    struct NANDFileInfo* fileInfo_p;

    drive_p = (struct VF_HANDLE_DRIVE*)VFSysPDMDisk2DriveP(p_disk);
    if (drive_p != 0) {
        fileInfo_p = drive_p->file_p;
        nandError = VFi_NandClose(fileInfo_p);
        if (nandError == 0) {
            return 0;
        }
        dCommon_setLastDeviceErrorToDisk(p_disk, nandError);
    }
    return 0xFFFF;
}

s32 nanddrv_BuildUpFSInfoSector(u8* buf) {
    s32 err = dCommon_MakeFsInfoSec(buf, 0x200);
    return (err == 0) ? 0 : -21;
}

s32 nanddrv_BuildUpBootSector(struct PDM_DISK* p_disk, u8* buf, enum FatType* type) {
    struct PDM_DISK* realDisk_p;
    u32 drvSPU;

    realDisk_p = (struct PDM_DISK*)((u8*)&VFipdm_disk_set + ((u32)p_disk & 0xFF) * sizeof(struct PDM_DISK));
    drvSPU = *(u32*)((u8*)realDisk_p + 0x1BC);

    dCommon_MakeBootSector(buf, type, drvSPU, 1, 0x200, 0xEB, 0x90, 0x3F, dCommon_getResvSecNumFromDisk(p_disk),
                           dCommon_getRootEntNumFromDisk(p_disk), 0xF0);
    dCommon_setFatTypeToDisk(p_disk, *type);
    return 0;
}

s32 nanddrv_init(struct PDM_DISK* p_disk) {
    if (p_disk == 0) {
        return -20;
    }
    dCommon_setFatTypeToDisk(p_disk, 1);
    VFipdm_disk_notify_media_insert(p_disk);
    return 0;
}

s32 nanddrv_mount(struct PDM_DISK* p_disk) {
    struct VF_HANDLE_DRIVE* drive_p;
    s32 err;
    if (p_disk == 0) {
        return -20;
    }
    drive_p = (struct VF_HANDLE_DRIVE*)VFSysPDMDisk2DriveP(p_disk);
    if (drive_p == 0) {
        return -20;
    }
    err = _MountPrfFile(p_disk, (s8*)drive_p->pf_filename);
    return err == 0 ? 0 : err;
}

s32 nanddrv_format(struct PDM_DISK* p_disk, const u8* param) {
    return p_disk ? 0 : -20;
}

s32 nanddrv_pread(struct PDM_DISK* p_disk, u8* p_buf, u32 block, u32 num_blocks, u32* p_num_success) {
    *p_num_success = 0;
    if (p_disk == 0 || p_buf == 0 || num_blocks == 0 || p_num_success == 0) {
        return -20;
    }
    return nanddrv_physical_read(num_blocks, p_buf, block, 0x200, p_num_success, p_disk);
}

s32 nanddrv_pwrite(struct PDM_DISK* p_disk, const u8* p_buf, u32 block, u32 num_blocks, u32* p_num_success) {
    *p_num_success = 0;
    if (p_disk == 0 || p_buf == 0 || num_blocks == 0 || p_num_success == 0) {
        return -20;
    }
    return nanddrv_physical_write(num_blocks, p_buf, block, 0x200, p_num_success, p_disk);
}

s32 nanddrv_unmount(struct PDM_DISK* p_disk) {
    u16 nandError;

    if (p_disk == 0) {
        return -20;
    }

    nandError = _UnmountPrfFile(p_disk);
    if (nandError == 0) {
        return 0;
    }
    return nandError;
}

s32 nanddrv_finalize(struct PDM_DISK* p_disk) {
    if (p_disk == 0) {
        return -20;
    }
    dCommon_setFatTypeToDisk(p_disk, 1);
    return 0;
}

s32 nanddrv_get_disk_info(struct PDM_DISK* p_disk, struct PDM_DISK_INFO* p_disk_info) {
    u32 fileSize;
    u32 dataSize;

    if (p_disk == 0 || p_disk_info == 0) {
        return -20;
    }

    fileSize = dCommon_getFileSizeFromDisk(p_disk);
    dataSize = (fileSize - 0x1F) >> 9;

    p_disk_info->total_sectors = dataSize;
    p_disk_info->cylinders = dataSize / 255 / 63 / 512;
    p_disk_info->heads = 0xFF;
    p_disk_info->sectors_per_track = 0x3F;
    p_disk_info->bytes_per_sector = 0x200;
    p_disk_info->media_attr = 0;
    p_disk_info->format_param = 0;

    return 0;
}

s32 VFi_nanddrv_init_drv_tbl(struct PDM_DISK_TBL* p_disk_tbl, u32 ui_ext) {
    p_disk_tbl->p_func = (struct PDM_FUNCTBL*)&l_nand_func;
    p_disk_tbl->ui_ext = ui_ext;
    return 0;
}

s32 nanddrv_physical_read(u32 num_blocks, u8* buf, u32 block, u32 bps, u32* p_num_success, struct PDM_DISK* p_disk) {
    u32 fileSize;
    u32 size;
    s32 err;
    u32 offset;
    s32 nandError;
    struct VF_HANDLE_DRIVE* drive_p;
    struct NANDFileInfo* fileInfo_p;

    fileSize = dCommon_getFileSizeFromDisk(p_disk);
    drive_p = (struct VF_HANDLE_DRIVE*)VFSysPDMDisk2DriveP(p_disk);

    if (drive_p == 0) {
        return -20;
    }

    fileInfo_p = drive_p->file_p;

    if (!dCommon_ReadDummyBPB(num_blocks, buf, block, p_num_success, p_disk, &err, nanddrv_BuildUpBootSector, nanddrv_BuildUpFSInfoSector)) {
        return err;
    }

    size = num_blocks * bps;
    offset = dCommon_GetPhysicalOffset(block, bps, dCommon_getResvSecNumFromDisk(p_disk));

    nandError = VFi_NandSeek(fileInfo_p, offset, 0);

    if (nandError == offset) {
        if (offset + size > fileSize) {
            return -22;
        }

        nandError = A32_NANDRead(fileInfo_p, buf, size);
        if (nandError == size) {
            *p_num_success = num_blocks;
            return 0;
        }
    }

    dCommon_setLastDeviceErrorToDisk(p_disk, nandError);
    return nandError;
}

s32 nanddrv_physical_write(u32 num_blocks, const u8* buf, u32 block, u32 bps, u32* p_num_success, struct PDM_DISK* p_disk) {
    u32 fileSize;
    u32 size;
    u32 offset;
    s32 nandError;
    struct VF_HANDLE_DRIVE* drive_p;
    struct NANDFileInfo* fileInfo_p;
    s32 err;

    fileSize = dCommon_getFileSizeFromDisk(p_disk);
    drive_p = (struct VF_HANDLE_DRIVE*)VFSysPDMDisk2DriveP(p_disk);
    err = 0;

    if (drive_p == 0) {
        return -20;
    }

    fileInfo_p = drive_p->file_p;

    if (!dCommon_WriteDummyBPB(num_blocks, block, p_num_success, p_disk, &err)) {
        return err;
    }

    size = num_blocks * bps;
    offset = dCommon_GetPhysicalOffset(block, bps, dCommon_getResvSecNumFromDisk(p_disk));

    nandError = VFi_NandSeek(fileInfo_p, offset, 0);

    if (nandError == offset) {
        if (offset + size > fileSize) {
            return -22;
        }

        nandError = A32_NANDWrite(fileInfo_p, (void*)buf, size, p_disk);
        if (nandError == size) {
            *p_num_success = num_blocks;
            return 0;
        }
    }

    dCommon_setLastDeviceErrorToDisk(p_disk, nandError);
    return nandError;
}
