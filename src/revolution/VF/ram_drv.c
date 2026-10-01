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
