#include "revolution/vf/d_common.h"
#include "revolution/vf/d_vf_sys.h"
#include "revolution/vf/nand_drv.h"
#include "revolution/vf/vf_struct.h"
#include "revolution/vf/pf_clib.h"

// RAM disk driver (VF device type 1): the PrFILE2 image lives in a memory block
// given to VFMountDriveRAM. Not present in the Wii Sports or Super Mario Galaxy builds;
// the file name and function names are modelled on nand_drv.c.

extern struct PDM_DISK_SET VFipdm_disk_set;

// CONFLICT (vf_struct.h): PDM_DISK is 0x34 bytes in this SDK (no p_erase_func); the shared header
// still has Petari's 0x38-byte layout (VF part 1 updates it).
#define VF_PDM_DISK_SIZE 0x34

#ifndef NON_MATCHING
void _savegpr_23(void);
void _restgpr_23(void);
#endif

s32 ramdrv_init(struct PDM_DISK* p_disk);
s32 ramdrv_finalize(struct PDM_DISK* p_disk);
s32 ramdrv_mount(struct PDM_DISK* p_disk);
s32 ramdrv_unmount(struct PDM_DISK* p_disk);
s32 ramdrv_format(struct PDM_DISK* p_disk, const u8* param);
s32 ramdrv_pread(struct PDM_DISK* p_disk, u8* p_buf, u32 block, u32 num_blocks, u32* p_num_success);
s32 ramdrv_pwrite(struct PDM_DISK* p_disk, const u8* p_buf, u32 block, u32 num_blocks, u32* p_num_success);
s32 ramdrv_get_disk_info(struct PDM_DISK* p_disk, struct PDM_DISK_INFO* p_disk_info);
s32 ramdrv_physical_read(u32 num_blocks, u8* buf, u32 block, u32 bps, u32* p_num_success, struct PDM_DISK* p_disk);
s32 ramdrv_physical_write(u32 num_blocks, const u8* buf, u32 block, u32 bps, u32* p_num_success, struct PDM_DISK* p_disk);

static const struct PDM_FUNCTBL l_ram_func = {ramdrv_init,
                                              ramdrv_finalize,
                                              ramdrv_mount,
                                              ramdrv_unmount,
                                              (s32(*)(struct PDM_DISK*, u8*))ramdrv_format,
                                              ramdrv_pread,
                                              (s32(*)(struct PDM_DISK*, u8*, u32, u32, u32*))ramdrv_pwrite,
                                              (s32(*)(struct PDM_DISK*, struct PDM_DISK_INFO*))ramdrv_get_disk_info};

s32 ramdrv_BuildUpFSInfoSector(u8* buf) {
    s32 err = dCommon_MakeFsInfoSec(buf, 0x200);
    return (err == 0) ? 0 : -21;
}

s32 ramdrv_BuildUpBootSector(struct PDM_DISK* p_disk, u8* buf, enum FatType* type) {
    struct PDM_DISK* realDisk_p;
    u32 drvSPU;

    realDisk_p = (struct PDM_DISK*)((u8*)&VFipdm_disk_set + ((u32)p_disk & 0xFF) * VF_PDM_DISK_SIZE);
    drvSPU = *(u32*)((u8*)realDisk_p + 0x1BC);

    dCommon_MakeBootSector(buf, type, drvSPU, 1, 0x200, 0xEB, 0x90, 0x3F, dCommon_getResvSecNumFromDisk(p_disk),
                           dCommon_getRootEntNumFromDisk(p_disk), 0xF0);
    dCommon_setFatTypeToDisk(p_disk, *type);
    return 0;
}

s32 ramdrv_init(struct PDM_DISK* p_disk) {
    if (p_disk == 0) {
        return -20;
    }
    dCommon_setFatTypeToDisk(p_disk, 1);
    VFipdm_disk_notify_media_insert(p_disk);
    return 0;
}

s32 ramdrv_mount(struct PDM_DISK* p_disk) {
    struct VF_HANDLE_DRIVE* drive_p;
    struct PR_BINHEADER header;
    u32 fileSize;
    enum FatType fatType;

    fileSize = dCommon_getFileSizeFromDisk(p_disk);
    if (p_disk == 0) {
        return -20;
    }
    drive_p = VFSysPDMDisk2DriveP(p_disk);
    if (drive_p == 0) {
        return -20;
    }

    VFipf_memcpy(&header, drive_p->file_p, 0x20);
    if (dCommon_IsPrfFile(&header)) {
        dCommon_setFileSizeToDisk(p_disk, ((u32)header.fileSize[3] | ((u32)header.fileSize[2] << 8)) |
                                              (((u32)header.fileSize[0] << 24) | ((u32)header.fileSize[1] << 16)));
    } else {
        dCommon_setFileSizeToDisk(p_disk, fileSize);
    }

    fileSize = dCommon_getFileSizeFromDisk(p_disk);
    fatType = dCommon_GetNiceFatType(NULL, (fileSize - 0x1F) >> 9, 1, 0x200);
    dCommon_setFatTypeToDisk(p_disk, fatType);
    dCommon_setResvSecNumToDisk(p_disk, dCommon_GetReservedSecFromFatType(fatType));
    dCommon_setRootEntNumToDisk(p_disk, dCommon_GetRootEntNumFromFatType(fatType));
    return 0;
}

