#include <revolution/vf.h>

extern PF_VOLUME_SET VFipf_vol_set;

// Everything in this file except VFiPFDIR_FinalizeAllDirs is dead-stripped in Wii Sports, so ogws
// has no source for it. The function names below are guesses.

static void VFiPFDIR_GetOpenedDir(struct PF_DIR_ENT* p_ent, struct PF_DIR_ENT** pp_open_ent) {
    s32 i;

    *pp_open_ent = NULL;
    for (i = 0; i < 3; i++) {
        if ((p_ent->p_vol->sdds[i].stat & 1) != 0 && (p_ent->p_vol->sdds[i].stat & 2) != 0 && p_ent->p_vol == p_ent->p_vol->sdds[i].dir_entry.p_vol && p_ent->entry_sector == p_ent->p_vol->sdds[i].dir_entry.entry_sector && p_ent->entry_offset == p_ent->p_vol->sdds[i].dir_entry.entry_offset) {
            *pp_open_ent = &p_ent->p_vol->sdds[i].dir_entry;
        }
    }
}

#ifdef NON_MATCHING
// 99.9%: namelength and prev_entry_offset get r31/r29 swapped.
static s32 VFiPFDIR_DoMakeDir(struct PF_VOLUME* p_vol, struct PF_STR* p_path, u32 unused, struct PF_DIRENT* p_dirent) {
    struct PF_DIR_ENT parent_ent;
    struct PF_DIR_ENT ent;
    s8 tmp_local[512];
    struct PF_ENT_ITER iter;
    struct PF_FFD ffd;
    u8 buf[32];
    struct PF_STR dir_path;
    struct PF_STR filename;
    struct PF_FAT_HINT hint;
    u32 next_chain[2];
    u32 is_found;
    struct PF_CACHE_PAGE* p_page;
    u32 sector;
    u32 success_size;
    u32 pos_idx;
    u16 namelength;
    u8 num_entry_LFNs;
    struct PF_FAT_HINT* p_hint;
    u32 num_sectors;
    u16 prev_entry_offset;
    u32 new_cluster;
    u8 sum;
    u32* position;
    u32 i;
    u32 old_sector;
    s32 err;

    num_entry_LFNs = 0;
    sector = 0;

    err = VFiPFPATH_SplitPath(p_path, &dir_path, &filename);
    if (err) {
        return err;
    }

    err = VFiPFENT_ITER_GetEntryOfPath(&iter, &parent_ent, p_vol, &dir_path, 1);
    if (err) {
        return err;
    }

    if ((parent_ent.attr & 0x10) == 0) {
        return 0x14;
    }

    namelength = VFiPFSTR_StrNumChar(&filename, 1);
    if (namelength > 255) {
        return 1;
    }
    if (namelength + parent_ent.path_len > 0x103) {
        return 2;
    }

    if (VFiPFSTR_GetCodeMode(&filename) == 2) {
        VFiPFPATH_transformFromUnicodeToNormal(tmp_local, (const u16*)VFiPFSTR_GetStrPos(&filename, 1));
    }
    VFiPFSTR_SetLocalStr(&filename, tmp_local);

    if (p_dirent != NULL) {
        p_dirent->reserved[0] = 0;
        p_dirent->reserved[1] = 0;
        p_dirent->dir_start_cluster = parent_ent.start_cluster;
        p_dirent->p_vol = parent_ent.p_vol;
        p_dirent->attr_required = 0x10;
        if (VFiPFSTR_GetCodeMode(&filename) == 1) {
            VFiPFPATH_GetSearchPattern(p_dirent->pattern, NULL, &filename);
            p_dirent->stat |= 1;
        } else {
            VFiPFPATH_GetSearchPattern(p_dirent->pattern, p_dirent->pattern_uni, &filename);
            p_dirent->stat |= 2;
        }
    }

    VFiPFFAT_InitFFD(&ffd, &hint, p_vol, &parent_ent.start_cluster);
    iter.ffd = ffd;
    VFiPFFAT_ResetFFD(&iter.ffd, &parent_ent.start_cluster);
    err = VFiPFENT_ITER_IteratorInitialize(&iter, 0);
    if (err) {
        return err;
    }

    VFipf_memcpy(&ent, &parent_ent, sizeof(struct PF_DIR_ENT));
    err = VFiPFENT_ITER_FindEntry(&iter, &ent, &filename, 0x10, 0, &is_found, 1);
    if (err) {
        return err;
    }
    if (is_found) {
        return 8;
    }

    err = VFiPFPATH_parseShortName(ent.short_name, &filename);
    if (err != 0) {
        if (ent.short_name[0] == 0) {
            return 1;
        }
    }

    if (err != 0) {
        err = VFiPFENT_AdjustSFN(&parent_ent, ent.short_name);
        if (err) {
            return err;
        }
        if (VFiPFSTR_GetCodeMode(&filename) == 1) {
            namelength = VFiPFPATH_transformInUnicode(ent.long_name, (const s8*)VFiPFSTR_GetStrPos(&filename, 1));
        } else {
            VFipf_w_strcpy(ent.long_name, (const u16*)VFiPFSTR_GetStrPos(&filename, 1));
        }
    } else {
        ent.long_name[0] = 0;
    }

    ent.start_cluster = 0;
    ent.file_size = 0;
    ent.p_vol = p_vol;
    ent.small_letter_flag = 0;
    ent.attr = 0x10;

    ent.create_time_ms = VFiPFENT_getcurrentDateTimeForEnt(&ent.create_date, &ent.create_time);
    ent.access_date = ent.create_date;
    ent.modify_time = ent.create_time;
    ent.modify_date = ent.create_date;
    prev_entry_offset = ent.entry_offset;

    if (ent.long_name[0] != 0 && (ent.small_letter_flag & 0x18) == 0) {
        num_entry_LFNs = namelength / 13u + (namelength % 13u != 0);
        err = VFiPFENT_allocateEntryPos(&ent, num_entry_LFNs + 1, &ffd, next_chain, &filename, &pos_idx);
        if (err) {
            return err;
        }

        if ((VFipf_vol_set.setting & 2) == 2) {
            VFiPFPATH_AdjustExtShortName(ent.short_name, pos_idx);
        }
    } else {
        err = VFiPFENT_allocateEntryPos(&ent, 1, &ffd, next_chain, &filename, &pos_idx);
        if (err) {
            return err;
        }
    }

    VFiPFFAT_ResetFFD(&ffd, &ent.start_cluster);
    err = VFiPFFAT_GetSectorSpecified(&ffd, 0, 1, &sector);
    if (err) {
        return err;
    }
    if (sector == 0xFFFFFFFF) {
        return 6;
    }

    p_hint = ffd.p_hint;
    new_cluster = p_hint->cluster;
    if (p_vol->bpb.fat_type == FAT_32 && *iter.ffd.p_start_cluster == p_vol->bpb.root_dir_cluster && prev_entry_offset == 0 && new_cluster == p_vol->bpb.root_dir_cluster) {
        iter.ffd = ffd;
        err = VFiPFENT_ITER_IteratorInitialize(&iter, num_entry_LFNs);
        if (err) {
            return err;
        }
        if (iter.buf[0] == 0) {
            VFiPFFAT_ResetFFD(&ffd, &ent.start_cluster);
            *ffd.p_start_cluster = 0;
            ffd.p_hint->cluster = 0;
            new_cluster = p_vol->bpb.root_dir_cluster + 1;
            err = VFiPFFAT_GetSectorSpecified(&ffd, 0, 1, &sector);
            if (err) {
                return err;
            }
        }
    }

    err = VFiPFCACHE_AllocateDataPage(p_vol, -1, &p_page);
    if (err) {
        return err;
    }

    VFipf_memset(p_page->p_buf, 0, p_vol->bpb.bytes_per_sector);
    for (num_sectors = p_vol->bpb.sectors_per_cluster; num_sectors != 0; num_sectors--) {
        if (num_sectors == 1) {
            VFiPFENT_MakeDotName(p_page->p_buf);
            VFiPFENT_storeEntryNumericFieldsToBuf(p_page->p_buf, &ent);
            VFiPFENT_MakeDotDotName(p_page->p_buf + 0x20);
            if (parent_ent.entry_sector == 0xFFFFFFFF) {
                ent.start_cluster = 0;
            } else {
                ent.start_cluster = parent_ent.start_cluster;
            }
            VFiPFENT_storeEntryNumericFieldsToBuf(p_page->p_buf + 0x20, &ent);
        }
        err = VFiPFSEC_WriteData(p_vol, p_page->p_buf, sector + num_sectors - 1, 0, p_vol->bpb.bytes_per_sector, &success_size, 0);
        if (err) {
            break;
        }
        if (success_size != p_vol->bpb.bytes_per_sector) {
            err = 0x11;
            break;
        }
    }
    VFiPFCACHE_FreeDataPage(p_vol, p_page);
    if (err) {
        return err;
    }

    ent.num_entry_LFNs = num_entry_LFNs;
    old_sector = ent.entry_sector;
    sum = VFiPFENT_CalcCheckSum(&ent);

    if ((ent.small_letter_flag & 0x18) == 0) {
        position = next_chain;
        for (i = num_entry_LFNs; i >= 1; i--) {
            VFiPFENT_storeLFNEntryFieldsToBuf(buf, &ent, i, sum, i == num_entry_LFNs);
            err = VFiPFSEC_WriteData(p_vol, buf, old_sector, ent.entry_offset, 32, &success_size, 0);
            if (err) {
                return err;
            }
            if (success_size != 32) {
                return 17;
            }
            ent.entry_offset += 32;
            if (ent.entry_offset >= p_vol->bpb.bytes_per_sector) {
                ent.entry_offset = 0;
                old_sector = *position;
                position++;
            }
        }
        ent.entry_sector = old_sector;
    }

    ent.start_cluster = new_cluster;
    err = VFiPFENT_updateEntry(&ent, 0);
    if (err) {
        return err;
    }

    if (p_dirent != NULL) {
        p_dirent->index = pos_idx + 1;
        VFipf_strcpy(p_dirent->short_name, ent.short_name);
        if ((p_dirent->stat & 2) == 2) {
            VFiPFPATH_transformInUnicode(p_dirent->short_name_uni, p_dirent->short_name);
        }

        if (ent.long_name[0] != 0) {
            if ((VFipf_vol_set.setting & 2) == 2) {
                VFipf_vol_set.setting = (VFipf_vol_set.setting & ~3) | 1;
                VFiPFPATH_transformFromUnicodeToNormal(p_dirent->long_name, ent.long_name);
                VFipf_vol_set.setting = (VFipf_vol_set.setting & ~3) | 2;
            } else {
                VFiPFPATH_transformFromUnicodeToNormal(p_dirent->long_name, ent.long_name);
            }
            if ((p_dirent->stat & 2) == 2) {
                VFipf_w_strcpy(p_dirent->long_name_uni, ent.long_name);
            }
            p_dirent->check_sum = sum;
            p_dirent->ordinal = 1;
        } else {
            p_dirent->long_name[0] = 0;
            if ((p_dirent->stat & 2) == 2) {
                p_dirent->long_name_uni[0] = 0;
            }
            p_dirent->ordinal = 0;
            p_dirent->check_sum = 0;
        }

        p_dirent->file_size = ent.file_size;
        p_dirent->modify_time = ent.modify_time;
        p_dirent->modify_date = ent.modify_date;
        p_dirent->attr = ent.attr;
        p_dirent->num_entry_LFNs = ent.num_entry_LFNs;
    }

    return 0;
}
#else
extern void _savegpr_23(void);
extern void _restgpr_23(void);

