#include <revolution/nand.h>
#include <revolution/nwc24.h>
#include <revolution/nwc24/NWC24Internal.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Download task list (WiiConnect24 "nwc24dl.bin").
 *
 * The Jun 2007 SDK links far more of this file than Wii Sports or SMG do.
 * Petari only has the list loading/consistency code (NWC24iOpenDlTaskList,
 * NWC24iLoadDlHeader, DeleteDlTask ...). The task setters/getters, the
 * add/update/delete API and the sorted iterator were written from the DOL;
 * their names are guesses (no symbols or strings name them).
 */

#define DL_HTTP "http://"
#define DL_HTTPS "https://"
#define DL_VF_FILE "wc24dl.vff"

#define DL_FLAG_HTTPS_ONLY (1 << 2)
#define DL_FLAG_GROUP_WRITABLE (1 << 6)
#define DL_FLAG_NO_LIMIT (1 << 30)

#define DL_INTERVAL_MIN 180
#define DL_INTERVAL_MAX 10080
#define DL_MARGIN_MAX 20160
#define DL_COUNT_MAX 100

typedef struct NWC24iDlSortIter {
    u32 mode;    // at 0x0 (low 16 bits: key, bit 31: descending)
    s32 cur;     // at 0x4
    s32 prev;    // at 0x8
    s32 lastId;  // at 0xC
    BOOL first;  // at 0x10
    BOOL valid;  // at 0x14
} NWC24iDlSortIter;

typedef s32 (*NWC24iDlKeyFunc)(u16 id);

static const char* DLFilePath = "/shared2/wc24/nwc24dl.bin";

static BOOL DlNoRestriction;

static BOOL IsPrivateId(u16 id);
static BOOL IsMyApp(u32 appId);
static BOOL IsGroupWritable(u16 groupId, u32 flags);
static NWC24iDlEntry* GetDlTaskEntryHeader(u16 id);
static NWC24Err WriteDlHeader(NWC24File* pFile);
static NWC24Err SeekDlTaskEntry(u16 id, NWC24File* pFile);
static NWC24Err CheckDlEntryAvailable(u16 id);
static NWC24Err WriteDlTaskEntry(NWC24iDlTask* pTask, NWC24File* pFile);
static NWC24Err ReadDlTaskEntry(NWC24iDlTask* pTask, u16 id, NWC24File* pFile);
static NWC24Err ClearDlTaskEntry(u16 id, NWC24File* pFile);
static NWC24Err ReadDlHeader(NWC24File* pFile);
static void InitTaskEntryHeader(u16 id);
static NWC24Err LoadDlTask(NWC24iDlTask* pTask, u16 id);
static NWC24Err GetDlUrlEx(const NWC24iDlTask* pTask, char* pBuf, u32 size, BOOL trailing, u8 index) NO_INLINE;
static NWC24Err WriteDlTask(NWC24iDlTask* pTask) NO_INLINE;
static NWC24Err AddDlTask(NWC24iDlTask* pTask, u16 minId, u16 maxId) NO_INLINE;
static NWC24Err DeleteDlTask(NWC24iDlTask* pTask) NO_INLINE;
static NWC24Err CheckHeader(NWC24iDlHeader* pHeader);
static s32 GetEntryLastAccess(u16 id);
static s32 GetEntryNextTime(u16 id);
static s32 GetEntryPriority(u16 id);

NWC24Err NWC24iIterateDlTaskSorted(NWC24iDlSortIter* pIter, u16* pId) NO_INLINE;
NWC24Err NWC24iDeleteOldestDlTask(void) NO_INLINE;

static NWC24Err CheckDlTask(const NWC24iDlTask* pTask, BOOL wantWrite) {
    NWC24iDlHeader* pHeader = NWC24iGetCachedDlHeader();

    if (pTask == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }

    if (pHeader == NULL) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }

    if (wantWrite && !NWC24IsMsgLibOpenedByTool() && !IsMyApp(pTask->appId)) {
        if (!IsGroupWritable(pTask->groupId, pTask->flags)) {
            return NWC24_ERR_PROTECTED;
        }
    }

    if (pTask->id != 0xFFFF && pTask->id >= pHeader->maxTasks) {
        return NWC24_ERR_INVALID_VALUE;
    }

    return NWC24_OK;
}

static u16 GetMaxTasks(void) {
    return NWC24iGetCachedDlHeader()->maxTasks;
}

static BOOL IsGroupWritable(u16 groupId, u32 flags) {
    return (flags & DL_FLAG_GROUP_WRITABLE) && groupId == NWC24GetGroupId();
}

static BOOL IsMyApp(u32 appId) {
    return (appId & 0xFFFFFF00) == (NWC24GetAppId() & 0xFFFFFF00);
}

static BOOL IsPrivateId(u16 id) {
    return id < NWC24iGetCachedDlHeader()->privateTasks;
}

static BOOL IsOctetStream(NWC24DlType type) {
    if (type == NWC24_DLTYPE_OCTETSTREAM_V1 || type == NWC24_DLTYPE_OCTETSTREAM_V2) {
        return TRUE;
    }
    return FALSE;
}

