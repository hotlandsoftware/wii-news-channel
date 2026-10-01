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
