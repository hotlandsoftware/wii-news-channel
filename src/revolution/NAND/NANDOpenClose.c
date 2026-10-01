#include <revolution/nand.h>
#include <revolution/fs.h>
#include <cstdio>

static void nandOpenCallback(ISFSError, void *);
static void nandCloseCallback(ISFSError, void *);
static ISFSError nandCopy(s32 fd, s32 origFd, void* buf, const u32 length);
s32 nandSafeOpen(const char* path, NANDFileInfo* info, const u8 accType, void* buf, const u32 length, BOOL privilege_flag, BOOL simple_flag);
s32 nandSafeClose(NANDFileInfo* info, BOOL simple_flag);

static u32 nandGetUniqueNumber(void) {
    static u32 s_counter = 0;
    u32 ret;
    BOOL enabled = OSDisableInterrupts();
    ret = s_counter++;
    OSRestoreInterrupts(enabled);
    return ret;
}

IOSFd nandOpen(const char* path, const u8 accType, NANDCommandBlock* block, const BOOL async_flag, const BOOL privilege_flag) {
    IOSFd fd = -117;
    char absPath[64] = "";
    u32 access = 0;

    nandGenerateAbsPath(absPath, path);

    if (!privilege_flag && nandIsPrivatePath(absPath)) {
        return ISFS_ERROR_ACCESS;
    }
    else {
        switch (accType) {
            case 3:
                access = 3;
                break;
            case 1:
                access = 1;
                break;
            case 2:
                access = 2;
                break;
            default:
                break;
        }

        if (async_flag) {
            fd = ISFS_OpenAsync((const u8*)absPath, access, nandOpenCallback, block);
        }
        else {
            fd = ISFS_Open((const u8*)absPath, access);
        }

        return fd;
    }
}

s32 NANDOpen(const char* path, NANDFileInfo* info, const u8 accType) {
    IOSFd fd = ISFS_ERROR_UNKNOWN;

    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    fd = nandOpen(path, accType, NULL, FALSE, FALSE);

    if (fd >= 0) {
        info->fileDescriptor = fd;
        info->mark = 1;
        return NAND_RESULT_OK;
    }
    else {
        return nandConvertErrorCode(fd);
    }
}

s32 NANDPrivateOpen(const char* path, NANDFileInfo* info, const u8 accType) {
    IOSFd fd = ISFS_ERROR_UNKNOWN;

    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    fd = nandOpen(path, accType, NULL, FALSE, TRUE);

    if (fd >= 0) {
        info->fileDescriptor = fd;
        info->mark = 1;
        return NAND_RESULT_OK;
    }
    else {
        return nandConvertErrorCode(fd);
    }
}

s32 NANDOpenAsync(const char* path, NANDFileInfo* info, const u8 accType, NANDCallback cb, NANDCommandBlock* block) {
    IOSFd fd = ISFS_ERROR_UNKNOWN;

    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    block->callback = cb;
    block->fileInfo = info;
    fd = nandOpen(path, accType, block, TRUE, FALSE);
    return nandConvertErrorCode(fd);
}

s32 NANDPrivateOpenAsync(const char* path, NANDFileInfo* info, const u8 accType, NANDCallback cb, NANDCommandBlock* block) {
    IOSFd fd = ISFS_ERROR_UNKNOWN;

    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    block->callback = cb;
    block->fileInfo = info;
    fd = nandOpen(path, accType, block, TRUE, TRUE);
    return nandConvertErrorCode(fd);
}

void nandOpenCallback(ISFSError result, void* ctxt) {
    NANDCommandBlock* b = (NANDCommandBlock*)ctxt;

    if (result >= 0) {
        ((NANDFileInfo*)(b->fileInfo))->fileDescriptor = result;
        ((NANDFileInfo*)(b->fileInfo))->stage = 2;
        ((NANDFileInfo*)(b->fileInfo))->mark = 1;
        ((NANDCallback)(b->callback))(NAND_RESULT_OK, b);
    }
    else {
        ((NANDCallback)(b->callback))(nandConvertErrorCode(result), b);
    }
}