static NWC24Err CheckDlUrl(const char* pUrl) {
    NWC24Err result;

    result = NWC24iCheckStrLength(pUrl, 7, 256);
    if (result < 0) {
        return result;
    }

    if (strncmp(pUrl, DL_HTTP, 7) != 0 && strncmp(pUrl, DL_HTTPS, 8) != 0) {
        return NWC24_ERR_FORMAT;
    }

    return NWC24_OK;
}

static NWC24Err SetDlNextTime(const NWC24iDlTask* pTask, s64 time) {
    NWC24Err result;
    u16 id;

    result = CheckDlTask(pTask, FALSE);
    if (result != NWC24_OK) {
        return result;
    }

    id = pTask->id;
    if (id == 0xFFFF) {
        return NWC24_ERR_FAILED;
    }

    GetDlTaskEntryHeader(id)->nextTime = time / 60;
    return NWC24_OK;
}

static NWC24Err SetDlLastAccess(const NWC24iDlTask* pTask, s64 time) {
    NWC24Err result;
    u16 id;

    result = CheckDlTask(pTask, FALSE);
    if (result != NWC24_OK) {
        return result;
    }

    id = pTask->id;
    if (id == 0xFFFF) {
        return NWC24_ERR_FAILED;
    }

    GetDlTaskEntryHeader(id)->lastAccess = time / 60;
    return NWC24_OK;
}

static NWC24Err UpdateDlLastAccess(const NWC24iDlTask* pTask) {
    s64 now;
    NWC24Err result;

    result = NWC24iGetUniversalTime(&now);
    if (result >= 0) {
        result = SetDlLastAccess(pTask, now);
        if (result < 0) {
            return result;
        }
    }

    return result;
}

static NWC24Err ClearDlTaskError(NWC24iDlTask* pTask) {
    NWC24Err result;

    result = CheckDlTask(pTask, TRUE);
    if (result != NWC24_OK) {
        return result;
    }

    pTask->lastError = 0;
    pTask->errorCount = 0;
    return NWC24_OK;
}

static NWC24Err CheckSubTaskEnabled(const NWC24iDlTask* pTask, u8 index) {
    NWC24Err result;

    result = CheckDlTask(pTask, FALSE);
    if (result != NWC24_OK) {
        return result;
    }

    if (pTask->subTaskType == NWC24_DL_STTYPE_NONE) {
        return NWC24_ERR_INVALID_OPERATION;
    }

    if (pTask->subTaskMask == 0) {
        return NWC24_ERR_FATAL;
    }

    if (index > 31) {
        return NWC24_ERR_INVALID_VALUE;
    }

    if (!((1 << index) & pTask->subTaskMask)) {
        return NWC24_ERR_DISABLED;
    }

    return NWC24_OK;
}

NWC24Err NWC24InitDlTask(NWC24DlTask* pTask, NWC24DlType type) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    char homeDir[NAND_MAX_PATH] = {0};
    u32 titleIdHi;
    u32 titleIdLo;

    NANDGetHomeDir(homeDir);
    homeDir[15] = '\0';
    titleIdHi = strtoul(homeDir + 7, NULL, 16);
    homeDir[24] = '\0';
    titleIdLo = strtoul(homeDir + 16, NULL, 16);

    if (NWC24iGetCachedDlHeader() == NULL) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }

    if (pTaskImpl == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }

    if (type >= 4) {
        return NWC24_ERR_INVALID_VALUE;
    }

    memset(pTaskImpl, 0, sizeof(NWC24iDlTask));
    pTaskImpl->type = type;
    pTaskImpl->priority = 0x7F;
    pTaskImpl->appId = NWC24GetAppId();
    pTaskImpl->groupId = NWC24GetGroupId();
    pTaskImpl->titleIdHi = titleIdHi;
    pTaskImpl->titleIdLo = titleIdLo;
    pTaskImpl->id = 0xFFFF;
    pTaskImpl->count = 1;
    pTaskImpl->interval = 2880;
    pTaskImpl->margin = 1440;

    if (IsOctetStream(type)) {
        strcpy(pTaskImpl->fileName, "content.bin");
    }

    return CheckDlTask(pTaskImpl, TRUE) < 0 ? NWC24_ERR_FATAL : NWC24_OK;
}

NWC24Err NWC24GetDlTaskId(const NWC24DlTask* pTask, u16* pId) {
    const NWC24iDlTask* pTaskImpl = (const NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTask(pTaskImpl, FALSE);
    if (result != NWC24_OK) {
        return result;
    }

    if (pId == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }

    if (pTaskImpl->id == 0xFFFF) {
        return NWC24_ERR_INVALID_OPERATION;
    }

    *pId = pTaskImpl->id;
    return NWC24_OK;
}

NWC24Err NWC24SetDlPriority(NWC24DlTask* pTask, u8 priority) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTask(pTaskImpl, TRUE);
    if (result != NWC24_OK) {
        return result;
    }

    pTaskImpl->priority = priority;
    return NWC24_OK;
}

