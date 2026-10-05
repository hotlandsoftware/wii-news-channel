// WiiConnect24.cpp: the news download module. Requests (download, update, register and
// unregister the WiiConnect24 download task) are queued to a worker thread, which talks to
// NWC24, reads the downloaded files out of the task's VF archive and LZ77-decompresses them.

#include <news/WiiConnect24.h>
#include <news/System.h>
#include <revolution/cx.h>
#include <revolution/mem.h>
#include <revolution/nand.h>
#include <revolution/net.h>
#include <revolution/nwc24.h>
#include <revolution/nwc24/internal/NWC24iSystem.h>
#include <revolution/nwc24/internal/NWC24iSchedule.h>
#include <revolution/os.h>
#include <revolution/sc.h>
#include <revolution/so.h>
#include <revolution/vf.h>
#include <stdio.h>

struct NewsHeader;

enum {
    WC24_REQ_DOWNLOAD,   // read the downloaded files, registering the task first if needed
    WC24_REQ_UPDATE,     // download now, then read the files
    WC24_REQ_REGISTER,   // register (or update) the download task
    WC24_REQ_UNREGISTER, // delete the download task and its VF
    WC24_REQ_QUIT,
};

enum {
    WC24_STATUS_IDLE = 0,
    WC24_STATUS_QUEUED = 1,
    WC24_STATUS_LIB = 2,
    WC24_STATUS_READ = 3,
    WC24_STATUS_DOWNLOAD = 5,
    WC24_STATUS_SETUP = 6,
    WC24_STATUS_DELETE = 7,
};

#define WC24_NUM_FILES 24
#define WC24_DRIVE "@24"

// One request to the worker thread.
class CWiiConnect24 {
public:
    s32 readFiles();
    s32 setupDlTasks(BOOL first, BOOL second, u8 force, u16 interval, u16 count, u16* idOut);
    s32 execDownload(s32 index, u32 mask, u16 id);
    s32 deleteDlTasks(BOOL first, BOOL second);
    s32 readLZ77FileEx(VFFile file, MEMHeapHandle heap, void** dst, u32* size);


    s32 mType;                           // at 0x000
    u16 mKind[2];                        // at 0x004 (2: the news download task)
    u32 mMask;                           // at 0x008
    MEMHeapHandle mHeap;                 // at 0x00C
    MEMHeapHandle mTmpHeap;              // at 0x010
    void** mFiles[WC24_NUM_FILES];       // at 0x014
    s64* mTimes[WC24_NUM_FILES];         // at 0x074
    u32* mSizes[WC24_NUM_FILES];         // at 0x0D4
    union {
        s64 mNextTime;                   // at 0x138
        u32 mNextTimeWords[2];
    };
    union {
        s64 mUnk140;                     // at 0x140
        u32 mUnk140Words[2];
    };
    const char* mUrl[2];                 // at 0x148
    u32 mVfSize;                         // at 0x150
    u32 mUnk154;                         // at 0x154
    u32 mUnk158;                         // at 0x158
    u16 mInterval;                       // at 0x15C
    u16 mCount;                          // at 0x15E
    u8 mForce;                           // at 0x160
    s32 mStatus;                         // at 0x164
    s32 mResult;                         // at 0x168
    s32 mErrorCode;                      // at 0x16C
    s32 mDetail;                         // at 0x170
    char mMessage[0x40];                 // at 0x174
};

extern u8 gWC24Work[0x4000];
extern OSMessageQueue gWC24Queue;
extern OSMessage gWC24Messages[16];
extern OSThread gWC24Thread;
extern u8 gWC24Stack[0x8000];
extern u8 gWC24ReadBuf[0x10000];

extern CWiiConnect24 gWC24Tasks[8];

static bool sLibOpen;
static bool sTasksReady;
static bool sDownloading;
static bool sSOReady;
static bool sSOStarting;
static volatile s32 sTaskHead;
static volatile s32 sTaskTail;
static volatile s32 sTaskCount;
static void* sSOHeapMem;
static MEMHeapHandle sSOHeap;

static s32 ConvertError(NWC24Err err);

// Records an error in the request.
static inline void SetError(CWiiConnect24* task, const char* msg, s32 code, s32 detail) {
    task->mErrorCode = code;
    task->mDetail = detail;
    sprintf(task->mMessage, "%s %d %d", msg, code, detail);
}

static void* ThreadMain(void* arg);
static void* SOAllocFunc(u32 name, s32 size);
static void SOFreeFunc(u32 name, void* ptr, s32 size);

void WC24Init() {
    sSOHeapMem = SubHeapAlloc(0x10000, 32);
    sSOHeap = MEMCreateExpHeapEx(sSOHeapMem, 0x10000, 4);
    OSInitMessageQueue(&gWC24Queue, gWC24Messages, 16);
    OSCreateThread(&gWC24Thread, ThreadMain, NULL, gWC24Stack + sizeof(gWC24Stack), sizeof(gWC24Stack), 8, 1);
    OSResumeThread(&gWC24Thread);
    sTaskHead = 0;
    sTaskTail = 0;
    sTaskCount = 0;
}

