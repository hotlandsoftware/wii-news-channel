// NWC24, SO, NET and VF placeholder: a console that has never been online.
//
// TODO(milestone 5): replace this file. NWC24 download tasks become HTTP
// requests (libcurl), the task's VF archive becomes host files, and SO/NET
// report a working connection. Every function is weak (PC_NOOP), so real
// definitions take over one by one.
//
// The placeholder answers like a Wii whose WiiConnect24 library works but
// which has no network connection, so the game takes the path it already has
// for that case and nothing is ever downloaded:
//
// - NWC24: the library opens and passes its checks. Download tasks can be
//   created, changed, registered and deleted, but the registry lives in memory
//   only (one task: this title's) and the scheduler never runs.
//   NWC24ExecDownloadTask() fails with NWC24_ERR_NETWORK.
// - SO: SOInit() succeeds; SOStartup() fails with SO_ERR_LINK_UP_TIMEOUT (no
//   link), which is what the game hits first when it tries to download.
// - NET: the error-code and CRC functions are the SDK's own, compiled
//   natively (pc/ported/sdk_net.txt). NETGetUniversalCalendar() is the host's
//   UTC clock.
// - VF (the FAT archive library that holds downloaded files): no drive can be
//   mounted and no file exists.

#include <revolution/nwc24.h>
#include <revolution/nwc24/internal/NWC24iSchedule.h>
#include <revolution/nwc24/internal/NWC24iSystem.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/time.h>

#include <revolution/ncd.h>
#include <revolution/net.h>
#include <revolution/os.h>
#include <revolution/so.h>
#include <revolution/vf.h>

#include "pc_noop.h"

namespace {

// The private contents of an NWC24DlTask (the SDK's NWC24iDlTask has the same
// role; the layout here is the placeholder's own).
struct DlTask {
    u32 magic;
    u16 id; // 0xFFFF until the task is registered
    u8 type;
    u8 priority;
    u16 interval;
    u16 margin;
    u32 serverInterval;
    u32 options;
    s16 count;
    u16 subTaskFlags;
    u32 subTaskType;
    u32 subTaskMask;
    char url[236];
    char fileName[64];
};
static_assert(sizeof(DlTask) <= sizeof(NWC24DlTask), "DlTask fits in NWC24DlTask");

const u32 kTaskMagic = 0x57634454; // 'WcDT'
const u16 kMyTaskId = 0;

// Where the title's download archive would be on the NAND.
const char kVfPath[] = "/title/00010002/48414745/data/wc24dl.vff";

bool sOpened;
s32 sErrorCode;
bool sRegistered;
DlTask sRegisteredTask;

DlTask* Impl(NWC24DlTask* task) {
    return reinterpret_cast<DlTask*>(task);
}
const DlTask* Impl(const NWC24DlTask* task) {
    return reinterpret_cast<const DlTask*>(task);
}

NWC24Err CheckTask(const NWC24DlTask* task) {
    if (task == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }
    if (!sOpened) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }
    if (Impl(task)->magic != kTaskMagic) {
        return NWC24_ERR_INVALID_VALUE;
    }
    return NWC24_OK;
}

const s32 kVfError = -1; // any VF failure

} // namespace

extern "C" {

// --- NWC24: library -----------------------------------------------------------

PC_NOOP NWC24Err NWC24OpenLib(void* work) {
    sErrorCode = 0;
    if (work == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }
    if (sOpened) {
        return NWC24_ERR_LIB_OPENED;
    }
    sOpened = true;
    return NWC24_OK;
}

PC_NOOP NWC24Err NWC24CloseLib(void) {
    if (!sOpened) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }
    sOpened = false;
    return NWC24_OK;
}

// Checks that WiiConnect24 can be used for `usage` (settings, parental
// controls, scheduler errors). Nothing objects here; the game checks the
// standby setting itself with SCGetIdleMode().
PC_NOOP NWC24Err NWC24Check(u32 usage) {
    sErrorCode = 0;
    return sOpened ? NWC24_OK : NWC24_ERR_LIB_NOT_OPENED;
}