static asm s32 VFiPFDIR_DoMakeDir(struct PF_VOLUME* p_vol, struct PF_STR* p_path, u32 unused, struct PF_DIRENT* p_dirent) {
    nofralloc
    stwu r1, -0x7d0(r1)
    mflr r0
    stw r0, 0x7d4(r1)
    addi r11, r1, 0x7d0
    bl _savegpr_23
    li r30, 0x0
    mr r26, r3
    stw r30, 0x10(r1)
    mr r3, r4
    mr r27, r6
    addi r4, r1, 0x48
    addi r5, r1, 0x38
    bl VFiPFPATH_SplitPath
    cmpwi r3, 0x0
    beq lbl_8005A5DC
    b lbl_8005AE08
lbl_8005A5DC:
    mr r5, r26
    addi r3, r1, 0xb0
    addi r4, r1, 0x560
    addi r6, r1, 0x48
    li r7, 0x1
    bl VFiPFENT_ITER_GetEntryOfPath
    cmpwi r3, 0x0
    beq lbl_8005A600
    b lbl_8005AE08
lbl_8005A600:
    lbz r0, 0x77c(r1)
    rlwinm. r0, r0, 0, 27, 27
    bne lbl_8005A614
    li r3, 0x14
    b lbl_8005AE08
lbl_8005A614:
    addi r3, r1, 0x38
    li r4, 0x1
    bl VFiPFSTR_StrNumChar
    clrlwi r4, r3, 16
    mr r29, r3
    cmplwi r4, 0xff
    ble lbl_8005A638
    li r3, 0x1
    b lbl_8005AE08
lbl_8005A638:
    lwz r0, 0x790(r1)
    add r0, r4, r0
    cmplwi r0, 0x103
    ble lbl_8005A650
    li r3, 0x2
    b lbl_8005AE08
lbl_8005A650:
    addi r3, r1, 0x38
    bl VFiPFSTR_GetCodeMode
    cmplwi r3, 0x2
    bne lbl_8005A678
    addi r3, r1, 0x38
    li r4, 0x1
    bl VFiPFSTR_GetStrPos
    mr r4, r3
    addi r3, r1, 0x120
    bl VFiPFPATH_transformFromUnicodeToNormal
lbl_8005A678:
    addi r3, r1, 0x38
    addi r4, r1, 0x120
    bl VFiPFSTR_SetLocalStr
    cmpwi r27, 0x0
    beq lbl_8005A6FC
    li r4, 0x0
    li r0, 0x10
    stw r4, 0x0(r27)
    addi r3, r1, 0x38
    stw r4, 0x4(r27)
    lwz r4, 0x794(r1)
    stw r4, 0xc(r27)
    lwz r4, 0x78c(r1)
    stw r4, 0x8(r27)
    stb r0, 0x1b(r27)
    bl VFiPFSTR_GetCodeMode
    cmplwi r3, 0x1
    bne lbl_8005A6E0
    addi r3, r27, 0x1c
    addi r5, r1, 0x38
    li r4, 0x0
    bl VFiPFPATH_GetSearchPattern
    lwz r0, 0x14(r27)
    ori r0, r0, 0x1
    stw r0, 0x14(r27)
    b lbl_8005A6FC
lbl_8005A6E0:
    addi r3, r27, 0x1c
    addi r4, r27, 0x444
    addi r5, r1, 0x38
    bl VFiPFPATH_GetSearchPattern
    lwz r0, 0x14(r27)
    ori r0, r0, 0x2
    stw r0, 0x14(r27)
lbl_8005A6FC:
    mr r5, r26
    addi r3, r1, 0x78
    addi r4, r1, 0x28
    addi r6, r1, 0x794
    bl VFiPFFAT_InitFFD
    lwz r31, 0x78(r1)
    addi r3, r1, 0xb8
    lwz r28, 0x7c(r1)
    addi r4, r1, 0x794
    lwz r25, 0x80(r1)
    lwz r24, 0x84(r1)
    lwz r23, 0x88(r1)
    lwz r12, 0x8c(r1)
    lwz r11, 0x90(r1)
    lwz r10, 0x94(r1)
    lwz r9, 0x98(r1)
    lwz r8, 0x9c(r1)
    lwz r7, 0xa0(r1)
    lwz r6, 0xa4(r1)
    lwz r5, 0xa8(r1)
    lwz r0, 0xac(r1)
    stw r31, 0xb8(r1)
    stw r28, 0xbc(r1)
    stw r25, 0xc0(r1)
    stw r24, 0xc4(r1)
    stw r23, 0xc8(r1)
    stw r12, 0xcc(r1)
    stw r11, 0xd0(r1)
    stw r10, 0xd4(r1)
    stw r9, 0xd8(r1)
    stw r8, 0xdc(r1)
    stw r7, 0xe0(r1)
    stw r6, 0xe4(r1)
    stw r5, 0xe8(r1)
    stw r0, 0xec(r1)
    bl VFiPFFAT_ResetFFD
    addi r3, r1, 0xb0
    li r4, 0x0
    bl VFiPFENT_ITER_IteratorInitialize
    cmpwi r3, 0x0
    beq lbl_8005A7A4
    b lbl_8005AE08
lbl_8005A7A4:
    addi r3, r1, 0x320
    addi r4, r1, 0x560
    li r5, 0x240
    bl VFipf_memcpy
    addi r3, r1, 0xb0
    addi r4, r1, 0x320
    addi r5, r1, 0x38
    addi r8, r1, 0x18
    li r6, 0x10
    li r7, 0x0
    li r9, 0x1
    bl VFiPFENT_ITER_FindEntry
    cmpwi r3, 0x0
    beq lbl_8005A7E0
    b lbl_8005AE08
lbl_8005A7E0:
    lwz r0, 0x18(r1)
    cmpwi r0, 0x0
    beq lbl_8005A7F4
    li r3, 0x8
    b lbl_8005AE08
lbl_8005A7F4:
    addi r3, r1, 0x52e
    addi r4, r1, 0x38
    bl VFiPFPATH_parseShortName
    cmpwi r3, 0x0
    beq lbl_8005A81C
    lbz r0, 0x52e(r1)
    extsb. r0, r0
    bne lbl_8005A81C
    li r3, 0x1
    b lbl_8005AE08
lbl_8005A81C:
    cmpwi r3, 0x0
    beq lbl_8005A888
    addi r3, r1, 0x560
    addi r4, r1, 0x52e
    bl VFiPFENT_AdjustSFN
    cmpwi r3, 0x0
    beq lbl_8005A83C
    b lbl_8005AE08
lbl_8005A83C:
    addi r3, r1, 0x38
    bl VFiPFSTR_GetCodeMode
    cmplwi r3, 0x1
    bne lbl_8005A86C
    addi r3, r1, 0x38
    li r4, 0x1
    bl VFiPFSTR_GetStrPos
    mr r4, r3
    addi r3, r1, 0x320
    bl VFiPFPATH_transformInUnicode
    clrlwi r29, r3, 16
    b lbl_8005A890
lbl_8005A86C:
    addi r3, r1, 0x38
    li r4, 0x1
    bl VFiPFSTR_GetStrPos
    mr r4, r3
    addi r3, r1, 0x320
    bl VFipf_w_strcpy
    b lbl_8005A890
lbl_8005A888:
    li r0, 0x0
    sth r0, 0x320(r1)
lbl_8005A890:
    li r5, 0x0
    li r0, 0x10
    stw r5, 0x554(r1)
    addi r3, r1, 0x540
    addi r4, r1, 0x53e
    stw r5, 0x548(r1)
    stw r26, 0x54c(r1)
    stb r5, 0x53b(r1)
    stb r0, 0x53c(r1)
    bl VFiPFENT_getcurrentDateTimeForEnt
    lhz r0, 0x320(r1)
    lhz r5, 0x540(r1)
    lhz r4, 0x53e(r1)
    cmpwi r0, 0x0
    stb r3, 0x53d(r1)
    lhz r31, 0x55c(r1)
    sth r5, 0x542(r1)
    sth r4, 0x544(r1)
    sth r5, 0x546(r1)
    beq lbl_8005A970
    lbz r0, 0x53b(r1)
    rlwinm. r0, r0, 0, 27, 28
    bne lbl_8005A970
    lis r3, 0x4ec5
    clrlwi r4, r29, 16
    subi r0, r3, 0x13b1
    addi r5, r1, 0x78
    mulhwu r0, r0, r4
    addi r3, r1, 0x320
    addi r6, r1, 0x20
    addi r7, r1, 0x38
    addi r8, r1, 0x8
    srwi r9, r0, 2
    mulli r0, r9, 0xd
    subf r4, r0, r4
    neg r0, r4
    or r0, r0, r4
    srwi r0, r0, 31
    add r0, r9, r0
    clrlwi r30, r0, 24
    addi r0, r30, 0x1
    clrlwi r4, r0, 24
    bl VFiPFENT_allocateEntryPos
    cmpwi r3, 0x0
    beq lbl_8005A948
    b lbl_8005AE08
lbl_8005A948:
    lis r3, VFipf_vol_set@ha
    addi r3, r3, VFipf_vol_set@l
    lwz r0, 0x3c(r3)
    rlwinm r0, r0, 0, 30, 30
    cmplwi r0, 0x2
    bne lbl_8005A998
    lwz r4, 0x8(r1)
    addi r3, r1, 0x52e
    bl VFiPFPATH_AdjustExtShortName
    b lbl_8005A998
lbl_8005A970:
    addi r3, r1, 0x320
    addi r5, r1, 0x78
    addi r6, r1, 0x20
    addi r7, r1, 0x38
    addi r8, r1, 0x8
    li r4, 0x1
    bl VFiPFENT_allocateEntryPos
    cmpwi r3, 0x0
    beq lbl_8005A998
    b lbl_8005AE08
lbl_8005A998:
    addi r3, r1, 0x78
    addi r4, r1, 0x554
    bl VFiPFFAT_ResetFFD
    addi r3, r1, 0x78
    addi r6, r1, 0x10
    li r4, 0x0
    li r5, 0x1
    bl VFiPFFAT_GetSectorSpecified
    cmpwi r3, 0x0
    beq lbl_8005A9C4
    b lbl_8005AE08
lbl_8005A9C4:
    lwz r3, 0x10(r1)
    addis r0, r3, 0x1
    cmplwi r0, 0xffff
    bne lbl_8005A9DC
    li r3, 0x6
    b lbl_8005AE08
lbl_8005A9DC:
    lwz r0, 0x1c(r26)
    lwz r23, 0xa8(r1)
    cmpwi r0, 0x2
    lwz r28, 0x4(r23)
    bne lbl_8005AAEC
    lwz r3, 0xc0(r1)
    lwz r4, 0x10(r26)
    lwz r0, 0x0(r3)
    cmplw r0, r4
    bne lbl_8005AAEC
    cmpwi r31, 0x0
    bne lbl_8005AAEC
    cmplw r28, r4
    bne lbl_8005AAEC
    lwz r24, 0x78(r1)
    mr r4, r30
    lwz r31, 0x7c(r1)
    addi r3, r1, 0xb0
    lwz r29, 0x80(r1)
    lwz r25, 0x84(r1)
    lwz r12, 0x88(r1)
    lwz r11, 0x8c(r1)
    lwz r10, 0x90(r1)
    lwz r9, 0x94(r1)
    lwz r8, 0x98(r1)
    lwz r7, 0x9c(r1)
    lwz r6, 0xa0(r1)
    lwz r5, 0xa4(r1)
    lwz r0, 0xac(r1)
    stw r24, 0xb8(r1)
    stw r31, 0xbc(r1)
    stw r29, 0xc0(r1)
    stw r25, 0xc4(r1)
    stw r12, 0xc8(r1)
    stw r11, 0xcc(r1)
    stw r10, 0xd0(r1)
    stw r9, 0xd4(r1)
    stw r8, 0xd8(r1)
    stw r7, 0xdc(r1)
    stw r6, 0xe0(r1)
    stw r5, 0xe4(r1)
    stw r23, 0xe8(r1)
    stw r0, 0xec(r1)
    bl VFiPFENT_ITER_IteratorInitialize
    cmpwi r3, 0x0
    beq lbl_8005AA98
    b lbl_8005AE08
lbl_8005AA98:
    lbz r0, 0xfc(r1)
    cmpwi r0, 0x0
    bne lbl_8005AAEC
    addi r3, r1, 0x78
    addi r4, r1, 0x554
    bl VFiPFFAT_ResetFFD
    lwz r4, 0x80(r1)
    li r0, 0x0
    addi r3, r1, 0x78
    addi r6, r1, 0x10
    stw r0, 0x0(r4)
    li r4, 0x0
    li r5, 0x1
    lwz r7, 0xa8(r1)
    stw r0, 0x4(r7)
    lwz r7, 0x10(r26)
    addi r28, r7, 0x1
    bl VFiPFFAT_GetSectorSpecified
    cmpwi r3, 0x0
    beq lbl_8005AAEC
    b lbl_8005AE08
lbl_8005AAEC:
    mr r3, r26
    addi r5, r1, 0x14
    li r4, -0x1
    bl VFiPFCACHE_AllocateDataPage
    cmpwi r3, 0x0
    mr r23, r3
    beq lbl_8005AB0C
    b lbl_8005AE08
lbl_8005AB0C:
    lwz r3, 0x14(r1)
    li r4, 0x0
    lhz r5, 0x0(r26)
    lwz r3, 0x8(r3)
    bl VFipf_memset
    lbz r31, 0x6(r26)
    li r25, 0x0
    b lbl_8005ABE8
lbl_8005AB2C:
    cmplwi r31, 0x1
    bne lbl_8005AB94
    lwz r3, 0x14(r1)
    lwz r3, 0x8(r3)
    bl VFiPFENT_MakeDotName
    lwz r3, 0x14(r1)
    addi r4, r1, 0x320
    lwz r3, 0x8(r3)
    bl VFiPFENT_storeEntryNumericFieldsToBuf
    lwz r3, 0x14(r1)
    lwz r3, 0x8(r3)
    addi r3, r3, 0x20
    bl VFiPFENT_MakeDotDotName
    lwz r3, 0x798(r1)
    addis r0, r3, 0x1
    cmplwi r0, 0xffff
    bne lbl_8005AB78
    stw r25, 0x554(r1)
    b lbl_8005AB80
lbl_8005AB78:
    lwz r0, 0x794(r1)
    stw r0, 0x554(r1)
lbl_8005AB80:
    lwz r3, 0x14(r1)
    addi r4, r1, 0x320
    lwz r3, 0x8(r3)
    addi r3, r3, 0x20
    bl VFiPFENT_storeEntryNumericFieldsToBuf
lbl_8005AB94:
    lwz r4, 0x14(r1)
    mr r3, r26
    lwz r0, 0x10(r1)
    addi r8, r1, 0xc
    lwz r4, 0x8(r4)
    li r6, 0x0
    add r5, r31, r0
    lhz r7, 0x0(r26)
    subi r5, r5, 0x1
    li r9, 0x0
    bl VFiPFSEC_WriteData
    cmpwi r3, 0x0
    mr r23, r3
    bne lbl_8005ABF0
    lwz r3, 0xc(r1)
    lhz r0, 0x0(r26)
    cmplw r3, r0
    beq lbl_8005ABE4
    li r23, 0x11
    b lbl_8005ABF0
lbl_8005ABE4:
    subi r31, r31, 0x1
lbl_8005ABE8:
    cmpwi r31, 0x0
    bne lbl_8005AB2C
lbl_8005ABF0:
    lwz r4, 0x14(r1)
    mr r3, r26
    bl VFiPFCACHE_FreeDataPage
    cmpwi r23, 0x0
    beq lbl_8005AC0C
    mr r3, r23
    b lbl_8005AE08
lbl_8005AC0C:
    stb r30, 0x52a(r1)
    addi r3, r1, 0x320
    lwz r29, 0x558(r1)
    bl VFiPFENT_CalcCheckSum
    lbz r0, 0x53b(r1)
    mr r31, r3
    rlwinm. r0, r0, 0, 27, 28
    bne lbl_8005ACD4
    mr r24, r30
    addi r23, r1, 0x20
    li r25, 0x0
    b lbl_8005ACC8
lbl_8005AC3C:
    subf r0, r24, r30
    addi r3, r1, 0x58
    cntlzw r0, r0
    addi r4, r1, 0x320
    clrlwi r5, r24, 24
    clrlwi r6, r31, 24
    srwi r7, r0, 5
    bl VFiPFENT_storeLFNEntryFieldsToBuf
    lhz r6, 0x55c(r1)
    mr r3, r26
    mr r5, r29
    addi r4, r1, 0x58
    addi r8, r1, 0xc
    li r7, 0x20
    li r9, 0x0
    bl VFiPFSEC_WriteData
    cmpwi r3, 0x0
    beq lbl_8005AC88
    b lbl_8005AE08
lbl_8005AC88:
    lwz r0, 0xc(r1)
    cmplwi r0, 0x20
    beq lbl_8005AC9C
    li r3, 0x11
    b lbl_8005AE08
lbl_8005AC9C:
    lhz r3, 0x55c(r1)
    addi r0, r3, 0x20
    sth r0, 0x55c(r1)
    clrlwi r3, r0, 16
    lhz r0, 0x0(r26)
    cmplw r3, r0
    blt lbl_8005ACC4
    sth r25, 0x55c(r1)
    lwz r29, 0x0(r23)
    addi r23, r23, 0x4
lbl_8005ACC4:
    subi r24, r24, 0x1
lbl_8005ACC8:
    cmplwi r24, 0x1
    bge lbl_8005AC3C
    stw r29, 0x558(r1)
lbl_8005ACD4:
    stw r28, 0x554(r1)
    addi r3, r1, 0x320
    li r4, 0x0
    bl VFiPFENT_updateEntry
    cmpwi r3, 0x0
    beq lbl_8005ACF0
    b lbl_8005AE08
lbl_8005ACF0:
    cmpwi r27, 0x0
    beq lbl_8005AE04
    lwz r5, 0x8(r1)
    addi r3, r27, 0x22d
    addi r4, r1, 0x52e
    addi r0, r5, 0x1
    stw r0, 0x10(r27)
    bl VFipf_strcpy
    lwz r0, 0x14(r27)
    rlwinm r0, r0, 0, 30, 30
    cmplwi r0, 0x2
    bne lbl_8005AD2C
    addi r3, r27, 0x64c
    addi r4, r27, 0x22d
    bl VFiPFPATH_transformInUnicode
lbl_8005AD2C:
    lhz r0, 0x320(r1)
    cmpwi r0, 0x0
    beq lbl_8005ADB4
    lis r26, VFipf_vol_set@ha
    addi r26, r26, VFipf_vol_set@l
    lwz r3, 0x3c(r26)
    rlwinm r0, r3, 0, 30, 30
    cmplwi r0, 0x2
    bne lbl_8005AD7C
    clrrwi r0, r3, 2
    addi r3, r27, 0x23a
    ori r0, r0, 0x1
    addi r4, r1, 0x320
    stw r0, 0x3c(r26)
    bl VFiPFPATH_transformFromUnicodeToNormal
    lwz r0, 0x3c(r26)
    clrrwi r0, r0, 2
    ori r0, r0, 0x2
    stw r0, 0x3c(r26)
    b lbl_8005AD88
lbl_8005AD7C:
    addi r3, r27, 0x23a
    addi r4, r1, 0x320
    bl VFiPFPATH_transformFromUnicodeToNormal
lbl_8005AD88:
    lwz r0, 0x14(r27)
    rlwinm r0, r0, 0, 30, 30
    cmplwi r0, 0x2
    bne lbl_8005ADA4
    addi r3, r27, 0x666
    addi r4, r1, 0x320
    bl VFipf_w_strcpy
lbl_8005ADA4:
    li r0, 0x1
    stb r31, 0x1a(r27)
    stb r0, 0x19(r27)
    b lbl_8005ADDC
lbl_8005ADB4:
    lwz r0, 0x14(r27)
    li r3, 0x0
    stb r3, 0x23a(r27)
    rlwinm r0, r0, 0, 30, 30
    cmplwi r0, 0x2
    bne lbl_8005ADD0
    sth r3, 0x666(r27)
lbl_8005ADD0:
    li r0, 0x0
    stb r0, 0x19(r27)
    stb r0, 0x1a(r27)
lbl_8005ADDC:
    lwz r0, 0x548(r1)
    stw r0, 0x228(r27)
    lhz r0, 0x544(r1)
    sth r0, 0x224(r27)
    lhz r0, 0x546(r1)
    sth r0, 0x226(r27)
    lbz r0, 0x53c(r1)
    stb r0, 0x22c(r27)
    lbz r0, 0x52a(r1)
    stb r0, 0x18(r27)
lbl_8005AE04:
    li r3, 0x0
lbl_8005AE08:
    addi r11, r1, 0x7d0
    bl _restgpr_23
    lwz r0, 0x7d4(r1)
    mtlr r0
    addi r1, r1, 0x7d0
    blr
}
#endif

