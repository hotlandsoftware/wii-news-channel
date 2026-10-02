#include <revolution/nwc24.h>
#include <revolution/nwc24/NWC24Internal.h>
#include <revolution/os.h>

#include <string.h>

#define CHECK_CALLING_STATUS(block) CheckCallingStatus(__FUNCTION__, block)

enum {
    NWC24_IOCTL_SUSPEND_SCHEDULER = 1,
    NWC24_IOCTL_TRY_SUSPEND_SCHEDULER = 2,
    NWC24_IOCTL_RESUME_SCHEDULER = 3,
    NWC24_IOCTL_STARTUP_SOCKET = 6,
    NWC24_IOCTL_CLEANUP_SOCKET = 7,
    NWC24_IOCTL_LOCK_SOCKET = 8,
    NWC24_IOCTL_UNLOCK_SOCKET = 9,
    NWC24_IOCTL_SAVE_MAIL_NOW = 13,
    NWC24_IOCTL_DOWNLOAD_NOW_EX = 14,
    NWC24_IOCTL_GENERATE_USER_ID = 15,
    NWC24_IOCTL_GET_SCHEDULER_STAT = 30,
};

typedef struct CommonBuffer {
    u32 WORD_0x0;
    u32 WORD_0x4;
    u32 WORD_0x8;
    u8 padding[32 - 0xC];
} __attribute__((packed)) CommonBuffer;

typedef struct CommonResult {
    s32 result;

    union {
        u32 userid[2];
        s32 exResult;
    };

    u32 WORD_0xC;
    u8 padding[32 - 0x10];
} __attribute__((packed)) CommonResult;

static u32 nwc24ScdInitialized = 0;
static s32 nwc24ScdSuspendCnt = 0;
static s32 nwc24ScdOpenCnt = 0;

static OSMutex nwc24ScdCommandMutex;
static OSMutex nwc24ScdCounterMutex;

static CommonBuffer nwc24ScdCommonBuffer __attribute__((aligned(32)));
static CommonResult nwc24ScdCommonResult __attribute__((aligned(32)));

u8 nwc24ScdStatBuf[256] __attribute__((aligned(32)));

static NWC24Err ExecSuspendScheduler(void) NO_INLINE;
static NWC24Err ExecTrySuspendScheduler(u32 arg0);
static NWC24Err ExecResumeScheduler(void) NO_INLINE;
static NWC24Err ExecNoParamCommand(const char* pUser, s32 type, NWC24Err* pExResult);

static void InitScdMutex(void);

static void LockRight(void);
static BOOL TryLockRight(void);
static void UnlockRight(void);

static void LockCounters(void);
static BOOL TryLockCounters(void);
static void UnlockCounters(void);

static NWC24Err CheckCallingStatus(const char* pUser, BOOL block);

#define SchedulerDevice NWC24i_SCHEDULER_DEVICE

s32 NWC24SuspendScheduler(void) {
    s32 count;

    LockCounters();
    {
        count = ExecSuspendScheduler();

        if (count >= 0) {
            nwc24ScdSuspendCnt++;
            count -= nwc24ScdOpenCnt;
        }
    }
    UnlockCounters();

    return count;
}

s32 NWC24ResumeScheduler(void) {
    s32 count;

    LockCounters();
    {
        if (nwc24ScdOpenCnt > 0 && nwc24ScdSuspendCnt == 0) {
            count = 0;
        } else {
            count = ExecResumeScheduler();

            if (nwc24ScdSuspendCnt > 0) {
                nwc24ScdSuspendCnt--;
                count -= nwc24ScdOpenCnt;
            }
        }
    }
    UnlockCounters();

    return count;
}