NWC24Err NWC24SetDlInterval(NWC24DlTask* pTask, u16 interval) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTask(pTaskImpl, TRUE);
    if (result != NWC24_OK) {
        return result;
    }

    if (!DlNoRestriction && !IsPrivateId(pTaskImpl->id) && !(pTaskImpl->flags & DL_FLAG_NO_LIMIT) &&
        (interval < DL_INTERVAL_MIN || interval > DL_INTERVAL_MAX)) {
        return NWC24_ERR_INVALID_VALUE;
    }

    if (pTaskImpl->interval != interval) {
        pTaskImpl->interval = interval;

        if (pTaskImpl->id != 0xFFFF) {
            s64 now = 0;

            if (NWC24iGetUniversalTime(&now) >= 0) {
                SetDlNextTime(pTaskImpl, now + pTaskImpl->interval * 60);
            }
        }
    }

    return NWC24_OK;
}

NWC24Err NWC24GetDlInterval(const NWC24DlTask* pTask, u16* pInterval) {
    const NWC24iDlTask* pTaskImpl = (const NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTask(pTaskImpl, FALSE);
    if (result != NWC24_OK) {
        return result;
    }

    if (pInterval == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }

    *pInterval = pTaskImpl->interval;
    return NWC24_OK;
}

NWC24Err NWC24SetDlServerInterval(NWC24DlTask* pTask, u32 interval) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTask(pTaskImpl, TRUE);
    if (result != NWC24_OK) {
        return result;
    }

    pTaskImpl->serverInterval = interval;
    return NWC24_OK;
}

NWC24Err NWC24SetDlMargin(NWC24DlTask* pTask, u16 margin) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTask(pTaskImpl, TRUE);
    if (result != NWC24_OK) {
        return result;
    }

    if (!DlNoRestriction && !IsPrivateId(pTaskImpl->id) && (margin < 1 || margin > DL_MARGIN_MAX)) {
        return NWC24_ERR_INVALID_VALUE;
    }

    pTaskImpl->margin = margin;
    return NWC24_OK;
}

NWC24Err NWC24SetDlUrl(NWC24DlTask* pTask, const char* pUrl) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTask(pTaskImpl, TRUE);
    if (result != NWC24_OK) {
        return result;
    }

    result = CheckDlUrl(pUrl);
    if (result < 0) {
        return result;
    }

    if (!DlNoRestriction && (pTaskImpl->flags & DL_FLAG_HTTPS_ONLY) && strncmp(pUrl, DL_HTTP, 7) == 0) {
        return NWC24_ERR_INVALID_VALUE;
    }

    NWC24iStrLCpy(pTaskImpl->url, pUrl, sizeof(pTaskImpl->url));
    return NWC24_OK;
}

NWC24Err NWC24GetDlUrl(const NWC24DlTask* pTask, char* pBuf, u32 size) {
    return GetDlUrlEx((const NWC24iDlTask*)pTask, pBuf, size, FALSE, 0xFF);
}

NWC24Err NWC24SetDlOption(NWC24DlTask* pTask, u32 flags) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTask(pTaskImpl, TRUE);
    if (result != NWC24_OK) {
        return result;
    }

    if (!DlNoRestriction && (flags & DL_FLAG_HTTPS_ONLY) && strncmp(pTaskImpl->url, DL_HTTP, 7) == 0) {
        return NWC24_ERR_INVALID_VALUE;
    }

    switch (pTaskImpl->type) {
    case NWC24_DLTYPE_MULTIPART_V1:
    case NWC24_DLTYPE_OCTETSTREAM_V1: {
        if (flags & 0x80000038) {
            return NWC24_ERR_INVALID_OPERATION;
        }
        break;
    }
    case NWC24_DLTYPE_MULTIPART_V2:
    case NWC24_DLTYPE_OCTETSTREAM_V2:
    default: {
        break;
    }
    }

    pTaskImpl->flags = flags;
    return NWC24_OK;
}

NWC24Err NWC24SetDlFilename(NWC24DlTask* pTask, const char* pFileName) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTask(pTaskImpl, TRUE);
    if (result != NWC24_OK) {
        return result;
    }

    result = NWC24iCheckStrLength(pFileName, 1, NAND_MAX_PATH);
    if (result < 0) {
        return result;
    }

    if (!IsOctetStream(pTaskImpl->type)) {
        return NWC24_ERR_INVALID_OPERATION;
    }

    NWC24iStrLCpy(pTaskImpl->fileName, pFileName, NAND_MAX_PATH);
    return NWC24_OK;
}

static NWC24Err GetDlFilenameEx(const NWC24iDlTask* pTask, char* pBuf, u32 size, BOOL trailing, u8 index) {
    NWC24Err result;
    u32 len;

    result = CheckDlTask(pTask, FALSE);
    if (result != NWC24_OK) {
        return result;
    }

    len = strlen(pTask->fileName);
    if (len == 0) {
        return NWC24_ERR_NOT_FOUND;
    }

    if (len + (trailing ? 3 : 0) + 1 > size) {
        return NWC24_ERR_NOMEM;
    }

    strncpy(pBuf, pTask->fileName, len);
    pBuf[len] = '\0';

    if (trailing) {
        snprintf(pBuf + len, size - len, ".%02d", index);
    }

    return NWC24_OK;
}