void WC24Calc() {
    if (sTaskCount > 0 && gWC24Tasks[sTaskHead].mStatus == WC24_STATUS_IDLE) {
        sTaskHead++;
        sTaskCount--;
        if (sTaskHead >= sTaskCount) {
            sTaskHead = 0;
        }
    }
}

void WC24Shutdown(u32 event) {
    for (s32 i = 0; i < 10 && (sDownloading || sSOStarting); i++) {
        NWC24iRequestShutdownSync(event);
        OSSleepTicks(OSSecondsToTicks((OSTime)1));
    }
    OSJoinThread(&gWC24Thread, NULL);
}

static inline s32 PushTask(CWiiConnect24& task) {
    s32 id = -1;
    if (sTaskCount < 8) {
        gWC24Tasks[sTaskTail] = task;
        gWC24Tasks[sTaskTail].mStatus = WC24_STATUS_QUEUED;
        gWC24Tasks[sTaskTail].mErrorCode = 0;
        gWC24Tasks[sTaskTail].mMessage[0] = '\0';
        if (OSSendMessage(&gWC24Queue, &gWC24Tasks[sTaskTail], OS_MESSAGE_NOBLOCK)) {
            id = sTaskTail;
            sTaskTail++;
            sTaskCount++;
            if (sTaskTail >= sTaskCount) {
                sTaskTail = 0;
            }
        }
    }
    return id;
}

s32 WC24RequestDownload(MEMHeapHandle heap, u32 tmpHeap, NewsHeader** files, u32* times,
                        u32* sizes, const char* url, u32 vfSize) {
    CWiiConnect24 task;
    task.mType = WC24_REQ_DOWNLOAD;
    task.mKind[0] = 2;
    task.mKind[1] = 0;
    task.mHeap = heap;
    task.mTmpHeap = (MEMHeapHandle)tmpHeap;
    for (s32 i = 0; i < WC24_NUM_FILES; i++) {
        if (files[i]) {
            MEMFreeToExpHeap(heap, files[i]);
            files[i] = NULL;
        }
        task.mFiles[i] = (void**)&files[i];
        task.mTimes[i] = (s64*)&times[i * 2];
        task.mSizes[i] = &sizes[i];
    }
    task.mUrl[0] = url;
    task.mVfSize = vfSize;
    return PushTask(task);
}

s32 WC24RequestUpdate(MEMHeapHandle heap, u32 tmpHeap, NewsHeader** files, u32* times,
                      u32* sizes, u32 mask) {
    CWiiConnect24 task;
    task.mType = WC24_REQ_UPDATE;
    task.mKind[0] = 2;
    task.mKind[1] = 0;
    task.mMask = mask;
    task.mHeap = heap;
    task.mTmpHeap = (MEMHeapHandle)tmpHeap;
    for (s32 i = 0; i < WC24_NUM_FILES; i++) {
        if (files[i]) {
            MEMFreeToExpHeap(heap, files[i]);
            files[i] = NULL;
        }
        task.mFiles[i] = (void**)&files[i];
        task.mTimes[i] = (s64*)&times[i * 2];
        task.mSizes[i] = &sizes[i];
    }
    task.mUrl[0] = NULL;
    return PushTask(task);
}

s32 WC24RequestRegister(const char* url, u32 vfSize, u32 force, u8 interval, u16 count) {
    CWiiConnect24 task;
    task.mType = WC24_REQ_REGISTER;
    task.mKind[0] = 2;
    task.mKind[1] = 0;
    task.mUrl[0] = url;
    task.mVfSize = vfSize;
    task.mForce = force;
    if (interval == 0) {
        task.mInterval = 30;
    } else {
        task.mInterval = interval;
    }
    if (count == 0) {
        task.mCount = 240;
    } else {
        task.mCount = count;
    }
    return PushTask(task);
}

s32 WC24RequestUnregister() {
    CWiiConnect24 task;
    task.mType = WC24_REQ_UNREGISTER;
    task.mKind[0] = 2;
    task.mKind[1] = 0;
    return PushTask(task);
}


static inline s32 OpenLib(CWiiConnect24* task) {
    NWC24Err err;
    for (;;) {
        err = NWC24OpenLib(gWC24Work);
        if (err != NWC24_ERR_MUTEX && err != NWC24_ERR_BUSY && err != NWC24_ERR_INPROGRESS) {
            break;
        }
        OSSleepTicks(OSMillisecondsToTicks((OSTime)100));
    }
    if (err != NWC24_OK) {
        SetError(task, "NWC24OpenLib() failed.", NWC24GetErrorCode(), err);
        return ConvertError(err);
    }
    sLibOpen = true;
    return 0;
}