NWC24Err NWC24iGetSchedulerStat(NWC24ScdStat* pStat, u32 size) {
    s32 fd;
    NWC24Err result;
    NWC24Err close;

    if (size > sizeof(nwc24ScdStatBuf)) {
        return NWC24_ERR_OVERFLOW;
    }

    if (OSGetCurrentThread() == NULL) {
        return NWC24_ERR_FATAL;
    }

    LockRight();
    {
        result = NWC24_OPEN_DEVICE(SchedulerDevice, &fd, 0);

        if (result >= 0) {
            result = NWC24_IOCTL_DEVICE(fd, NWC24_IOCTL_GET_SCHEDULER_STAT, NULL, 0, pStat, size);

            if (result >= 0) {
                result = pStat->result;
            }

            close = NWC24_CLOSE_DEVICE(fd);
            if (result >= 0) {
                result = close;
            }
        }

        if (size > offsetof(NWC24ScdStat, newMsgFlag) + sizeof(u32)) {
            NWC24iSetNewMsgArrived(pStat->newMsgFlag);
        }
    }
    UnlockRight();

    return result;
}

NWC24Err NWC24iSetScriptMode(s32 mode) {
    IOSFd fId;
    NWC24Err err;
    OSThread* thread = OSGetCurrentThread();

    if (thread == 0) {
        err = NWC24_ERR_FATAL;
    } else if (NWC24IsMsgLibOpened() || NWC24IsMsgLibOpenedByTool()) {
        err = NWC24_ERR_LIB_OPENED;
    } else if (NWC24IsMsgLibOpenBlocking()) {
        err = NWC24_ERR_BUSY;
    } else {
        err = NWC24_OK;
    }

    if (err < 0) {
        return err;
    } else {
        NWC24Err resErr;

        LockRight();
        memset(&nwc24ScdCommonBuffer, 0, 0x20);
        resErr = NWC24iOpenResourceManager(__FUNCTION__, SchedulerDevice, &fId, 0);

        if (resErr >= 0) {
            nwc24ScdCommonBuffer.WORD_0x0 = mode;
            resErr = NWC24iIoctlResourceManager(__FUNCTION__, fId, 0x22, &nwc24ScdCommonBuffer, 0x20, &nwc24ScdCommonResult, 0x20);

            if (resErr >= 0) {
                resErr = nwc24ScdCommonResult.result;
            }

            {
                s32 tmp = NWC24iCloseResourceManager(__FUNCTION__, fId);

                if (resErr >= 0) {
                    resErr = tmp;
                }
            }
        }

        UnlockRight();
        return resErr;
    }

    return err;
}

NWC24Err NWC24iRequestGenerateUserId(NWC24UserId* pUserId, u32* arg1) {
    s32 fd;
    NWC24Err result;
    NWC24Err close;

    result = CHECK_CALLING_STATUS(TRUE);
    if (result < 0) {
        return result;
    }

    LockRight();
    {
        result = NWC24_OPEN_DEVICE(SchedulerDevice, &fd, 0);

        if (result >= 0) {
            result = NWC24_IOCTL_DEVICE(fd, NWC24_IOCTL_GENERATE_USER_ID, NULL, 0, &nwc24ScdCommonResult, sizeof(CommonResult));

            if (result >= 0) {
                result = nwc24ScdCommonResult.result;

                if (result == NWC24_OK || result == NWC24_ERR_ID_GENERATED || result == NWC24_ERR_ID_REGISTERED) {
                    if (pUserId != NULL) {
                        *pUserId = *(NWC24UserId*)nwc24ScdCommonResult.userid;
                    }

                    if (arg1 != NULL) {
                        *arg1 = nwc24ScdCommonResult.WORD_0xC;
                    }
                }
            }

            close = NWC24_CLOSE_DEVICE(fd);
            if (result >= 0) {
                result = close;
            }
        }
    }
    UnlockRight();

    return result;
}

NWC24Err NWC24ExecDownloadTask(u32 arg0, u32 arg1, u32 arg2) {
    static const char* path = "dlcnt.bin";
    NWC24Err result = NWC24_OK;
    BOOL saveMail = FALSE;
    NANDStatus stat;
    NWC24ScdStat* pStat;
    u32 errorIdx;

    if (NANDPrivateGetStatus(path, &stat) == NAND_RESULT_OK) {
        NANDPrivateDelete(path);
    }

    result = NWC24iGetSchedulerStat((NWC24ScdStat*)nwc24ScdStatBuf, sizeof(nwc24ScdStatBuf));
    if (result < 0) {
        return result;
    }

    pStat = (NWC24ScdStat*)nwc24ScdStatBuf;
    errorIdx = pStat->numErrors;

    result = NWC24iDownloadNowEx(&saveMail, arg0, arg1, arg2);
    if (result >= 0 && saveMail) {
        result = NWC24iSaveMailNow();
    } else if (result == NWC24_ERR_PROTECTED) {
        result = result;
    }

    if (result < 0) {
        if (NWC24iGetSchedulerStat((NWC24ScdStat*)nwc24ScdStatBuf, sizeof(nwc24ScdStatBuf)) >= 0) {
            pStat = (NWC24ScdStat*)nwc24ScdStatBuf;
            NWC24iSetErrorCode(pStat->errorLog[errorIdx]);
        } else {
            NWC24iSetErrorCode(result - 107200);
        }
    } else {
        NWC24iSetErrorCode(0);
    }

    return result;
}