NWC24Err NWC24GetDlFilename(const NWC24DlTask* pTask, char* pBuf, u32 size, u8 index) {
    const NWC24iDlTask* pTaskImpl = (const NWC24iDlTask*)pTask;
    BOOL trailing = FALSE;
    NWC24Err result;

    result = CheckDlTask(pTaskImpl, FALSE);
    if (result != NWC24_OK) {
        return result;
    }

    if (pTaskImpl->subTaskFlags & NWC24_DL_STFLAG_TRAILING_FILENAME) {
        trailing = TRUE;
    }

    return GetDlFilenameEx(pTaskImpl, pBuf, size, trailing, index);
}

NWC24Err NWC24SetDlCount(NWC24DlTask* pTask, s16 count) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTask(pTaskImpl, TRUE);
    if (result != NWC24_OK) {
        return result;
    }

    if (count < 1 || (!IsPrivateId(pTaskImpl->id) && !(pTaskImpl->flags & DL_FLAG_NO_LIMIT) && count > DL_COUNT_MAX)) {
        return NWC24_ERR_INVALID_VALUE;
    }

    pTaskImpl->count = count;
    return NWC24_OK;
}

NWC24Err NWC24GetDlSubTaskLastUpdate(const NWC24DlTask* pTask, u8 index, s64* pTime) {
    const NWC24iDlTask* pTaskImpl = (const NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTask(pTaskImpl, FALSE);
    if (result != NWC24_OK) {
        return result;
    }

    if (pTime == NULL || index > 31) {
        return NWC24_ERR_INVALID_VALUE;
    }

    *pTime = pTaskImpl->lastUpdateSubTask[index] * 60;
    return NWC24_OK;
}

NWC24Err NWC24SetDlSubTask(NWC24DlTask* pTask, NWC24DlSubTaskType type, u32 mask, u16 flags) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTask(pTaskImpl, TRUE);
    if (result != NWC24_OK) {
        return result;
    }

    if (type >= 5 || (type != NWC24_DL_STTYPE_NONE && mask == 0)) {
        return NWC24_ERR_INVALID_VALUE;
    }

    if (pTaskImpl->subTaskType != (u8)type) {
        memset(pTaskImpl->lastUpdateSubTask, 0, sizeof(pTaskImpl->lastUpdateSubTask));
        pTaskImpl->lastUpdate = 0;
    }

    pTaskImpl->subTaskType = type;
    pTaskImpl->subTaskMask = mask;
    pTaskImpl->subTaskFlags = flags;
    pTaskImpl->subTaskCounter = 0;
    return NWC24_OK;
}

NWC24Err NWC24CheckDlTask(const NWC24DlTask* pTask) {
    NWC24Err result;

    result = CheckDlTask((const NWC24iDlTask*)pTask, FALSE);
    if (result != NWC24_OK) {
        return result;
    }

    return NWC24_OK;
}

static NWC24Err FindDlTaskByAppId(u16* pId, u32 appId) {
    NWC24Err result;

    if (NWC24iGetCachedDlHeader() == NULL) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }

    if (appId == 0) {
        return NWC24_ERR_INVALID_VALUE;
    }

    for (result = NWC24IterateDlTask(pId, TRUE); result >= 0; result = NWC24IterateDlTask(pId, FALSE)) {
        if (GetDlTaskEntryHeader(*pId)->app == appId) {
            return NWC24_OK;
        }
    }

    return NWC24_ERR_NOT_FOUND;
}

NWC24Err NWC24GetMyDlTask(NWC24DlTask* pTask) {
    u32 appId = NWC24GetAppId();
    u16 id;
    NWC24Err result;

    result = FindDlTaskByAppId(&id, appId);
    if (result < 0) {
        return result;
    }

    return NWC24GetDlTask(pTask, id);
}

NWC24Err NWC24IterateDlTask(u16* pId, BOOL first) {
    u16 id;

    if (NWC24iGetCachedDlHeader() == NULL) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }

    if (pId == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }

    if (first) {
        *pId = 0;
    } else {
        (*pId)++;
    }

    if (*pId >= NWC24iGetCachedDlHeader()->maxTasks) {
        return NWC24_ERR_DONE;
    }

    for (id = *pId; id < (NWC24WorkP == NULL ? NULL : (NWC24iDlHeader*)NWC24WorkP->dlHeader)->maxTasks; id++) {
        if (CheckDlEntryAvailable(id) >= 0) {
            *pId = id;
            return NWC24_OK;
        }
    }

    return NWC24_ERR_DONE;
}

static BOOL IsKeyBefore(s32 lhs, s32 rhs, BOOL desc) {
    return desc ? rhs < lhs : lhs < rhs;
}