static inline s32 CheckLib(CWiiConnect24* task) {
    NWC24Err err = NWC24Check(2);
    if (err != NWC24_OK) {
        SetError(task, "NWC24Check() failed.", NWC24GetErrorCode(), err);
        return ConvertError(err);
    }
    SCIdleModeInfo idle;
    if (!SCGetIdleMode(&idle) || idle.mode != 1) {
        return -3;
    }
    return 0;
}

static inline s32 CloseLib(CWiiConnect24* task) {
    NWC24Err err = NWC24CloseLib();
    if (err != NWC24_OK) {
        SetError(task, "NWC24CloseLib() failed.", NWC24GetErrorCode(), err);
        return ConvertError(err);
    }
    sLibOpen = false;
    return 0;
}

static void* ThreadMain(void* arg) {
    bool quit;
    do {
        quit = false;
        OSMessage msg;
        OSReceiveMessage(&gWC24Queue, &msg, OS_MESSAGE_BLOCK);
        CWiiConnect24* task = (CWiiConnect24*)msg;
        if (task == NULL) {
            break;
        }

        switch (task->mType) {
        case WC24_REQ_DOWNLOAD: {
            BOOL opened = FALSE;
            s32 retry = 0;
            u16 id;
            for (;;) {
                id = 0xFFFF;
                task->mStatus = WC24_STATUS_LIB;
                if ((task->mResult = OpenLib(task)) != 0) {
                    break;
                }
                opened = TRUE;
                if ((task->mResult = CheckLib(task)) != 0) {
                    break;
                }
                task->mStatus = WC24_STATUS_READ;
                if ((task->mResult = task->readFiles()) == 0) {
                    break;
                }
                if (task->mResult != -1 || retry == 1) {
                    break;
                }
                task->mStatus = WC24_STATUS_SETUP;
                if ((task->mResult = task->setupDlTasks(TRUE, FALSE, TRUE, 30, 240, &id)) != 0) {
                    break;
                }
                task->mStatus = WC24_STATUS_LIB;
                task->mResult = CloseLib(task);
                opened = FALSE;
                if (task->mResult != 0) {
                    break;
                }
                for (s32 i = 0; i < WC24_NUM_FILES; i++) {
                    if (*task->mFiles[i]) {
                        MEMFreeToExpHeap(task->mHeap, *task->mFiles[i]);
                        *task->mFiles[i] = NULL;
                    }
                }
                task->mStatus = WC24_STATUS_DOWNLOAD;
                if ((task->mResult = task->execDownload(0, 0xFFFFFF, id)) != 0) {
                    break;
                }
                retry++;
            }
            if (opened) {
                task->mStatus = WC24_STATUS_LIB;
                s32 result = CloseLib(task);
                if (task->mResult == 0) {
                    task->mResult = result;
                }
            }
            task->mStatus = WC24_STATUS_IDLE;
            break;
        }
        case WC24_REQ_UPDATE: {
            task->mStatus = WC24_STATUS_LIB;
            if ((task->mResult = OpenLib(task)) != 0) {
                break;
            }
            NWC24DlTask dl;
            u16 id;
            s32 result;
            NWC24Err err = NWC24GetMyDlTask(&dl);
            if (err != NWC24_OK) {
                SetError(task, "NWC24GetDlTaskMine() failed.", NWC24GetErrorCode(), err);
                result = ConvertError(err);
            } else {
                err = NWC24GetDlTaskId(&dl, &id);
                if (err != NWC24_OK) {
                    SetError(task, "NWC24GetDlId() failed.", NWC24GetErrorCode(), err);
                    result = ConvertError(err);
                } else {
                    result = 0;
                }
            }
            if ((task->mResult = result) != 0) {
                break;
            }
            task->mStatus = WC24_STATUS_LIB;
            if ((task->mResult = CloseLib(task)) != 0) {
                break;
            }
            task->mStatus = WC24_STATUS_DOWNLOAD;
            if ((task->mResult = task->execDownload(0, task->mMask, id)) == 0) {
                task->mStatus = WC24_STATUS_LIB;
                if ((task->mResult = OpenLib(task)) == 0) {
                    if ((task->mResult = CheckLib(task)) == 0) {
                        task->mStatus = WC24_STATUS_READ;
                        switch (task->mKind[0]) {
                        case 2:
                            task->mResult = task->readFiles();
                            break;
                        }
                    }
                    task->mStatus = WC24_STATUS_LIB;
                    result = CloseLib(task);
                    if (task->mResult == 0) {
                        task->mResult = result;
                    }
                }
            }
            task->mStatus = WC24_STATUS_IDLE;
            break;
        }
        case WC24_REQ_REGISTER: {
            task->mStatus = WC24_STATUS_LIB;
            if ((task->mResult = OpenLib(task)) == 0) {
                if ((task->mResult = CheckLib(task)) == 0) {
                    task->mStatus = WC24_STATUS_SETUP;
                    task->mResult = task->setupDlTasks(TRUE, FALSE, task->mForce, task->mInterval,
                                                       task->mCount, NULL);
                }
                task->mStatus = WC24_STATUS_LIB;
                s32 result = CloseLib(task);
                if (task->mResult == 0) {
                    task->mResult = result;
                }
            }
            task->mStatus = WC24_STATUS_IDLE;
            break;
        }
        case WC24_REQ_UNREGISTER: {
            task->mStatus = WC24_STATUS_LIB;
            if ((task->mResult = OpenLib(task)) == 0) {
                if ((task->mResult = CheckLib(task)) == 0) {
                    task->mStatus = WC24_STATUS_DELETE;
                    task->mResult = task->deleteDlTasks(TRUE, FALSE);
                }
                task->mStatus = WC24_STATUS_LIB;
                s32 result = CloseLib(task);
                if (task->mResult == 0) {
                    task->mResult = result;
                }
            }
            task->mStatus = WC24_STATUS_IDLE;
            break;
        }
        case WC24_REQ_QUIT:
            quit = true;
            break;
        }
    } while (!quit);
    return NULL;
}