s32 NANDClose(NANDFileInfo* info) {
    ISFSError err = ISFS_ERROR_UNKNOWN;

    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    if (info->mark != 1) {
        return NAND_RESULT_INVALID;
    }

    err = ISFS_Close(info->fileDescriptor);

    if (err == ISFS_ERROR_OK) {
        info->mark = 2;
    }

    return nandConvertErrorCode(err);
}

s32 NANDCloseAsync(NANDFileInfo* info, NANDCallback cb, NANDCommandBlock* block) {
    ISFSError err = ISFS_ERROR_UNKNOWN;

    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    if (info->mark != 1) {
        return NAND_RESULT_INVALID;
    }

    block->callback = cb;
    block->fileInfo = info;
    err = ISFS_CloseAsync(info->fileDescriptor, nandCloseCallback, block);
    return nandConvertErrorCode(err);
}

s32 NANDSafeOpen(const char* path, NANDFileInfo* info, const u8 accType, void* buf, const u32 length) {
    return nandSafeOpen(path, info, accType, buf, length, FALSE, FALSE);
}

s32 nandSafeOpen(const char* path, NANDFileInfo* info, const u8 accType, void* buf, const u32 length, BOOL privilege_flag, BOOL simple_flag) {
    
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    if (simple_flag && ((length & 0x3FFF) != 0)) {
        return NAND_RESULT_INVALID;
    }
    
    info->accType = accType;
    info->stage = 0;
    nandGenerateAbsPath(info->origPath, path);

    if (!privilege_flag && nandIsPrivatePath(info->origPath)) {
        return NAND_RESULT_ACCESS;
    }

    if (accType == 1) {
        IOSFd fd = ISFS_Open((u8*)(info->origPath), 1);
        if (fd >= ISFS_ERROR_OK) {
            info->fileDescriptor = fd;
            info->stage = 2;

            if (!simple_flag) {
                info->mark = 3;
            } else {
                info->mark = 5;
            }

            return NAND_RESULT_OK;
        } else {
            return nandConvertErrorCode(fd);
        }
    } else if (accType == 2 || accType == 3) {
        char dirName[64];
        char relName[13] = {0};
        ISFSError ret = ISFS_ERROR_UNKNOWN;
        IOSUid ownerId;
        IOSGid groupId;
        u32 attr;
        u32 ownerAcc;
        u32 groupAcc;
        u32 othersAcc;
        u32 sp48 = -1;

        ret = ISFS_CreateDir((u8*)"/tmp/sys", 0, 3, 3, 3);
        if (ret != ISFS_ERROR_OK && ret != ISFS_ERROR_EXISTS) {
            return nandConvertErrorCode(ret);
        } else {
            info->stage = 1;
            ret = ISFS_GetAttr((const u8*)info->origPath, &ownerId, &groupId, &attr, &ownerAcc, &groupAcc, &othersAcc);
            if (ret != ISFS_ERROR_OK) {
                return nandConvertErrorCode(ret);
            }

            info->origFd = ISFS_Open((u8*)info->origPath, 1);
            if (info->origFd < 0) {
                return nandConvertErrorCode(info->origFd);
            }

            info->stage = 2;

            if (!simple_flag) {
                sp48 = nandGetUniqueNumber();
                sprintf(dirName, "%s/%08x", "/tmp/sys", sp48);
                ret = ISFS_CreateDir((const u8*)dirName, 0, 3, 0, 0);
                if (ret != 0) {
                    return nandConvertErrorCode(ret);
                }

                info->stage = 3;
            }

            nandGetRelativeName(relName, info->origPath);

            if (!simple_flag) {
                sprintf(info->tmpPath, "%s/%08x/%s", "/tmp/sys", sp48, relName);
            } else {
                sprintf(info->tmpPath, "%s/%s", "/tmp/sys", relName);
            }

            ret = ISFS_CreateFile((const u8*)info->tmpPath, attr, ownerAcc, groupAcc, othersAcc);
            if (ret != 0) {
                return nandConvertErrorCode(ret);
            }

            info->stage = 4;

            if (accType == 2) {
                info->fileDescriptor = ISFS_Open((const u8*)info->tmpPath, 2);
            } else if (accType == 3) {
                info->fileDescriptor = ISFS_Open((const u8*)info->tmpPath, 3);
            }

            if (info->fileDescriptor < 0) {
                return nandConvertErrorCode(info->fileDescriptor);
            }

            info->stage = 5;

            ret = nandCopy(info->fileDescriptor, info->origFd, buf, length);
            if (ret != 0) {
                return nandConvertErrorCode(ret);
            }

            ret = ISFS_Seek(info->fileDescriptor, 0, 0);
            if (ret == 0) {
                ret = 0;
            } else {
                return nandConvertErrorCode(ret);
            }

            if (ret == 0) {
                if (simple_flag) {
                    info->mark = 5;
                } else {
                    info->mark = 3;
                }
            }

            return nandConvertErrorCode(ret);
        }
    } else {
        return NAND_RESULT_INVALID;
    }
}