NWC24Err NWC24iStartupSocket(NWC24Err* pExResult) {
    return ExecNoParamCommand(NULL, NWC24_IOCTL_STARTUP_SOCKET, pExResult);
}

NWC24Err NWC24iCleanupSocket(NWC24Err* pExResult) {
    return ExecNoParamCommand(NULL, NWC24_IOCTL_CLEANUP_SOCKET, pExResult);
}

NWC24Err NWC24iLockSocket(void) {
    return ExecNoParamCommand(NULL, NWC24_IOCTL_LOCK_SOCKET, NULL);
}

NWC24Err NWC24iUnlockSocket(void) {
    return ExecNoParamCommand(NULL, NWC24_IOCTL_UNLOCK_SOCKET, NULL);
}

NWC24Err NWC24iSaveMailNow(void) {
    NWC24Err result;

    result = CHECK_CALLING_STATUS(FALSE);
    if (result < 0) {
        return result;
    }

    return ExecNoParamCommand(NULL, NWC24_IOCTL_SAVE_MAIL_NOW, NULL);
}

NWC24Err NWC24iDownloadNowEx(BOOL* pSaveMail, u32 arg0, u32 arg1, u32 arg2) {
    s32 fd;
    NWC24Err result;
    NWC24Err close;

    result = CHECK_CALLING_STATUS(FALSE);
    if (result < 0) {
        return result;
    }

    LockRight();
    {
        result = NWC24_OPEN_DEVICE(SchedulerDevice, &fd, 0);

        if (result >= 0) {
            nwc24ScdCommonBuffer.WORD_0x0 = arg0;
            nwc24ScdCommonBuffer.WORD_0x4 = arg1;
            nwc24ScdCommonBuffer.WORD_0x8 = arg2;

            result = NWC24_IOCTL_DEVICE(fd, NWC24_IOCTL_DOWNLOAD_NOW_EX, &nwc24ScdCommonBuffer, sizeof(CommonBuffer), &nwc24ScdCommonResult,
                                        sizeof(CommonResult));

            if (result >= 0) {
                result = nwc24ScdCommonResult.result;
                *pSaveMail = nwc24ScdCommonResult.userid[1];
            }

            close = NWC24_CLOSE_DEVICE(fd);
            if (result >= 0) {
                result = close;
            }
        }
    }
    UnlockRight();

    return result;
}

NWC24Err NWC24iTrySuspendForOpenLib(void) {
    NWC24Err result;

    if (!TryLockCounters()) {
        return NWC24_ERR_MUTEX;
    }

    result = ExecTrySuspendScheduler(0);
    if (result >= 0) {
        nwc24ScdOpenCnt++;
        result = NWC24_OK;
    }

    UnlockCounters();
    return result;
}

NWC24Err NWC24iResumeForCloseLib(void) {
    NWC24Err result;

    LockCounters();
    {
        result = ExecResumeScheduler();
        if (result >= 0) {
            nwc24ScdOpenCnt--;
            result = NWC24_OK;
        }
    }
    UnlockCounters();

    return result;
}

static NWC24Err ExecSuspendScheduler(void) {
    return ExecNoParamCommand(NULL, NWC24_IOCTL_SUSPEND_SCHEDULER, NULL);
}