s32 CWiiConnect24::readFiles() {
    NWC24DlTask dl;
    u16 id;
    NWC24Err err;

    err = NWC24GetMyDlTask(&dl);
    if (err == NWC24_ERR_NOT_FOUND) {
        SetError(this, "NWC24GetDlTaskMine() failed.", NWC24GetErrorCode(), err);
        return -1;
    }
    if (err != NWC24_OK) {
        SetError(this, "NWC24GetDlTaskMine() failed.", NWC24GetErrorCode(), err);
        return ConvertError(err);
    }
    err = NWC24GetDlTaskId(&dl, &id);
    if (err != NWC24_OK) {
        SetError(this, "NWC24GetDlId() failed.", NWC24GetErrorCode(), err);
        return ConvertError(err);
    }
    if (id == 2) {
        err = NWC24DeleteDlTask(&dl);
        if (err != NWC24_OK) {
            SetError(this, "NWC24DeleteDlTask() failed.", NWC24GetErrorCode(), err);
            return ConvertError(err);
        }
        SetError(this, "NWC24GetDlId() failed. TaskId: 2", 0, 0);
        return -1;
    }

    char url[0x100];
    err = NWC24GetDlUrl(&dl, url, 0xFF);
    if (err != NWC24_OK) {
        SetError(this, "NWC24GetDlUrl() failed.", NWC24GetErrorCode(), err);
        return ConvertError(err);
    }
    if (mUrl[0]) {
        for (u32 i = 0; i < 0xFF && url[i] != '\0'; i++) {
            if (url[i] != mUrl[0][i]) {
                SetError(this, "URL is not same.", 0, 0);
                return -1;
            }
        }
    }

    char path[0x50];
    err = NWC24GetDlVfPath(&dl, path, sizeof(path));
    if (err != NWC24_OK) {
        SetError(this, "NWC24GetDlVfName() failed.", NWC24GetErrorCode(), err);
        return ConvertError(err);
    }

    NANDFileInfo info;
    s32 result = NANDOpen(path, &info, NAND_ACCESS_READ);
    if (result != NAND_RESULT_OK) {
        SetError(this, "NANDOpen() failed.", 0, result);
        return -1;
    }
    u32 length;
    result = NANDGetLength(&info, &length);
    if (result != NAND_RESULT_OK) {
        SetError(this, "NANDGetLength() failed.", 0, result);
        NANDClose(&info);
        return -8;
    }
    if (length < 0x400) {
        SetError(this, "NANDGetLength size < 1024.", 0, 0);
        NANDClose(&info);
        return -1;
    }
    void* mem = MEMAllocFromExpHeapEx(mTmpHeap, length, 32);
    if (mem == NULL) {
        SetError(this, "TmpHeapHandle Memory Error.", 0, 0);
        NANDClose(&info);
        return -10;
    }
    result = VFCreateSystemFileRAM(mem, length);
    if (result != 0) {
        SetError(this, "VFCreateSystemFileRam() failed.", 0, result);
        NANDClose(&info);
        if (mem) {
            MEMFreeToExpHeap(mTmpHeap, mem);
        }
        return -7;
    }
    result = VFMountDriveRAM(WC24_DRIVE, mem);
    if (result != 0) {
        SetError(this, "VFMountDriveRam() failed.", 0, result);
        NANDClose(&info);
        if (mem) {
            MEMFreeToExpHeap(mTmpHeap, mem);
        }
        return -7;
    }
    result = VFSyncDrive(WC24_DRIVE, 1);
    if (result != 0) {
        SetError(this, "VFSync() failed.", 0, result);
        NANDClose(&info);
        VFUnmountDrive(WC24_DRIVE);
        if (mem) {
            MEMFreeToExpHeap(mTmpHeap, mem);
        }
        return -7;
    }
    result = NANDRead(&info, mem, length);
    if (result != length) {
        SetError(this, "NANDRead() failed.", 0, 0);
        NANDClose(&info);
        VFUnmountDrive(WC24_DRIVE);
        if (mem) {
            MEMFreeToExpHeap(mTmpHeap, mem);
        }
        return -8;
    }
    result = NANDClose(&info);
    if (result != NAND_RESULT_OK) {
        SetError(this, "NANDClose() failed.", 0, result);
        VFUnmountDrive(WC24_DRIVE);
        if (mem) {
            MEMFreeToExpHeap(mTmpHeap, mem);
        }
        return -8;
    }

    u8 dta[0x448];
    for (result = VFFindFirst(dta, WC24_DRIVE ":/*", 0x7F); result == 0; result = VFFindNext(dta)) {
    }

    for (u8 i = 0; i < WC24_NUM_FILES; i++) {
        err = NWC24GetDlSubTaskLastUpdate(&dl, i, mTimes[i]);
        if (err != NWC24_OK) {
            SetError(this, "NWC24GetDlLastUpdateSubTask() failed.", NWC24GetErrorCode(), err);
            VFUnmountDrive(WC24_DRIVE);
            if (mem) {
                MEMFreeToExpHeap(mTmpHeap, mem);
            }
            return ConvertError(err);
        }
    }

    OSCalendarTime cal;
    NETGetUniversalCalendar(&cal);
    u8 n;
    u8 hour = cal.hour;
    for (n = 0; n < WC24_NUM_FILES; n++) {
        char name[16];
        err = NWC24GetDlFilename(&dl, name, sizeof(name), hour);
        if (err != NWC24_OK) {
            SetError(this, "NWC24GetDlFilenameSubTask() failed.", NWC24GetErrorCode(), err);
            VFUnmountDrive(WC24_DRIVE);
            if (mem) {
                MEMFreeToExpHeap(mTmpHeap, mem);
            }
            return ConvertError(err);
        }
        VFFile file = VFOpenFile(name, "r", 0);
        if (file == NULL) {
            SetError(this, "VFOpenFile() failed.", 0, 0);
            result = VFUnmountDrive(WC24_DRIVE);
            if (mem) {
                MEMFreeToExpHeap(mTmpHeap, mem);
            }
            if (result != 0) {
                SetError(this, "VFUnmountDrive() failed.", 0, result);
                return -7;
            }
            return -1;
        }
        u32 size;
        s32 read = readLZ77FileEx(file, mHeap, mFiles[hour], &size);
        if (read != 0 && read != 1) {
            VFUnmountDrive(WC24_DRIVE);
            if (mem) {
                MEMFreeToExpHeap(mTmpHeap, mem);
            }
            return read;
        }
        *mSizes[hour] = size;
        result = VFCloseFile(file);
        if (result != 0) {
            SetError(this, "VFCloseFile() failed.", 0, result);
            VFUnmountDrive(WC24_DRIVE);
            if (mem) {
                MEMFreeToExpHeap(mTmpHeap, mem);
            }
            return -7;
        }
        if (n < WC24_NUM_FILES - 1 && read == 1) {
            for (; n < WC24_NUM_FILES; n++) {
                *mFiles[hour] = NULL;
                *mSizes[hour] = 0;
                if (hour == 0) {
                    hour = 23;
                } else {
                    hour--;
                }
            }
        }
        if (hour == 0) {
            hour = 23;
        } else {
            hour--;
        }
    }

    err = NWC24GetDlNextTime(&dl, &mNextTime);
    if (err != NWC24_OK) {
        SetError(this, "NWC24GetDlNextTime() failed.", NWC24GetErrorCode(), err);
        VFUnmountDrive(WC24_DRIVE);
        if (mem) {
            MEMFreeToExpHeap(mTmpHeap, mem);
        }
        return ConvertError(err);
    }
    result = VFUnmountDrive(WC24_DRIVE);
    if (mem) {
        MEMFreeToExpHeap(mTmpHeap, mem);
    }
    if (result != 0) {
        SetError(this, "VFUnmoundDrive() failed.", 0, result);
        return -7;
    }
    return 0;
}