s32 NANDSafeClose(NANDFileInfo* info) {
    return nandSafeClose(info, FALSE);
}

s32 nandSafeClose(NANDFileInfo* info, BOOL simple_flag) {
    ISFSError err = ISFS_ERROR_UNKNOWN;
    char tmpdir[64] = "";
    
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }
    
    if (!((info->mark == 3 && !simple_flag) || (info->mark == 5 && simple_flag))) {
        return NAND_RESULT_INVALID;
    }

    if (info->accType == 1) {
        err = ISFS_Close(info->fileDescriptor);
        if (err == 0) {
            info->stage = 7;

            if (!simple_flag) {
                info->mark = 4;
            } else {
                info->mark = 6;
            }
        }

        return nandConvertErrorCode(err);
    } else if (info->accType == 2 || info->accType == 3) {
        err = ISFS_Close(info->fileDescriptor);
        if (err != 0) {
            return nandConvertErrorCode(err);
        }

        info->stage = 6;

        err = ISFS_Close(info->origFd);
        if (err != 0) {
            return nandConvertErrorCode(err);
        }

        info->stage = 7;

        err = ISFS_Rename((const u8*)info->tmpPath, (const u8*)info->origPath);
        if (err != 0) {
            return nandConvertErrorCode(err);
        }

        info->stage = 8;

        if (!simple_flag) {
            nandGetParentDirectory(tmpdir, info->tmpPath);
            err = ISFS_Delete((const u8*)tmpdir);
            if (err == 0) {
                info->stage = 9;
                info->mark = 4;
            }
        } else {
            info->mark = 6;
        }

        return nandConvertErrorCode(err);
    } else {
        OSReport("Illegal NANDFileInfo.\n");
    }

    return NAND_RESULT_FATAL_ERROR;
}

static void nandCloseCallback(ISFSError result, void* ctxt) {
    NANDCommandBlock* b = (NANDCommandBlock*)ctxt;

    if (result == 0) {
        ((NANDFileInfo*)(b->fileInfo))->stage = 7;
        ((NANDFileInfo*)(b->fileInfo))->mark = 2;
    }
    
    ((NANDCallback)(b->callback))(nandConvertErrorCode(result), b);
}

static ISFSError nandCopy(s32 fd, s32 origFd, void* buf, const u32 length) {
    while (1) {
        ISFSError ret;
        ISFSError ret2;

        ret = ISFS_Read(origFd, buf, length);
        if (ret == 0) {
            return 0;
        }
        
        if (ret < 0) {
            return ret;
        }

        ret2 = ISFS_Write(fd, buf, ret);
        if (ret2 < 0) {
            return ret2;
        }

        if (ret2 != ret) {}
    }
}