static s32 VFiPFDIR_DoFindNext(struct PF_DIRENT* p_dirent) {
    struct PF_DIR_ENT ent;
    struct PF_FFD ffd;
    struct PF_FAT_HINT hint;
    struct PF_STR pattern;
    struct PF_DIR_ENT* p_found;
    u32 lpos;
    u32 ppos;
    s32 err;

    lpos = 0;
    ppos = 0;

    if ((p_dirent->stat & 1) == 1) {
        err = VFiPFSTR_InitStr(&pattern, p_dirent->pattern, 1);
    } else {
        err = VFiPFSTR_InitStr(&pattern, (const s8*)p_dirent->pattern_uni, 2);
    }
    if (err) {
        return err;
    }
    VFiPFSTR_SetLocalStr(&pattern, p_dirent->pattern);

    while (ppos < 999999) {
        if (p_dirent->index >= 999999) {
            return 10;
        }
        ent.start_cluster = p_dirent->dir_start_cluster;
        VFiPFFAT_InitFFD(&ffd, &hint, p_dirent->p_vol, &ent.start_cluster);
        err = VFiPFENT_findEntryPos(&ffd, &ent, p_dirent->index, &pattern, p_dirent->attr_required, 0, &lpos, &ppos);
        if (err) {
            return err;
        }
        p_dirent->index = ppos + 1;
        if (ppos == 999999) {
            return 3;
        }
        break;
    }

    VFipf_strcpy(p_dirent->short_name, ent.short_name);
    if ((p_dirent->stat & 2) == 2) {
        VFiPFPATH_transformInUnicode(p_dirent->short_name_uni, p_dirent->short_name);
    }

    if (ent.long_name[0] != 0) {
        if ((VFipf_vol_set.setting & 2) == 2) {
            VFipf_vol_set.setting = (VFipf_vol_set.setting & ~3) | 1;
            VFiPFPATH_transformFromUnicodeToNormal(p_dirent->long_name, ent.long_name);
            VFipf_vol_set.setting = (VFipf_vol_set.setting & ~3) | 2;
        } else {
            VFiPFPATH_transformFromUnicodeToNormal(p_dirent->long_name, ent.long_name);
        }
        if ((p_dirent->stat & 2) == 2) {
            VFipf_w_strcpy(p_dirent->long_name_uni, ent.long_name);
        }
    } else {
        p_dirent->long_name[0] = 0;
        if ((p_dirent->stat & 2) == 2) {
            p_dirent->long_name_uni[0] = 0;
        }
    }

    p_found = NULL;
    if ((ent.attr & 0x10) != 0 && p_dirent->p_vol->num_opened_directories != 0) {
        VFiPFDIR_GetOpenedDir(&ent, &p_found);
    } else if ((ent.attr & 0x10) == 0 && (ent.attr & 8) == 0 && p_dirent->p_vol->num_opened_files != 0) {
        VFiPFFILE_GetOpenedFile(&ent, &p_found);
    }
    if (p_found == NULL) {
        p_found = &ent;
    }

    p_dirent->file_size = p_found->file_size;
    p_dirent->modify_time = p_found->modify_time;
    p_dirent->modify_date = p_found->modify_date;
    p_dirent->attr = p_found->attr;
    if (p_dirent->attr == 0) {
        p_dirent->attr = 0x40;
    }
    p_dirent->num_entry_LFNs = p_found->num_entry_LFNs;
    p_dirent->ordinal = p_found->ordinal;
    p_dirent->check_sum = p_found->check_sum;
    return 0;
}