PC_NOOP s32 NWC24GetErrorCode(void) {
    return sErrorCode;
}

// Asks the scheduler to stop what it is doing before a shutdown. Nothing runs.
PC_NOOP NWC24Err NWC24iRequestShutdownSync(u32 event) {
    return NWC24_OK;
}

// --- NWC24: download tasks -----------------------------------------------------

PC_NOOP NWC24Err NWC24InitDlTask(NWC24DlTask* task, NWC24DlType type) {
    if (!sOpened) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }
    if (task == NULL || type >= 4) {
        return NWC24_ERR_INVALID_VALUE;
    }
    std::memset(task, 0, sizeof(*task));
    DlTask* impl = Impl(task);
    impl->magic = kTaskMagic;
    impl->type = static_cast<u8>(type);
    impl->priority = 0x7F;
    impl->id = 0xFFFF;
    impl->count = 1;
    impl->interval = 2880;
    impl->margin = 1440;
    if (type == NWC24_DLTYPE_OCTETSTREAM_V1 || type == NWC24_DLTYPE_OCTETSTREAM_V2) {
        std::strcpy(impl->fileName, "content.bin");
    }
    return NWC24_OK;
}

PC_NOOP NWC24Err NWC24CheckDlTask(const NWC24DlTask* task) {
    return CheckTask(task);
}

// The task this title registered, if any.
PC_NOOP NWC24Err NWC24GetMyDlTask(NWC24DlTask* task) {
    if (task == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }
    if (!sOpened) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }
    if (!sRegistered) {
        return NWC24_ERR_NOT_FOUND;
    }
    std::memset(task, 0, sizeof(*task));
    *Impl(task) = sRegisteredTask;
    return NWC24_OK;
}

PC_NOOP NWC24Err NWC24AddDlTask(NWC24DlTask* task) {
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    Impl(task)->id = kMyTaskId;
    sRegisteredTask = *Impl(task);
    sRegistered = true;
    return NWC24_OK;
}

PC_NOOP NWC24Err NWC24UpdateDlTask(NWC24DlTask* task) {
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    if (!sRegistered || Impl(task)->id != sRegisteredTask.id) {
        return NWC24_ERR_NOT_FOUND;
    }
    sRegisteredTask = *Impl(task);
    return NWC24_OK;
}

PC_NOOP NWC24Err NWC24DeleteDlTask(NWC24DlTask* task) {
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    if (!sRegistered || Impl(task)->id != sRegisteredTask.id) {
        return NWC24_ERR_NOT_FOUND;
    }
    sRegistered = false;
    Impl(task)->id = 0xFFFF;
    return NWC24_OK;
}

PC_NOOP NWC24Err NWC24GetDlTaskId(const NWC24DlTask* task, u16* id) {
    NWC24Err result = CheckTask(task);
    if (result == NWC24_OK) {
        *id = Impl(task)->id;
    }
    return result;
}

PC_NOOP NWC24Err NWC24SetDlPriority(NWC24DlTask* task, u8 priority) {
    NWC24Err result = CheckTask(task);
    if (result == NWC24_OK) {
        Impl(task)->priority = priority;
    }
    return result;
}

PC_NOOP NWC24Err NWC24SetDlInterval(NWC24DlTask* task, u16 interval) {
    NWC24Err result = CheckTask(task);
    if (result == NWC24_OK) {
        Impl(task)->interval = interval;
    }
    return result;
}

PC_NOOP NWC24Err NWC24GetDlInterval(const NWC24DlTask* task, u16* interval) {
    NWC24Err result = CheckTask(task);
    if (result == NWC24_OK) {
        *interval = Impl(task)->interval;
    }
    return result;
}