s32 ramdrv_format(struct PDM_DISK* p_disk, const u8* param) {
    if (p_disk == 0) {
        return -20;
    }
    return VFSysPDMDisk2DriveP(p_disk) ? 0 : -20;
}

// FAKEMATCH? The original does not inline ramdrv_physical_read here.
#pragma dont_inline on
s32 ramdrv_pread(struct PDM_DISK* p_disk, u8* p_buf, u32 block, u32 num_blocks, u32* p_num_success) {
    *p_num_success = 0;
    if (p_disk == 0 || p_buf == 0 || num_blocks == 0 || p_num_success == 0) {
        return -20;
    }
    return ramdrv_physical_read(num_blocks, p_buf, block, 0x200, p_num_success, p_disk);
}
#pragma dont_inline reset

s32 ramdrv_pwrite(struct PDM_DISK* p_disk, const u8* p_buf, u32 block, u32 num_blocks, u32* p_num_success) {
    *p_num_success = 0;
    if (p_disk == 0 || p_buf == 0 || num_blocks == 0 || p_num_success == 0) {
        return -20;
    }
    return ramdrv_physical_write(num_blocks, p_buf, block, 0x200, p_num_success, p_disk);
}

s32 ramdrv_unmount(struct PDM_DISK* p_disk) {
    return p_disk ? 0 : -20;
}

s32 ramdrv_finalize(struct PDM_DISK* p_disk) {
    if (p_disk == 0) {
        return -20;
    }
    dCommon_setFatTypeToDisk(p_disk, 1);
    return 0;
}