static s32 VFiPFDIR_DoFindFirst(struct PF_VOLUME* p_vol, struct PF_STR* p_path, u32 attr, struct PF_DIRENT* p_dirent) {
    struct PF_STR dir_path;
    struct PF_STR filename;
    struct PF_ENT_ITER iter;
    struct PF_DIR_ENT ent;
    s32 err;

    err = VFiPFPATH_SplitPathWildcard(p_path, &dir_path, &filename);
    if (err) {
        return err;
    }

    err = VFiPFENT_ITER_GetEntryOfPattern(&iter, &ent, p_vol, &dir_path);
    if (err) {
        return err;
    }

    if ((ent.attr & 0x10) == 0) {
        return 0x14;
    }

    p_dirent->dir_start_cluster = ent.start_cluster;
    p_dirent->index = 0;
    p_dirent->p_vol = ent.p_vol;
    p_dirent->attr_required = attr;
    p_dirent->stat = 0;

    if (VFiPFSTR_GetCodeMode(&filename) == 1) {
        VFiPFPATH_GetSearchPattern(p_dirent->pattern, NULL, &filename);
        p_dirent->stat |= 1;
    } else {
        VFiPFPATH_GetSearchPattern(p_dirent->pattern, p_dirent->pattern_uni, &filename);
        p_dirent->stat |= 2;
    }

    return VFiPFDIR_DoFindNext(p_dirent);
}