static NWC24Err ExecTrySuspendScheduler(u32 arg0) {
    s32 fd;
    NWC24Err result;
    NWC24Err close;

    result = CHECK_CALLING_STATUS(TRUE);
    if (result < 0) {
        return result;
    }

    if (!TryLockRight()) {
        return NWC24_ERR_MUTEX;
    }

    result = NWC24_OPEN_DEVICE(SchedulerDevice, &fd, 0);

    if (result >= 0) {
        nwc24ScdCommonBuffer.WORD_0x0 = arg0;

        result = NWC24_IOCTL_DEVICE(fd, NWC24_IOCTL_TRY_SUSPEND_SCHEDULER, &nwc24ScdCommonBuffer, sizeof(CommonBuffer), &nwc24ScdCommonResult,
                                    sizeof(CommonResult));

        if (result >= 0) {
            result = nwc24ScdCommonResult.result;
        }

        close = NWC24_CLOSE_DEVICE(fd);
        if (close < 0) {
            result = close;
        }
    }

    UnlockRight();
    return result;
}

static NWC24Err ExecResumeScheduler(void) {
    return ExecNoParamCommand(NULL, NWC24_IOCTL_RESUME_SCHEDULER, NULL);
}

static NWC24Err ExecNoParamCommand(const char* pUser, s32 type, NWC24Err* pExResult) {
    s32 fd;
    NWC24Err result;
    NWC24Err close;

    OSThread* thread = OSGetCurrentThread();
    if (thread == 0) {
        return NWC24_ERR_FATAL;
    }

    LockRight();
    {
        result = NWC24iOpenResourceManager(pUser, SchedulerDevice, &fd, 0);

        if (result >= 0) {
            result = NWC24iIoctlResourceManager(pUser, fd, type, NULL, 0, &nwc24ScdCommonResult, sizeof(CommonResult));

            if (result >= 0) {
                result = nwc24ScdCommonResult.result;

                if (result == NWC24_ERR_FAILED || result == NWC24_ERR_CONFIG_NETWORK) {
                    if (pExResult != (void*)NULL) {
                        *pExResult = nwc24ScdCommonResult.exResult;
                    }
                }
            }

            close = NWC24iCloseResourceManager(pUser, fd);
            if (close < 0) {
                result = close;
            }
        }
    }
    UnlockRight();

    return result;
}

static void InitScdMutex(void) {
    BOOL enabled = OSDisableInterrupts();

    if (!(nwc24ScdInitialized & 1)) {
        OSInitMutex(&nwc24ScdCommandMutex);
        OSInitMutex(&nwc24ScdCounterMutex);

        memset(&nwc24ScdCommonBuffer, 0, sizeof(CommonBuffer));
        memset(&nwc24ScdCommonResult, 0, sizeof(CommonResult));

        nwc24ScdInitialized |= 1;
    }

    OSRestoreInterrupts(enabled);
}

static void LockRight(void) {
    if (!(nwc24ScdInitialized & 1)) {
        InitScdMutex();
    }

    OSLockMutex(&nwc24ScdCommandMutex);
}

static BOOL TryLockRight(void) {
    if (!(nwc24ScdInitialized & 1)) {
        InitScdMutex();
    }

    return OSTryLockMutex(&nwc24ScdCommandMutex);
}

static void UnlockRight(void) {
    OSUnlockMutex(&nwc24ScdCommandMutex);
}

static void LockCounters(void) {
    if (!(nwc24ScdInitialized & 1)) {
        InitScdMutex();
    }

    OSLockMutex(&nwc24ScdCounterMutex);
}

static BOOL TryLockCounters(void) {
    if (!(nwc24ScdInitialized & 1)) {
        InitScdMutex();
    }

    return OSTryLockMutex(&nwc24ScdCounterMutex);
}

static void UnlockCounters(void) {
    OSUnlockMutex(&nwc24ScdCounterMutex);
}

static NWC24Err CheckCallingStatus(const char* pUser, BOOL block) {
    OSThread* thread = OSGetCurrentThread();
    if (thread == 0) {
        return NWC24_ERR_FATAL;
    }

    if (NWC24IsMsgLibOpened() || NWC24IsMsgLibOpenedByTool()) {
        return NWC24_ERR_LIB_OPENED;
    }

    if (!block && NWC24IsMsgLibOpenBlocking()) {
        return NWC24_ERR_BUSY;
    }

    return NWC24_OK;
}