PC_NOOP NWC24Err NWC24SetDlServerInterval(NWC24DlTask* task, u32 interval) {
    NWC24Err result = CheckTask(task);
    if (result == NWC24_OK) {
        Impl(task)->serverInterval = interval;
    }
    return result;
}

PC_NOOP NWC24Err NWC24SetDlMargin(NWC24DlTask* task, u16 margin) {
    NWC24Err result = CheckTask(task);
    if (result == NWC24_OK) {
        Impl(task)->margin = margin;
    }
    return result;
}

PC_NOOP NWC24Err NWC24SetDlOption(NWC24DlTask* task, u32 flags) {
    NWC24Err result = CheckTask(task);
    if (result == NWC24_OK) {
        Impl(task)->options = flags;
    }
    return result;
}

PC_NOOP NWC24Err NWC24SetDlCount(NWC24DlTask* task, s16 count) {
    NWC24Err result = CheckTask(task);
    if (result == NWC24_OK) {
        Impl(task)->count = count;
    }
    return result;
}

PC_NOOP NWC24Err NWC24SetDlSubTask(NWC24DlTask* task, NWC24DlSubTaskType type, u32 mask, u16 flags) {
    NWC24Err result = CheckTask(task);
    if (result == NWC24_OK) {
        Impl(task)->subTaskType = type;
        Impl(task)->subTaskMask = mask;
        Impl(task)->subTaskFlags = flags;
    }
    return result;
}

PC_NOOP NWC24Err NWC24SetDlUrl(NWC24DlTask* task, const char* url) {
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    if (url == NULL || std::strlen(url) >= sizeof(Impl(task)->url)) {
        return NWC24_ERR_INVALID_VALUE;
    }
    std::strcpy(Impl(task)->url, url);
    return NWC24_OK;
}

PC_NOOP NWC24Err NWC24GetDlUrl(const NWC24DlTask* task, char* buf, u32 size) {
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    if (std::strlen(Impl(task)->url) + 1 > size) {
        return NWC24_ERR_NOMEM;
    }
    std::strcpy(buf, Impl(task)->url);
    return NWC24_OK;
}

PC_NOOP NWC24Err NWC24SetDlFilename(NWC24DlTask* task, const char* fileName) {
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    if (fileName == NULL || std::strlen(fileName) >= sizeof(Impl(task)->fileName)) {
        return NWC24_ERR_INVALID_VALUE;
    }
    std::strcpy(Impl(task)->fileName, fileName);
    return NWC24_OK;
}

// File name of sub-task `index`: with NWC24_DL_STFLAG_TRAILING_FILENAME the
// index is appended as ".NN", as in the SDK.
PC_NOOP NWC24Err NWC24GetDlFilename(const NWC24DlTask* task, char* buf, u32 size, u8 index) {
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    const DlTask* impl = Impl(task);
    bool trailing = (impl->subTaskFlags & NWC24_DL_STFLAG_TRAILING_FILENAME) != 0;
    u32 length = static_cast<u32>(std::strlen(impl->fileName));
    if (length == 0) {
        return NWC24_ERR_NOT_FOUND;
    }
    if (length + (trailing ? 3 : 0) + 1 > size) {
        return NWC24_ERR_NOMEM;
    }
    std::strcpy(buf, impl->fileName);
    if (trailing) {
        std::snprintf(buf + length, size - length, ".%02d", index);
    }
    return NWC24_OK;
}

// Nothing was ever downloaded: time 0.
PC_NOOP NWC24Err NWC24GetDlSubTaskLastUpdate(const NWC24DlTask* task, u8 index, s64* time) {
    NWC24Err result = CheckTask(task);
    if (result == NWC24_OK) {
        *time = 0;
    }
    return result;
}

PC_NOOP NWC24Err NWC24GetDlNextTime(const NWC24DlTask* task, s64* time) {
    NWC24Err result = CheckTask(task);
    if (result == NWC24_OK) {
        *time = 0;
    }
    return result;
}