static void VFiPFDIR_FinalizeSDD(struct PF_SDD* p_sdd) {
    p_sdd->stat = 0;
    VFiPFFAT_FinalizeFFD(&p_sdd->ffd);
}

static void VFiPFDIR_FinalizeUDD(struct PF_DIR* p_dir) {
    p_dir->stat &= ~1u;
}

void VFiPFDIR_FinalizeAllDirs(struct PF_VOLUME* p_vol) {
    u16 i;

    for (i = 0; i < 3u; ++i) {
        VFiPFDIR_FinalizeSDD(&p_vol->sdds[i]);
    }
    for (i = 0; i < 3u; ++i) {
        VFiPFDIR_FinalizeUDD(&p_vol->udds[i]);
    }
    p_vol->num_opened_directories = 0;
}

s32 VFiPFDIR_fsfirst(struct PF_STR* p_path, u32 attr, struct PF_DIRENT* p_dirent) {
    struct PF_VOLUME* p_vol;
    s32 err;

    if (p_path == NULL) {
        VFipf_vol_set.last_error = 10;
        return 10;
    }

    p_vol = VFiPFPATH_GetVolumeFromPath(p_path);
    if (p_vol == NULL) {
        VFipf_vol_set.last_error = 9;
        return 9;
    }

    if (p_dirent == NULL) {
        VFipf_vol_set.last_error = 10;
        p_vol->last_error = 10;
        return 10;
    }

    if (attr != 0x7F && ((attr & ~0xFF) != 0 || ((u8)attr & 0x18) == 0x18)) {
        VFipf_vol_set.last_error = 10;
        p_vol->last_error = 10;
        return 10;
    }

    err = VFiPFVOL_CheckForRead(p_vol);
    if (err) {
        VFipf_vol_set.last_error = err;
        p_vol->last_error = err;
        return err;
    }

    err = VFiPFDIR_DoFindFirst(p_vol, p_path, attr, p_dirent);
    if (err) {
        p_dirent->p_vol = NULL;
        VFipf_vol_set.last_error = err;
        p_vol->last_error = err;
    }
    return err;
}