s32 CWiiConnect24::setupDlTasks(BOOL first, BOOL second, u8 force, u16 interval, u16 count,
                                u16* idOut) {
    u16 id = 0xFFFF;
    u16 kind[2];
    const char* url[2];
    NWC24DlTask dl[2];
    u8 dta[0x448];
    NWC24Err err;
    s32 result;
    BOOL mounted = FALSE;
    BOOL add = force;
    BOOL recreate = force;

    if (first) {
        kind[0] = mKind[0];
        url[0] = mUrl[0];
    } else {
        kind[0] = 0;
        url[0] = NULL;
    }
    if (second) {
        kind[1] = mKind[1];
        url[1] = mUrl[1];
    } else {
        kind[1] = 0;
        url[1] = NULL;
    }

    for (s32 i = 0; i < 2; i++) {
        if (kind[i] == 0) {
            continue;
        }
        err = NWC24GetMyDlTask(&dl[i]);
        if (err == NWC24_ERR_NOT_FOUND) {
            add = TRUE;
            recreate = TRUE;
            break;
        }
        if (err != NWC24_OK) {
            SetError(this, "NWC24GetDlTask() failed.", NWC24GetErrorCode(), err);
            return ConvertError(err);
        }
        err = NWC24GetDlTaskId(&dl[i], &id);
        if (err != NWC24_OK) {
            SetError(this, "NWC24GetDlId() failed.", NWC24GetErrorCode(), err);
            return ConvertError(err);
        }
        if (id == 2) {
            add = TRUE;
            recreate = TRUE;
            break;
        }
        char path[0x50];
        err = NWC24GetDlVfPath(&dl[i], path, sizeof(path));
        if (err != NWC24_OK) {
            SetError(this, "NWC24GetDlVfName() failed.", NWC24GetErrorCode(), err);
            return ConvertError(err);
        }
        if (!mounted) {
            result = VFMountDriveNANDFlash(WC24_DRIVE, path);
            if (result == VF_ERROR_0002 || result == VF_ERROR_B001) {
                add = TRUE;
                recreate = TRUE;
                break;
            }
            if (result != 0) {
                SetError(this, "VFMountDriveNANDFlash() failed.", 0, result);
                return -7;
            }
            mounted = TRUE;
            for (result = VFFindFirst(dta, WC24_DRIVE ":/*", 0x7F); result == 0;
                 result = VFFindNext(dta)) {
            }
        }
        if (mounted) {
            switch (kind[i]) {
            case 2:
                for (u8 j = 0; j < WC24_NUM_FILES; j++) {
                    char name[16];
                    err = NWC24GetDlFilename(&dl[i], name, sizeof(name), j);
                    if (err != NWC24_OK) {
                        SetError(this, "NWC24GetDlFilenameSubTask() failed.", NWC24GetErrorCode(), err);
                        VFUnmountDrive(WC24_DRIVE);
                        return -7;
                    }
                    VFFile file = VFOpenFile(name, "r", 0);
                    if (file == NULL) {
                        break;
                    }
                    result = VFCloseFile(file);
                    if (result != 0) {
                        SetError(this, "VFCloseFile() failed.", 0, result);
                        VFUnmountDrive(WC24_DRIVE);
                        return -7;
                    }
                }
                break;
            }
        }
    }

    if (mounted) {
        result = VFUnmountDrive(WC24_DRIVE);
        if (result != 0) {
            SetError(this, "VFUnmountDrive() failed.", 0, result);
            return -7;
        }
    }

    if (!add) {
        for (s32 i = 0; i < 2; i++) {
            if (kind[i] == 0) {
                continue;
            }
            char buf[0x100];
            err = NWC24GetDlUrl(&dl[i], buf, 0xFF);
            if (err != NWC24_OK) {
                SetError(this, "NWC24GetDlUrl() failed.", NWC24GetErrorCode(), err);
                return ConvertError(err);
            }
            for (u32 j = 0; j < 0xFF && buf[j] != '\0'; j++) {
                if (buf[j] != url[i][j]) {
                    add = TRUE;
                    recreate = TRUE;
                    break;
                }
            }
            u16 oldInterval;
            err = NWC24GetDlInterval(&dl[i], &oldInterval);
            if (err != NWC24_OK) {
                SetError(this, "NWC24GetDlInterval() failed.", NWC24GetErrorCode(), err);
                return ConvertError(err);
            }
            if (interval != oldInterval) {
                add = TRUE;
            }
        }
    }

    if (add) {
        BOOL vfCreated = FALSE;
        for (s32 i = 0; i < 2; i++) {
            if (kind[i] == 0) {
                continue;
            }
            if (recreate) {
                if (id != 0xFFFF) {
                    err = NWC24DeleteDlTask(&dl[i]);
                    if (err != NWC24_OK) {
                        SetError(this, "NWC24DeleteDlTask() failed.", NWC24GetErrorCode(), err);
                        return ConvertError(err);
                    }
                }
                err = NWC24InitDlTask(&dl[i], NWC24_DLTYPE_OCTETSTREAM_V1);
                if (err != NWC24_OK) {
                    SetError(this, "NWC24InitDlTask() failed.", NWC24GetErrorCode(), err);
                    return ConvertError(err);
                }
                char path[0x50];
                err = NWC24GetDlVfPath(&dl[i], path, sizeof(path));
                if (err != NWC24_OK) {
                    SetError(this, "NWC24GetDlVfName() failed.", NWC24GetErrorCode(), err);
                    return ConvertError(err);
                }
                if (!vfCreated) {
                    result = NANDDelete(path);
                    if (result != NAND_RESULT_NOEXISTS && result != NAND_RESULT_OK) {
                        SetError(this, "NANDDelete() failed.", 0, result);
                        return -8;
                    }
                    err = NWC24CreateDlVf(&dl[i], mVfSize);
                    if (err != NWC24_OK) {
                        SetError(this, "NWC24CreateDlVf() failed.", NWC24GetErrorCode(), err);
                        return ConvertError(err);
                    }
                    vfCreated = TRUE;
                }
            }
            err = NWC24SetDlUrl(&dl[i], url[i]);
            if (err != NWC24_OK) {
                SetError(this, "NWC24SetDlUrl() failed.", NWC24GetErrorCode(), err);
                return ConvertError(err);
            }
            switch (kind[i]) {
            case 2:
                // The original takes the task through a pointer to const here
                // (it shares the induction variable of the getters).
                const NWC24DlTask* task = &dl[i];
                err = NWC24SetDlServerInterval((NWC24DlTask*)task, 1440);
                if (err != NWC24_OK) {
                    SetError(this, "NWC24SetDlServerInterval() failed.", NWC24GetErrorCode(), err);
                    return ConvertError(err);
                }
                err = NWC24SetDlSubTask(&dl[i], NWC24_DL_STTYPE_TIME_HOUR, 0xFFFFFF, 0x103);
                if (err != NWC24_OK) {
                    SetError(this, "NWC24SetDlSubtaskParameter() failed.", NWC24GetErrorCode(), err);
                    return ConvertError(err);
                }
                break;
            }
            err = NWC24SetDlPriority(&dl[i], 100);
            if (err != NWC24_OK) {
                SetError(this, "NWC24SetDlPriority() failed.", NWC24GetErrorCode(), err);
                return ConvertError(err);
            }
            err = NWC24SetDlOption(&dl[i], 0x40000000);
            if (err != NWC24_OK) {
                SetError(this, "NWC24SetDlFlags() failed.", NWC24GetErrorCode(), err);
                return ConvertError(err);
            }
            err = NWC24SetDlInterval(&dl[i], interval);
            if (err != NWC24_OK) {
                SetError(this, "NWC24SetDlInterval() failed.", NWC24GetErrorCode(), err);
                return ConvertError(err);
            }
            err = NWC24SetDlMargin(&dl[i], 720);
            if (err != NWC24_OK) {
                SetError(this, "NWC24SetDlRetryMargin() failed.", NWC24GetErrorCode(), err);
                return ConvertError(err);
            }
            char name[16];
            sprintf(name, "%d.bin", kind[i]);
            err = NWC24SetDlFilename(&dl[i], name);
            if (err != NWC24_OK) {
                SetError(this, "NWC24SetDlFilename() failed.", NWC24GetErrorCode(), err);
                return ConvertError(err);
            }
        }
    }

    for (s32 i = 0; i < 2; i++) {
        if (kind[i] == 0) {
            continue;
        }
        err = NWC24SetDlCount(&dl[i], count);
        if (err != NWC24_OK) {
            SetError(this, "NWC24SetDlCount() failed.", NWC24GetErrorCode(), err);
            return ConvertError(err);
        }
        if (add) {
            err = NWC24AddDlTask(&dl[i]);
            if (err != NWC24_OK) {
                SetError(this, "NWC24AddDlTask() failed.", NWC24GetErrorCode(), err);
                return ConvertError(err);
            }
        } else {
            err = NWC24UpdateDlTask(&dl[i]);
            if (err != NWC24_OK) {
                SetError(this, "NWC24UpdateDlTask() failed.", NWC24GetErrorCode(), err);
                return ConvertError(err);
            }
        }
        if (idOut) {
            err = NWC24GetDlTaskId(&dl[i], idOut);
            if (err != NWC24_OK) {
                SetError(this, "NWC24GetDlId() failed.", NWC24GetErrorCode(), err);
                return ConvertError(err);
            }
        }
        NWC24CheckDlTask(&dl[i]);
    }
    sTasksReady = true;
    return 0;
}