NWC24Err NWC24iIterateDlTaskSorted(NWC24iDlSortIter* pIter, u16* pId) {
    BOOL desc;
    BOOL found = FALSE;
    NWC24iDlKeyFunc getKey;
    NWC24Err result;
    u16 id;
    s32 key;

    if (!pIter->valid) {
        return NWC24_ERR_INVALID_VALUE;
    }

    desc = pIter->mode >> 31;

    switch (pIter->mode & 0xFFFF) {
    case 0: {
        getKey = GetEntryLastAccess;
        break;
    }
    case 1: {
        getKey = GetEntryNextTime;
        break;
    }
    case 2: {
        getKey = GetEntryPriority;
        break;
    }
    default: {
        return NWC24_ERR_INVALID_VALUE;
    }
    }

    if (!pIter->first) {
        for (result = NWC24IterateDlTask(&id, TRUE); result >= 0; result = NWC24IterateDlTask(&id, FALSE)) {
            key = getKey(id);
            if (key == pIter->cur && pIter->lastId < id) {
                *pId = id;
                pIter->cur = key;
                pIter->lastId = id;
                pIter->prev = key;
                return NWC24_OK;
            }
        }
    } else {
        pIter->first = FALSE;
    }

    if (!(pIter->mode & 0x80000000)) {
        pIter->cur = 0x7FFFFFFF;
    } else {
        pIter->cur = 0x80000001;
    }

    for (result = NWC24IterateDlTask(&id, TRUE); result >= 0; result = NWC24IterateDlTask(&id, FALSE)) {
        key = getKey(id);

        if (IsKeyBefore(pIter->prev, key, desc) && IsKeyBefore(key, pIter->cur, desc)) {
            found = TRUE;
            *pId = id;
            pIter->cur = key;
            pIter->lastId = id;
        }
    }

    if (found) {
        pIter->prev = pIter->cur;
        return NWC24_OK;
    }

    pIter->valid = FALSE;
    return NWC24_ERR_DONE;
}

#pragma push
#pragma inline_max_auto_size(1000)
NWC24Err NWC24UpdateDlTask(NWC24DlTask* pTask) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTask(pTaskImpl, TRUE);
    if (result != NWC24_OK) {
        return result;
    }

    if (pTaskImpl->id == 0xFFFF || pTaskImpl->id >= NWC24iGetCachedDlHeader()->maxTasks) {
        return NWC24_ERR_INVALID_VALUE;
    }

    result = UpdateDlLastAccess(pTaskImpl);
    if (result < 0) {
        return result;
    }

    ClearDlTaskError(pTaskImpl);

    if (pTaskImpl->subTaskType == NWC24_DL_STTYPE_INCREMENT) {
        while ((result = CheckSubTaskEnabled(pTaskImpl, pTaskImpl->subTaskCounter)) == NWC24_ERR_DISABLED) {
            pTaskImpl->subTaskCounter = (u32)(pTaskImpl->subTaskCounter + 1) % NWC24i_DL_SUBTASK_MAX;
        }

        if (result < 0) {
            return result;
        }
    }

    return WriteDlTask(pTaskImpl);
}
#pragma pop

NWC24Err NWC24DeleteDlTask(NWC24DlTask* pTask) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    if (NWC24GetAppId() != NWC24i_APP_ID_IPL) {
        result = CheckDlTask(pTaskImpl, TRUE);
        if (result != NWC24_OK) {
            return result;
        }
    }

    return NWC24DeleteDlTaskForced(pTask);
}

NWC24Err NWC24AddDlTask(NWC24DlTask* pTask) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;
    s64 now;

    result = CheckHeader(NWC24iGetCachedDlHeader());
    if (result < 0) {
        return result;
    }

    result = AddDlTask(pTaskImpl, NWC24iGetCachedDlHeader()->privateTasks, NWC24iGetCachedDlHeader()->maxTasks);
    if (result >= 0) {
        now = 0;
        result = NWC24iGetUniversalTime(&now);
        if (result < 0) {
            return result;
        }

        result = SetDlNextTime(pTaskImpl, now + pTaskImpl->interval * 60);
    }

    return result;
}

NWC24Err NWC24GetDlNextTime(const NWC24DlTask* pTask, s64* pTime) {
    const NWC24iDlTask* pTaskImpl = (const NWC24iDlTask*)pTask;
    NWC24iDlEntry* pEntry;
    NWC24Err result;
    s64 now;

    result = CheckDlTask(pTaskImpl, FALSE);
    if (result != NWC24_OK) {
        return result;
    }

    pEntry = GetDlTaskEntryHeader(pTaskImpl->id);
    result = NWC24iGetUniversalTime(&now);

    if (pEntry == NULL) {
        return NWC24_ERR_FATAL;
    }

    if (result >= 0 && pEntry->nextTime > now + 604800) {
        pEntry->nextTime = now / 60 + 2880;
    }

    if (pTime == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }

    *pTime = pEntry->nextTime * 60;
    return NWC24_OK;
}

static NWC24Err InitDlSortIter(NWC24iDlSortIter* pIter, u32 key, BOOL desc) {
    memset(pIter, 0, sizeof(NWC24iDlSortIter));

    pIter->cur = desc ? 0x80000001 : 0x7FFFFFFF;
    pIter->prev = desc ? 0x7FFFFFFF : 0x80000001;
    pIter->mode = key | (desc ? 0x80000000 : 0);
    pIter->lastId = -1;
    pIter->first = TRUE;
    pIter->valid = TRUE;
    return NWC24_OK;
}