PC_NOOP NWC24Err NWC24GetDlVfPath(const NWC24DlTask* task, char* buf, u32 size) {
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    if (sizeof(kVfPath) > size) {
        return NWC24_ERR_NOMEM;
    }
    std::strcpy(buf, kVfPath);
    return NWC24_OK;
}

// Creates the archive file the downloads go into. Nothing is written: with no
// file, the game finds "no data" when it looks for the archive.
PC_NOOP NWC24Err NWC24CreateDlVf(const NWC24DlTask* task, u32 size) {
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    if (size < 0x2800) {
        return NWC24_ERR_INVALID_VALUE;
    }
    return NWC24_OK;
}

// Runs a download now. There is no network.
PC_NOOP NWC24Err NWC24ExecDownloadTask(u32 arg0, u32 arg1, u32 arg2) {
    sErrorCode = -51099; // the code NETGetStartupErrorCode() gives for "no link"
    return NWC24_ERR_NETWORK;
}

// --- SO / NCD / NET ------------------------------------------------------------

PC_NOOP int SOInit(const SOLibraryConfig* config) {
    return SO_SUCCESS;
}

// Brings the network interface up. There is none.
PC_NOOP int SOStartup(void) {
    return SO_ERR_LINK_UP_TIMEOUT;
}

// The connection profiles of the Wii's Internet settings: none configured.
// (Used by the SDK's NETGetStartupErrorCode().)
PC_NOOP s32 NCDiGetEnabledConfigList(u32* enabled, u32* wireless, u32* wired) {
    *enabled = 0;
    *wireless = 0;
    *wired = 0;
    return 0;
}

// Universal time. On the console this is the RTC plus the offset WiiConnect24
// learnt from the server; here it is the host's clock. This one is real, not a
// placeholder.
BOOL NETGetUniversalCalendar(OSCalendarTime* calendar) {
    struct timeval now;
    struct tm utc;
    gettimeofday(&now, nullptr);
    time_t seconds = now.tv_sec;
    gmtime_r(&seconds, &utc);

    calendar->sec = utc.tm_sec;
    calendar->min = utc.tm_min;
    calendar->hour = utc.tm_hour;
    calendar->mday = utc.tm_mday;
    calendar->mon = utc.tm_mon;
    calendar->year = utc.tm_year + 1900;
    calendar->wday = utc.tm_wday;
    calendar->yday = utc.tm_yday;
    calendar->msec = static_cast<int>(now.tv_usec / 1000);
    calendar->usec = static_cast<int>(now.tv_usec % 1000);
    return TRUE;
}

// --- VF --------------------------------------------------------------------------

PC_NOOP void VFInit(void) {}

PC_NOOP s32 VFCreateSystemFileRAM(void* memory, u32 size) {
    return kVfError;
}

PC_NOOP s32 VFMountDriveRAM(const char* drive, void* memory) {
    return kVfError;
}

PC_NOOP s32 VFMountDriveNANDFlash(const char* drive, const char* systemFileName) {
    return kVfError;
}

PC_NOOP s32 VFUnmountDrive(const char* drive) {
    return 0;
}

PC_NOOP s32 VFSyncDrive(const char* drive, u32 mode) {
    return kVfError;
}

PC_NOOP void* VFOpenFile(const char* path, const char* mode, u32 attr) {
    return NULL;
}

PC_NOOP s32 VFCloseFile(void* file) {
    return kVfError;
}

PC_NOOP s32 VFReadFile(void* file, void* buf, u32 size, u32* readSize) {
    if (readSize != NULL) {
        *readSize = 0;
    }
    return kVfError;
}

PC_NOOP s32 VFGetFileSizeByFd(void* file) {
    return 0;
}

PC_NOOP s32 VFFindFirst(void* dta, const char* path, u32 attr) {
    return kVfError; // no entry
}

PC_NOOP s32 VFFindNext(void* dta) {
    return kVfError;
}

} // extern "C"