s32 ramdrv_get_disk_info(struct PDM_DISK* p_disk, struct PDM_DISK_INFO* p_disk_info) {
    struct VF_HANDLE_DRIVE* drive_p;
    struct PR_BINHEADER header;
    u32 fileSize;
    u32 dataSize;
    void* memory_p;

    if (p_disk == 0 || p_disk_info == 0) {
        return -20;
    }
    drive_p = VFSysPDMDisk2DriveP(p_disk);
    if (drive_p == 0) {
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

    memory_p = drive_p->file_p;
    VFipf_memcpy(&header, memory_p, 0x20);
    if (!dCommon_IsPrfFile(&header) || header.volatile_memory == 1) {
        p_disk_info->media_attr |= 2;
        dCommon_SetPrfFileVolatile(memory_p, 0);
    }
    p_disk_info->format_param = 0;

    return 0;
}

s32 VFi_ramdrv_init_drv_tbl(struct PDM_DISK_TBL* p_disk_tbl, u32 ui_ext) {
    p_disk_tbl->p_func = (struct PDM_FUNCTBL*)&l_ram_func;
    p_disk_tbl->ui_ext = ui_ext;
    return 0;
}

#ifdef NON_MATCHING
s32 ramdrv_physical_read(u32 num_blocks, u8* buf, u32 block, u32 bps, u32* p_num_success, struct PDM_DISK* p_disk) {
    u32 fileSize;
    u32 size;
    s32 err;
    u32 offset;
    struct VF_HANDLE_DRIVE* drive_p;
    u8* memory_p;
    u8* src_p;

    fileSize = dCommon_getFileSizeFromDisk(p_disk);
    drive_p = VFSysPDMDisk2DriveP(p_disk);

    if (drive_p == 0) {
        return -20;
    }

    memory_p = drive_p->file_p;

    if (!dCommon_ReadDummyBPB(num_blocks, buf, block, p_num_success, p_disk, &err, ramdrv_BuildUpBootSector, ramdrv_BuildUpFSInfoSector)) {
        return err;
    }

    size = num_blocks * bps;
    offset = dCommon_GetPhysicalOffset(block, bps, dCommon_getResvSecNumFromDisk(p_disk));
    src_p = memory_p + offset;
    if (offset + size > fileSize) {
        return -22;
    }

    VFipf_memcpy(buf, src_p, size);
    *p_num_success = num_blocks;
    return 0;
}
#else
// Register allocation differs from the C version above.
asm s32 ramdrv_physical_read(u32 num_blocks, u8* buf, u32 block, u32 bps, u32* p_num_success, struct PDM_DISK* p_disk) {
    nofralloc
    stwu r1, -0x40(r1)
    mflr r0
    stw r0, 0x44(r1)
    addi r11, r1, 0x40
    bl _savegpr_23
    mr r28, r8
    mr r23, r3
    mr r24, r4
    mr r25, r5
    mr r26, r6
    mr r27, r7
    mr r3, r28
    bl dCommon_getFileSizeFromDisk
    mr r31, r3
    mr r3, r28
    bl VFSysPDMDisk2DriveP
    cmpwi r3, 0x0
    bne L_80074958
    li r3, -0x14
    b L_800749E0
L_80074958:
    lwz r30, 0x0(r3)
    lis r9, ramdrv_BuildUpBootSector@ha
    lis r10, ramdrv_BuildUpFSInfoSector@ha
    mr r3, r23
    mr r4, r24
    mr r5, r25
    mr r6, r27
    mr r7, r28
    addi r8, r1, 0x8
    addi r9, r9, ramdrv_BuildUpBootSector@l
    addi r10, r10, ramdrv_BuildUpFSInfoSector@l
    bl dCommon_ReadDummyBPB
    cmpwi r3, 0x0
    bne L_80074998
    lwz r3, 0x8(r1)
    b L_800749E0
L_80074998:
    mullw r29, r23, r26
    mr r3, r28
    bl dCommon_getResvSecNumFromDisk
    mr r5, r3
    mr r3, r25
    mr r4, r26
    bl dCommon_GetPhysicalOffset
    add r0, r3, r29
    add r4, r30, r3
    cmplw r0, r31
    ble L_800749CC
    li r3, -0x16
    b L_800749E0
L_800749CC:
    mr r3, r24
    mr r5, r29
    bl VFipf_memcpy
    stw r23, 0x0(r27)
    li r3, 0x0
L_800749E0:
    addi r11, r1, 0x40
    bl _restgpr_23
    lwz r0, 0x44(r1)
    mtlr r0
    addi r1, r1, 0x40
    blr
}
#endif

#ifdef NON_MATCHING
s32 ramdrv_physical_write(u32 num_blocks, const u8* buf, u32 block, u32 bps, u32* p_num_success, struct PDM_DISK* p_disk) {
    u32 fileSize;
    u32 size;
    u32 offset;
    struct VF_HANDLE_DRIVE* drive_p;
    u8* memory_p;
    u8* dst_p;
    s32 err;

    fileSize = dCommon_getFileSizeFromDisk(p_disk);
    drive_p = VFSysPDMDisk2DriveP(p_disk);
    err = 0;

    if (drive_p == 0) {
        return -20;
    }

    memory_p = drive_p->file_p;

    if (!dCommon_WriteDummyBPB(num_blocks, block, p_num_success, p_disk, &err)) {
        return err;
    }

    size = num_blocks * bps;
    offset = dCommon_GetPhysicalOffset(block, bps, dCommon_getResvSecNumFromDisk(p_disk));
    dst_p = memory_p + offset;
    if (offset + size > fileSize) {
        return -22;
    }

    VFipf_memcpy(dst_p, (void*)buf, size);
    *p_num_success = num_blocks;
    return 0;
}
#else
// Register allocation differs from the C version above.
asm s32 ramdrv_physical_write(u32 num_blocks, const u8* buf, u32 block, u32 bps, u32* p_num_success, struct PDM_DISK* p_disk) {
    nofralloc
    stwu r1, -0x40(r1)
    mflr r0
    stw r0, 0x44(r1)
    addi r11, r1, 0x40
    bl _savegpr_23
    mr r28, r8
    mr r23, r3
    mr r24, r4
    mr r25, r5
    mr r26, r6
    mr r27, r7
    mr r3, r28
    bl dCommon_getFileSizeFromDisk
    mr r31, r3
    mr r3, r28
    bl VFSysPDMDisk2DriveP
    li r0, 0x0
    cmpwi r3, 0x0
    stw r0, 0x8(r1)
    bne L_80074A50
    li r3, -0x14
    b L_80074AC4
L_80074A50:
    lwz r30, 0x0(r3)
    mr r3, r23
    mr r4, r25
    mr r5, r27
    mr r6, r28
    addi r7, r1, 0x8
    bl dCommon_WriteDummyBPB
    cmpwi r3, 0x0
    bne L_80074A7C
    lwz r3, 0x8(r1)
    b L_80074AC4
L_80074A7C:
    mullw r29, r23, r26
    mr r3, r28
    bl dCommon_getResvSecNumFromDisk
    mr r5, r3
    mr r3, r25
    mr r4, r26
    bl dCommon_GetPhysicalOffset
    add r0, r3, r29
    add r3, r30, r3
    cmplw r0, r31
    ble L_80074AB0
    li r3, -0x16
    b L_80074AC4
L_80074AB0:
    mr r4, r24
    mr r5, r29
    bl VFipf_memcpy
    stw r23, 0x0(r27)
    li r3, 0x0
L_80074AC4:
    addi r11, r1, 0x40
    bl _restgpr_23
    lwz r0, 0x44(r1)
    mtlr r0
    addi r1, r1, 0x40
    blr
}
#endif