NWC24Err NWC24iDeleteOldestDlTask(void) {
    NWC24iDlSortIter iter;
    NWC24DlTask task;
    NWC24Err result;
    u16 id;

    result = InitDlSortIter(&iter, 0, FALSE);
    if (result < 0) {
        return result;
    }

    while ((result = NWC24iIterateDlTaskSorted(&iter, &id)) == NWC24_OK) {
        if (!IsPrivateId(id)) {
            break;
        }
    }

    if (result >= 0) {
        result = NWC24GetDlTask(&task, id);
        if (result < 0) {
            return result;
        }

        result = NWC24DeleteDlTaskForced(&task);
        if (result < 0) {
            return result;
        }
    } else if (result == NWC24_ERR_DONE) {
        result = NWC24_ERR_FAILED;
    }

    return result;
}

static NWC24Err GetDlTitleDir(const NWC24DlTask* pPublic, char* pBuf, u32 size) {
    const NWC24iDlTask* pTask = (const NWC24iDlTask*)pPublic;
    NWC24Err result;

    result = CheckDlTask(pTask, FALSE);
    if (result != NWC24_OK) {
        return result;
    }

    if (pTask->appId == 0) {
        return NWC24_ERR_INVALID_VALUE;
    }

    if (size < 30) {
        return NWC24_ERR_NOMEM;
    }

    snprintf(pBuf, size, "/title/%08x/%08x/data", pTask->titleIdHi, pTask->titleIdLo);
    return NWC24_OK;
}

static NWC24Err GetDlVfPath(const NWC24iDlTask* pTask, char* pBuf, u32 size) {
    const char* pFileName = DL_VF_FILE;
    NWC24Err result;
    u32 len;

    result = CheckDlTask(pTask, FALSE);
    if (result != NWC24_OK) {
        return result;
    }

    if (pTask->appId == 0) {
        return NWC24_ERR_INVALID_VALUE;
    }

    result = GetDlTitleDir((const NWC24DlTask*)pTask, pBuf, size);
    if (result < 0) {
        return result;
    }

    len = strlen(pBuf);
    snprintf(pBuf + len, size - len, "/%s", pFileName);
    return NWC24_OK;
}

NWC24Err NWC24GetDlVfPath(const NWC24DlTask* pTask, char* pBuf, u32 size) {
    return GetDlVfPath((const NWC24iDlTask*)pTask, pBuf, size);
}

NWC24Err NWC24CreateDlVf(const NWC24DlTask* pTask, u32 size) {
    static char path[128];
    NWC24Err result;

    result = CheckDlTask((const NWC24iDlTask*)pTask, TRUE);
    if (result != NWC24_OK) {
        return result;
    }

    if (size < 0x2800) {
        return NWC24_ERR_INVALID_VALUE;
    }

    GetDlVfPath((const NWC24iDlTask*)pTask, path, sizeof(path));
    return NWC24CreateVF(path, size);
}

NWC24Err NWC24DeleteDlTaskForced(NWC24DlTask* pTask) {
    NWC24iDlTask* pTaskImpl;
    NWC24Err result;

    pTaskImpl = (NWC24iDlTask*)pTask;

    result = CheckDlTask(pTaskImpl, FALSE);
    if (result != NWC24_OK) {
        return result;
    }

    result = DeleteDlTask(pTaskImpl);
    if (result < 0) {
        return result;
    }

    pTaskImpl->id = 0xFFFF;
    return result;
}

NWC24Err NWC24GetDlTask(NWC24DlTask* pTask, u16 id) {
    NWC24iDlTask* pTaskImpl;
    NWC24Err result;

    pTaskImpl = (NWC24iDlTask*)pTask;

    if (NWC24iGetCachedDlHeader() == NULL) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }
    result = CheckDlEntryAvailable(id);
    if (result < 0) {
        return result;
    }

    result = LoadDlTask(pTaskImpl, id);
    if (result < 0) {
        return result;
    }

    return result;
}

NWC24Err NWC24iOpenDlTaskList(void) {
    NWC24Err result;

    result = NWC24iLoadDlHeader();

    if (result >= 0) {
        NWC24iSynchronizeRtcCounter(FALSE);
        result = NWC24iCheckDlHeaderConsistency(NWC24iGetCachedDlHeader(), FALSE);

        if (result >= 0) {
            return NWC24_OK;
        }
    }

    return result;
}

NWC24Err NWC24iCloseDlTaskList(void) {
    return NWC24_OK;
}

NWC24iDlHeader* NWC24iGetCachedDlHeader(void) {
    if (NWC24WorkP != 0)
        return (NWC24iDlHeader*)NWC24WorkP->dlHeader;
    else
        return 0;
}

NWC24Err NWC24iCheckDlHeaderConsistency(NWC24iDlHeader* pHeader, BOOL clear) {
    NWC24iDlTask* pTaskImpl;
    NWC24DlTask task;
    u16 i;

    pTaskImpl = (NWC24iDlTask*)&task;

    for (i = 0; i < pHeader->maxTasks; i++) {
        if (CheckDlEntryAvailable(i) == NWC24_OK && clear) {
            if (NWC24GetDlTask(&task, i) < 0) {
                NWC24DeleteDlTaskForced(&task);
            } else if (!IsPrivateId(i) && pTaskImpl->count == 0) {
                NWC24DeleteDlTaskForced(&task);
            }
        }
    }

    return NWC24_OK;
}