s32 CWiiConnect24::execDownload(s32 index, u32 mask, u16 id) {
    NWC24Err err;
    s32 result;
    u16 kind = mKind[index];
    if (!sSOReady) {
        sSOStarting = true;
        SOLibraryConfig config;
        config.alloc = SOAllocFunc;
        config.free = SOFreeFunc;
        result = SOInit(&config);
        if (result < 0) {
            SetError(this, "SOInit() failed.", 0, result);
            sSOStarting = false;
            return -9;
        }
        result = SOStartup();
        if (result < 0) {
            SetError(this, "SOStartup() failed.", NETGetStartupErrorCode(result), result);
            sSOStarting = false;
            return -9;
        }
        sSOReady = true;
        sSOStarting = false;
    }
    switch (kind) {
    case 2:
        sDownloading = true;
        err = NWC24ExecDownloadTask(6, id, mask);
        sDownloading = false;
        if (err != NWC24_OK) {
            SetError(this, "NWC24ExecDownloadTask() failed.", NWC24GetErrorCode(), err);
            return ConvertError(err);
        }
        break;
    }
    return 0;
}

s32 CWiiConnect24::deleteDlTasks(BOOL first, BOOL second) {
    BOOL gotPath = FALSE;
    u16 kind[2];
    char path[0x50];
    NWC24DlTask dl;
    NWC24Err err;

    kind[0] = first ? mKind[0] : 0;
    if (second) {
        kind[1] = mKind[1];
    } else {
        kind[1] = 0;
    }
    for (s32 i = 0; i < 2; i++) {
        if (kind[i] == 0) {
            continue;
        }
        err = NWC24GetMyDlTask(&dl);
        if (err == NWC24_ERR_NOT_FOUND) {
            continue;
        }
        if (err != NWC24_OK) {
            SetError(this, "NWC24GetDl*() failed.", NWC24GetErrorCode(), err);
            return ConvertError(err);
        }
        if (!gotPath) {
            gotPath = TRUE;
            err = NWC24GetDlVfPath(&dl, path, sizeof(path));
            if (err != NWC24_OK) {
                SetError(this, "NWC24GetDlVfName() failed.", NWC24GetErrorCode(), err);
                return ConvertError(err);
            }
        }
        err = NWC24DeleteDlTask(&dl);
        if (err != NWC24_OK) {
            SetError(this, "NWC24DeleteDlTask() failed.", NWC24GetErrorCode(), err);
            return ConvertError(err);
        }
    }
    if (gotPath) {
        s32 result = NANDDelete(path);
        if (result != NAND_RESULT_NOEXISTS && result != NAND_RESULT_OK) {
            SetError(this, "NANDDelete() failed.", 0, result);
            return -8;
        }
    }
    return 0;
}