s32 VFiPFDIR_fsnext(struct PF_DIRENT* p_dirent) {
    struct PF_VOLUME* p_vol;
    s32 err;

    if (p_dirent == NULL) {
        VFipf_vol_set.last_error = 10;
        return 10;
    }

    p_vol = p_dirent->p_vol;
    if (p_vol == NULL) {
        VFipf_vol_set.last_error = 10;
        return 10;
    }

    err = VFiPFVOL_CheckForRead(p_vol);
    if (err) {
        VFipf_vol_set.last_error = err;
        p_vol->last_error = err;
        return err;
    }

    err = VFiPFDIR_DoFindNext(p_dirent);
    if (err) {
        VFipf_vol_set.last_error = err;
        p_vol->last_error = err;
    }
    return err;
}

s32 VFiPFDIR_mkdir(struct PF_STR* p_path) {
    struct PF_VOLUME* p_vol;
    s32 err;

    if (p_path == NULL) {
        VFipf_vol_set.last_error = 10;
        return 10;
    }

    if (VFiPFSTR_StrNCmp(p_path, (const s8*)"", 1, 0, 1) == 0) {
        VFipf_vol_set.last_error = 10;
        return 10;
    }

    p_vol = VFiPFPATH_GetVolumeFromPath(p_path);
    if (p_vol == NULL) {
        VFipf_vol_set.last_error = 9;
        return 9;
    }

    err = VFiPFVOL_CheckForWrite(p_vol);
    if (err) {
        VFipf_vol_set.last_error = err;
        p_vol->last_error = err;
        return err;
    }

    p_vol->cache.signature = NULL;
    err = VFiPFDIR_DoMakeDir(p_vol, p_path, 0, NULL);
    if (err) {
        VFipf_vol_set.last_error = err;
        p_vol->last_error = err;
    } else {
        err = VFiPFCACHE_FlushFATCache(p_vol);
        if (err) {
            VFipf_vol_set.last_error = err;
            p_vol->last_error = err;
        } else {
            err = VFiPFCACHE_FlushDataCacheSpecific(p_vol, NULL);
            if (err) {
                VFipf_vol_set.last_error = err;
                p_vol->last_error = err;
            }
        }
    }
    p_vol->cache.signature = NULL;
    return err;
}