NWC24Err NWC24iLoadDlHeader(void) {
    NWC24File file;
    NWC24Err result;
    NWC24Err close;
    NWC24Err ret;
    u32 length;

    length = 0;

    result = NWC24FOpen(&file, DLFilePath, NWC24_OPEN_NAND_R);
    if (result < 0) {
        return result;
    }

    result = ReadDlHeader(&file);
    if (result < 0) {
        return result;
    }

    result = NWC24FGetLength(&file, &length);
    if (result >= 0) {
        result = CheckHeader(NWC24iGetCachedDlHeader());
    }

    close = NWC24FClose(&file);

    if (result != NWC24_OK) {
        ret = result;
    } else {
        ret = close;
    }

    return ret;
}

static NWC24Err GetDlUrlEx(const NWC24iDlTask* pTask, char* pBuf, u32 size, BOOL trailing, u8 index) {
    NWC24Err result;
    u32 len;

    result = CheckDlTask(pTask, FALSE);
    if (result != NWC24_OK) {
        return result;
    }

    len = strlen(pTask->url);
    if (len == 0) {
        return NWC24_ERR_NOT_FOUND;
    }

    if (len < 8) {
        return NWC24_ERR_FAILED;
    }

    if (len + (trailing ? 3 : 0) + 1 > size) {
        return NWC24_ERR_NOMEM;
    }

    strncpy(pBuf, pTask->url, len);
    pBuf[len] = '\0';

    if (trailing) {
        snprintf(pBuf + len, size - len, ".%02d", index);
    }

    return NWC24_OK;
}

static NWC24Err SetDlEntryHeader(const NWC24iDlTask* pTask) {
    NWC24iDlEntry* pEntry = GetDlTaskEntryHeader(pTask->id);

    pEntry->app = pTask->appId;
    pEntry->flags = pTask->priority;
    return NWC24_OK;
}

static NWC24Err WriteDlTask(NWC24iDlTask* pTask) {
    NWC24File file;
    NWC24Err result;
    NWC24Err close;
    NWC24Err ret;

    result = NWC24FOpen(&file, DLFilePath, NWC24_OPEN_NAND_RW);
    if (result < 0) {
        return result;
    }

    result = WriteDlTaskEntry(pTask, &file);
    if (result >= 0) {
        result = SetDlEntryHeader(pTask);
        if (result >= 0) {
            result = WriteDlHeader(&file);
        }
    }

    close = NWC24FClose(&file);

    if (result != NWC24_OK) {
        ret = result;
    } else {
        ret = close;
    }

    return ret;
}

static NWC24Err CheckDlTaskParams(const NWC24iDlTask* pTask) {
    NWC24Err result;

    if (pTask->id >= NWC24iGetCachedDlHeader()->maxTasks && pTask->id != 0xFFFF) {
        return NWC24_ERR_INVALID_VALUE;
    }

    result = CheckDlUrl(pTask->url);
    if (result < 0) {
        return result;
    }

    return NWC24_OK;
}

static NWC24Err AssignDlTaskId(NWC24iDlTask* pTask, u16 minId, u16 maxId) {
    NWC24iDlHeader* pHeader = NWC24iGetCachedDlHeader();
    u16 id;

    if (pTask == NULL || minId > maxId || minId >= pHeader->maxTasks || maxId > pHeader->maxTasks) {
        return NWC24_ERR_INVALID_VALUE;
    }

    for (id = minId; id < maxId; id++) {
        if (GetDlTaskEntryHeader(id)->app == 0) {
            pTask->id = id;
            return NWC24_OK;
        }
    }

    return NWC24_ERR_FULL;
}

static NWC24Err AddDlTask(NWC24iDlTask* pTask, u16 minId, u16 maxId) {
    NWC24Err result;

    result = CheckDlTask(pTask, TRUE);
    if (result != NWC24_OK) {
        return result;
    }

    result = CheckDlTaskParams(pTask);
    if (result < 0) {
        return result;
    }

    while (TRUE) {
        if (pTask->id != 0xFFFF) {
            return NWC24UpdateDlTask((NWC24DlTask*)pTask);
        }

        result = AssignDlTaskId(pTask, minId, maxId);
        if (result == NWC24_ERR_FULL) {
            result = NWC24iDeleteOldestDlTask();
            if (result < 0) {
                return result;
            }
        } else if (result < 0) {
            return result;
        }
    }
}

static NWC24iDlEntry* GetDlTaskEntryHeader(u16 id) {
    return &NWC24iGetCachedDlHeader()->entries[id];
}

static NWC24Err WriteDlHeader(NWC24File* pFile) {
    NWC24Err result;

    result = NWC24FSeek(pFile, 0, NWC24_SEEK_BEG);
    if (result < 0) {
        return result;
    }

    result = NWC24FWrite(NWC24iGetCachedDlHeader(), sizeof(NWC24iDlHeader), pFile);
    if (result < 0) {
        return result;
    }

    return NWC24_OK;
}