static s32 ConvertError(NWC24Err err) {
    s32 result = -6;
    if ((err <= -16 && err >= -21) || err == -38 || err == -41 || err == -43 || err == -46) {
        result = -11;
    } else if (err == -49) {
        result = -12;
    } else if (err == -1 || err == -11 || err == -42) {
        result = -2;
    } else if (NWC24GetErrorCode() == -109106) {
        result = -5;
    } else {
        switch (err) {
        case -39:
            result = -3;
            break;
        case -31:
            result = -4;
            break;
        case -32:
            result = -5;
            break;
        }
    }
    return result;
}

static inline BOOL IsUncompUnfinished(const CXUncompContextLZ* ctx) {
    return ctx->destCount > 0 || ctx->headerSize != 0;
}

s32 CWiiConnect24::readLZ77FileEx(VFFile file, MEMHeapHandle heap, void** dst, u32* size) {
    CXUncompContextLZ ctx;
    u32 fileSize = VFGetFileSizeByFd(file);
    u32 outSize = 0;
    u32 avail = MEMGetAllocatableSizeForExpHeapEx(heap, 4);
    for (u32 ofs = 0; ofs < fileSize; ofs += sizeof(gWC24ReadBuf)) {
        u32 len = fileSize - ofs;
        if (len > sizeof(gWC24ReadBuf)) {
            len = sizeof(gWC24ReadBuf);
        }
        s32 result = VFReadFile(file, gWC24ReadBuf, len, NULL);
        if (result != 0) {
            SetError(this, "VFReadFile() failed.", 0, result);
            return -7;
        }
        if (ofs == 0) {
            if ((gWC24ReadBuf[0] & 0xF0) != 0x10) {
                SetError(this, "CXGetCompressionType() data is not LZ.", 0, 0);
                return -5;
            }
            outSize = CXGetUncompressedSize(gWC24ReadBuf);
            if (outSize > avail) {
                SetError(this, "CXGetUncompressedSize() overflow.", 0, 0);
                *dst = NULL;
                *size = 0;
                return 1;
            }
            if (*dst) {
#line 3040
                OSPanic(__FILE__, __LINE__, "CWiiConnect24::readLZ77FileEx() dst is not NULL.\n");
            }
            *dst = MEMAllocFromExpHeapEx(heap, outSize, 4);
            CXInitUncompContextLZ(&ctx, *dst);
        }
        CXReadUncompLZ(&ctx, gWC24ReadBuf, len);
    }
    if (IsUncompUnfinished(&ctx)) {
        SetError(this, "CXIsFinisiedUncompLZ() is false.", 0, 0);
        return -10;
    }
    *size = outSize;
    return 0;
}

static void* SOAllocFunc(u32 name, s32 size) {
    void* ptr = NULL;
    if (size > 0) {
        ptr = MEMAllocFromExpHeapEx(sSOHeap, size, 32);
    }
    return ptr;
}

static void SOFreeFunc(u32 name, void* ptr, s32 size) {
    if (ptr != NULL && size > 0) {
        MEMFreeToExpHeap(sSOHeap, ptr);
    }
}

u8 gWC24Work[0x4000] ATTRIBUTE_ALIGN(32);
OSMessageQueue gWC24Queue;
OSMessage gWC24Messages[16];
OSThread gWC24Thread;
u8 gWC24Stack[0x8000];
u8 gWC24ReadBuf[0x10000];
CWiiConnect24 gWC24Tasks[8];