static NWC24Err SeekDlTaskEntry(u16 id, NWC24File* pFile) {
    u16 maxTasks = NWC24iGetCachedDlHeader()->maxTasks;
    if (maxTasks > NWC24i_DL_TASK_MAX || id >= maxTasks) {
        return NWC24_ERR_INVALID_VALUE;
    }
    return NWC24FSeek(pFile, sizeof(NWC24iDlHeader) + id * sizeof(NWC24iDlTask), NWC24_SEEK_BEG);
}

static NWC24Err CheckDlEntryAvailable(u16 id) {
    if (id >= NWC24iGetCachedDlHeader()->maxTasks || id == 0xFFFF) {
        return NWC24_ERR_INVALID_VALUE;
    }

    if (GetDlTaskEntryHeader(id)->app == 0) {
        return NWC24_ERR_NOT_FOUND;
    }

    return NWC24_OK;
}

static NWC24Err WriteDlTaskEntry(NWC24iDlTask* pTask, NWC24File* pFile) {
    NWC24Err result;

    result = SeekDlTaskEntry(pTask->id, pFile);
    if (result < 0) {
        return result;
    }

    memcpy(NWC24WorkP->dlTask, pTask, sizeof(NWC24iDlTask));
    result = NWC24FWrite(NWC24WorkP->dlTask, sizeof(NWC24iDlTask), pFile);
    if (result < 0) {
        return result;
    }

    return NWC24_OK;
}

static NWC24Err ReadDlTaskEntry(NWC24iDlTask* pTask, u16 id, NWC24File* pFile) {
    NWC24Err result;

    result = SeekDlTaskEntry(id, pFile);
    if (result < 0) {
        return result;
    }

    result = NWC24FRead(pTask, sizeof(NWC24iDlTask), pFile);
    if (result < 0) {
        return result;
    }

    return NWC24_OK;
}

static NWC24Err ClearDlTaskEntry(u16 id, NWC24File* pFile) {
    NWC24iDlTask* pTask = (NWC24iDlTask*)NWC24WorkP->dlTask;

    memset(pTask, 0, sizeof(NWC24iDlTask));
    pTask->type = 0xFF;
    pTask->id = id;

    InitTaskEntryHeader(id);
    return WriteDlTaskEntry(pTask, pFile);
}

static NWC24Err ReadDlHeader(NWC24File* pFile) {
    NWC24Err result;

    result = NWC24FSeek(pFile, 0, NWC24_SEEK_BEG);
    if (result < 0) {
        return result;
    }

    result = NWC24FRead(&NWC24WorkP->dlHeader, sizeof(NWC24iDlHeader), pFile);
    if (result < 0) {
        return result;
    }

    return NWC24_OK;
}

static void InitTaskEntryHeader(u16 id) {
    NWC24iDlHeader* pHeader = (NWC24iDlHeader*)NWC24WorkP->dlHeader;
    memset(&pHeader->entries[id], 0, sizeof(NWC24iDlEntry));
}

static NWC24Err LoadDlTask(NWC24iDlTask* pTask, u16 id) {
    NWC24File file;
    NWC24Err result;
    NWC24Err close;
    NWC24Err ret;

    result = NWC24FOpen(&file, DLFilePath, NWC24_OPEN_NAND_RBUFF);
    if (result < 0) {
        return result;
    }

    result = ReadDlTaskEntry(pTask, id, &file);
    close = NWC24FClose(&file);

    if (result != NWC24_OK) {
        ret = result;
    } else {
        ret = close;
    }

    return ret;
}

static NWC24Err DeleteDlTask(NWC24iDlTask* pTask) {
    NWC24File file;
    NWC24Err result;
    NWC24Err close;
    NWC24Err ret;

    result = NWC24FOpen(&file, DLFilePath, NWC24_OPEN_NAND_RW);
    if (result < 0) {
        return result;
    }

    result = ClearDlTaskEntry(pTask->id, &file);
    if (result >= 0) {
        InitTaskEntryHeader(pTask->id);
        result = WriteDlHeader(&file);
    }

    close = NWC24FClose(&file);

    if (result != NWC24_OK) {
        ret = result;
    } else {
        ret = close;
    }

    return ret;
}

static NWC24Err CheckHeader(NWC24iDlHeader* pHeader) {
    NWC24File file;
    if (pHeader->maxTasks == 0 && pHeader->maxSubTasks != 0) {
        pHeader->maxTasks = pHeader->maxSubTasks;
        if (pHeader->maxSubTasks > 32) {
            pHeader->maxSubTasks = 32;
        }
        if (NWC24FOpen(&file, DLFilePath, NWC24_OPEN_NAND_RW) >= 0) {
            WriteDlHeader(&file);
            NWC24FClose(&file);
        }
    }

    if (pHeader->maxTasks < 1 || pHeader->privateTasks < 1 || pHeader->maxTasks < pHeader->privateTasks) {
        return NWC24_ERR_BROKEN;
    }

    return NWC24_OK;
}

static s32 GetEntryLastAccess(u16 id) {
    return GetDlTaskEntryHeader(id)->lastAccess;
}

static s32 GetEntryNextTime(u16 id) {
    return GetDlTaskEntryHeader(id)->nextTime;
}

static s32 GetEntryPriority(u16 id) {
    return GetDlTaskEntryHeader(id)->flags;
}
