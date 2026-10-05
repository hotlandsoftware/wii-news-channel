// NWC24: the WiiConnect24 library, as far as the News Channel uses it -- the
// library itself, this title's download task and the downloader.
// (docs/pc_port.md, "How the channel gets its news".)
//
// On the Wii the library is a client of the system's WiiConnect24 daemon
// (/dev/net/kd/request): the task list is the daemon's file
// /shared2/wc24/nwc24dl.bin, and downloads are done by the daemon, in the
// background or when a title asks with NWC24ExecDownloadTask(). Here library
// and daemon are this file:
//
// - The task list holds one task, this title's. Its fields are the SDK's own
//   NWC24iDlTask; the checks and defaults follow src/revolution/NWC24. It is
//   kept in /shared2/wc24/nwc24dl.bin of the PC NAND directory (a PC format,
//   not the console's), so a second start finds the task and its archive.
// - NWC24ExecDownloadTask() is the downloader. For every sub-task asked for it
//   gets "<url>.<NN>" from the news source (src/pc/news/pc_news.h: a directory
//   today, HTTP later), removes the WiiConnect24 wrapper and stores the payload
//   as "<file name>.<NN>" in the task's archive (wc24dl.vff, see vf.cpp).
//   The signature in the wrapper is NOT verified.
// - There is no background scheduler: nothing is downloaded unless the game
//   asks. The game asks whenever what it finds in the archive is missing or
//   out of date.

#include <revolution/nwc24.h>
#include <revolution/nwc24/internal/NWC24iDownload.h>
#include <revolution/nwc24/internal/NWC24iSchedule.h>
#include <revolution/nwc24/internal/NWC24iSystem.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <pthread.h>
#include <sys/stat.h>

#include <pc/files.h>
#include <pc/os.h>
#include <revolution/nand.h>
#include <revolution/os.h>
#include <revolution/vf.h>

#include "../news/pc_news.h"

namespace {

static_assert(sizeof(NWC24iDlTask) == sizeof(NWC24DlTask), "NWC24iDlTask is the contents of NWC24DlTask");

// This title: the News Channel, 00010002-HAGE.
const u32 kTitleIdHi = 0x00010002;
const u32 kTitleIdLo = 0x48414745;
// The id the daemon gives the task. Ids 0 and 1 belong to the system; the game
// treats 2 as a left-over of an old version and deletes it.
const u16 kMyTaskId = 3;
const u16 kNoTaskId = 0xFFFF;

const char kTaskListPath[] = "/shared2/wc24/nwc24dl.bin";
const char kVfFile[] = "wc24dl.vff";
// The drive name the downloader mounts the archive under (the game uses "@24").
const char kDownloadDrive[] = "@dl";

// Seconds from 1970-01-01 to 2000-01-01, the epoch of WiiConnect24's clock.
const s64 kUnixToNWC24Epoch = 946684800LL;

// Error codes (NWC24GetErrorCode(), shown as "Error Code: NNNNNN").
const s32 kErrorNoLink = -51099;      // what NETGetStartupErrorCode() gives for "no link"
const s32 kErrorHttpBase = -117000;   // -(117000 + HTTP status): the server refused the file
const s32 kErrorBadFile = -117900;    // PC: the served file has no usable wrapper

// The task list file: PC format.
struct TaskList {
    u32 magic;
    u32 version;
    u32 registered;
    s32 nextTime;   // minutes, as in the console's entry header
    s32 lastAccess; // minutes
    NWC24iDlTask task;
};

const u32 kListMagic = 0x4C444350; // "PCDL"
const u32 kListVersion = 1;

pthread_mutex_t sMutex = PTHREAD_MUTEX_INITIALIZER;
bool sOpened;
s32 sErrorCode;
bool sListLoaded;
TaskList sList;
volatile bool sShutdownRequested;

struct Lock {
    Lock() { pthread_mutex_lock(&sMutex); }
    ~Lock() { pthread_mutex_unlock(&sMutex); }
};

NWC24iDlTask* Impl(NWC24DlTask* task) {
    return reinterpret_cast<NWC24iDlTask*>(task);
}
const NWC24iDlTask* Impl(const NWC24DlTask* task) {
    return reinterpret_cast<const NWC24iDlTask*>(task);
}

// Universal time in seconds since 2000-01-01 (NWC24iGetUniversalTime()).
s64 UniversalTime() {
    return PCOSGetUnixTime(nullptr) - kUnixToNWC24Epoch;
}

bool IsOctetStream(u8 type) {
    return type == NWC24_DLTYPE_OCTETSTREAM_V1 || type == NWC24_DLTYPE_OCTETSTREAM_V2;
}

// sMutex held. The list survives NWC24CloseLib(): it is the daemon's.
void LoadList() {
    if (sListLoaded) {
        return;
    }
    sListLoaded = true;
    std::memset(&sList, 0, sizeof(sList));
    char path[1200];
    if (!PCNandHostPath(kTaskListPath, path, sizeof(path))) {
        return;
    }
    FILE* f = std::fopen(path, "rb");
    if (f == nullptr) {
        return;
    }
    TaskList list;
    if (std::fread(&list, sizeof(list), 1, f) == 1 && list.magic == kListMagic && list.version == kListVersion) {
        list.task.url[sizeof(list.task.url) - 1] = '\0';
        list.task.fileName[sizeof(list.task.fileName) - 1] = '\0';
        sList = list;
    }
    std::fclose(f);
}

// sMutex held.
void SaveList() {
    char path[1200];
    if (!PCNandHostPath(kTaskListPath, path, sizeof(path))) {
        return;
    }
    // /shared2 exists (NANDInit); /shared2/wc24 is the daemon's.
    if (char* slash = std::strrchr(path, '/')) {
        *slash = '\0';
        mkdir(path, 0777);
        *slash = '/';
    }
    sList.magic = kListMagic;
    sList.version = kListVersion;
    FILE* f = std::fopen(path, "wb");
    if (f == nullptr || std::fwrite(&sList, sizeof(sList), 1, f) != 1) {
        std::fprintf(stderr, "NWC24: cannot write %s: %s\n", path, std::strerror(errno));
    }
    if (f != nullptr) {
        std::fclose(f);
    }
}

// CheckDlTask() of the SDK: the library is open and the task is the caller's.
NWC24Err CheckTask(const NWC24DlTask* task) {
    if (!sOpened) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }
    if (task == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }
    if (Impl(task)->appId != kTitleIdLo) {
        return NWC24_ERR_PROTECTED;
    }
    return NWC24_OK;
}

void VfPath(char* buf, u32 size) {
    std::snprintf(buf, size, "/title/%08x/%08x/data/%s", kTitleIdHi, kTitleIdLo, kVfFile);
}

// The downloader, for one sub-task: get the file, unwrap it, store it.
NWC24Err DownloadOne(const PCNewsSource* source, const NWC24iDlTask* task, bool subTasks, u8 index) {
    char url[300];
    char name[NAND_MAX_PATH + 24];
    if (subTasks && (task->subTaskFlags & NWC24_DL_STFLAG_TRAILING_URL)) {
        std::snprintf(url, sizeof(url), "%s.%02d", task->url, index);
    } else {
        std::snprintf(url, sizeof(url), "%s", task->url);
    }
    if (subTasks && (task->subTaskFlags & NWC24_DL_STFLAG_TRAILING_FILENAME)) {
        std::snprintf(name, sizeof(name), "%s:/%s.%02d", kDownloadDrive, task->fileName, index);
    } else {
        std::snprintf(name, sizeof(name), "%s:/%s", kDownloadDrive, task->fileName);
    }

    u8* data = nullptr;
    u32 size = 0;
    int status = source->get(url, &data, &size);
    if (status == PC_NEWS_STATUS_UNREACHABLE) {
        sErrorCode = kErrorNoLink;
        return NWC24_ERR_NETWORK;
    }
    if (status != PC_NEWS_STATUS_OK) {
        sErrorCode = kErrorHttpBase - status;
        return NWC24_ERR_SERVER;
    }

    const u8* payload;
    u32 payloadSize;
    NWC24Err result = NWC24_OK;
    if (!PCNewsUnwrap(data, size, &payload, &payloadSize)) {
        std::fprintf(stderr, "NWC24: %s is not a signed WiiConnect24 file (%u bytes%s)\n", url, size,
                     size >= 4 && std::memcmp(data, "WC24", 4) == 0 ? ", encrypted" : "");
        sErrorCode = kErrorBadFile;
        result = NWC24_ERR_VERIFY_SIGNATURE;
    } else {
        void* file = VFOpenFile(name, "w", 0);
        if (file == NULL) {
            result = NWC24_ERR_INTERNAL_VF;
        } else {
            s32 written = VFWriteFile(file, const_cast<u8*>(payload), payloadSize);
            s32 closed = VFCloseFile(file);
            if (written != 0 || closed != 0) {
                std::fprintf(stderr, "NWC24: no room for %s (%u bytes) in the download archive\n", name, payloadSize);
                result = NWC24_ERR_INTERNAL_VF;
            }
        }
    }
    std::free(data);
    return result;
}

} // namespace

extern "C" {

// --- library --------------------------------------------------------------------

NWC24Err NWC24OpenLib(void* work) {
    Lock lock;
    sErrorCode = 0;
    if (work == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }
    if (sOpened) {
        return NWC24_ERR_LIB_OPENED;
    }
    LoadList();
    sOpened = true;
    return NWC24_OK;
}

NWC24Err NWC24CloseLib(void) {
    Lock lock;
    if (!sOpened) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }
    sOpened = false;
    return NWC24_OK;
}

// Checks that WiiConnect24 can be used for `usage` (settings, parental
// controls, errors of the daemon). Nothing objects here; the game checks the
// standby setting itself with SCGetIdleMode().
NWC24Err NWC24Check(u32 /*usage*/) {
    Lock lock;
    sErrorCode = 0;
    return sOpened ? NWC24_OK : NWC24_ERR_LIB_NOT_OPENED;
}

s32 NWC24GetErrorCode(void) {
    return sErrorCode;
}

u32 NWC24GetAppId(void) {
    return kTitleIdLo;
}

// Asks the daemon to stop what it is doing before a shutdown: a download in
// progress ends after the file it is at.
NWC24Err NWC24iRequestShutdownSync(u32 /*event*/) {
    sShutdownRequested = true;
    return NWC24_OK;
}

// --- download tasks ----------------------------------------------------------------

NWC24Err NWC24InitDlTask(NWC24DlTask* task, NWC24DlType type) {
    Lock lock;
    if (!sOpened) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }
    if (task == NULL || type >= 4) {
        return NWC24_ERR_INVALID_VALUE;
    }
    std::memset(task, 0, sizeof(*task));
    NWC24iDlTask* impl = Impl(task);
    impl->type = static_cast<u8>(type);
    impl->priority = 0x7F;
    impl->appId = kTitleIdLo;
    impl->titleIdHi = kTitleIdHi;
    impl->titleIdLo = kTitleIdLo;
    impl->id = kNoTaskId;
    impl->count = 1;
    impl->interval = 2880;
    impl->margin = 1440;
    if (IsOctetStream(impl->type)) {
        std::strcpy(impl->fileName, "content.bin");
    }
    return NWC24_OK;
}

NWC24Err NWC24CheckDlTask(const NWC24DlTask* task) {
    Lock lock;
    return CheckTask(task);
}

// The task this title registered, if any.
NWC24Err NWC24GetMyDlTask(NWC24DlTask* task) {
    Lock lock;
    if (!sOpened) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }
    if (task == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }
    if (!sList.registered) {
        return NWC24_ERR_NOT_FOUND;
    }
    std::memcpy(task, &sList.task, sizeof(*task));
    return NWC24_OK;
}

// Registers the task. The first download is due one interval from now.
NWC24Err NWC24AddDlTask(NWC24DlTask* task) {
    Lock lock;
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    NWC24iDlTask* impl = Impl(task);
    if (std::strlen(impl->url) < 8) {
        return NWC24_ERR_INVALID_VALUE;
    }
    s64 now = UniversalTime();
    if (impl->id != kNoTaskId) {
        // A task that has an id is updated, as in the SDK's AddDlTask().
        if (!sList.registered || impl->id != sList.task.id) {
            return NWC24_ERR_INVALID_VALUE;
        }
        impl->lastError = 0;
        impl->errorCount = 0;
    } else if (sList.registered) {
        return NWC24_ERR_FULL; // the list has room for one task
    } else {
        impl->id = kMyTaskId;
    }
    sList.task = *impl;
    sList.registered = 1;
    sList.lastAccess = static_cast<s32>(now / 60);
    sList.nextTime = static_cast<s32>((now + impl->interval * 60) / 60);
    SaveList();
    return NWC24_OK;
}

NWC24Err NWC24UpdateDlTask(NWC24DlTask* task) {
    Lock lock;
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    NWC24iDlTask* impl = Impl(task);
    if (impl->id == kNoTaskId) {
        return NWC24_ERR_INVALID_VALUE;
    }
    if (!sList.registered || impl->id != sList.task.id) {
        return NWC24_ERR_NOT_FOUND;
    }
    impl->lastError = 0;
    impl->errorCount = 0;
    sList.task = *impl;
    sList.lastAccess = static_cast<s32>(UniversalTime() / 60);
    SaveList();
    return NWC24_OK;
}

NWC24Err NWC24DeleteDlTask(NWC24DlTask* task) {
    Lock lock;
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    NWC24iDlTask* impl = Impl(task);
    if (!sList.registered || impl->id != sList.task.id) {
        return NWC24_ERR_NOT_FOUND;
    }
    std::memset(&sList, 0, sizeof(sList));
    impl->id = kNoTaskId;
    SaveList();
    return NWC24_OK;
}

NWC24Err NWC24GetDlTaskId(const NWC24DlTask* task, u16* id) {
    Lock lock;
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    if (id == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }
    *id = Impl(task)->id;
    return NWC24_OK;
}

NWC24Err NWC24SetDlPriority(NWC24DlTask* task, u8 priority) {
    Lock lock;
    NWC24Err result = CheckTask(task);
    if (result == NWC24_OK) {
        Impl(task)->priority = priority;
    }
    return result;
}

// Minutes between downloads.
NWC24Err NWC24SetDlInterval(NWC24DlTask* task, u16 interval) {
    Lock lock;
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    if (interval == 0) {
        return NWC24_ERR_INVALID_VALUE;
    }
    Impl(task)->interval = interval;
    return NWC24_OK;
}

NWC24Err NWC24GetDlInterval(const NWC24DlTask* task, u16* interval) {
    Lock lock;
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    if (interval == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }
    *interval = Impl(task)->interval;
    return NWC24_OK;
}

NWC24Err NWC24SetDlServerInterval(NWC24DlTask* task, u32 interval) {
    Lock lock;
    NWC24Err result = CheckTask(task);
    if (result == NWC24_OK) {
        Impl(task)->serverInterval = interval;
    }
    return result;
}

NWC24Err NWC24SetDlMargin(NWC24DlTask* task, u16 margin) {
    Lock lock;
    NWC24Err result = CheckTask(task);
    if (result == NWC24_OK) {
        Impl(task)->margin = margin;
    }
    return result;
}

// 0x40000000, which the game sets, is not interpreted here.
NWC24Err NWC24SetDlOption(NWC24DlTask* task, u32 flags) {
    Lock lock;
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    NWC24iDlTask* impl = Impl(task);
    if ((impl->type == NWC24_DLTYPE_MULTIPART_V1 || impl->type == NWC24_DLTYPE_OCTETSTREAM_V1) &&
        (flags & 0x80000038) != 0) {
        return NWC24_ERR_INVALID_OPERATION;
    }
    impl->flags = flags;
    return NWC24_OK;
}

// How many downloads are left before the daemon drops the task. Nothing counts
// down here (there is no background scheduler).
NWC24Err NWC24SetDlCount(NWC24DlTask* task, s16 count) {
    Lock lock;
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    if (count < 1) {
        return NWC24_ERR_INVALID_VALUE;
    }
    Impl(task)->count = count;
    return NWC24_OK;
}

NWC24Err NWC24SetDlSubTask(NWC24DlTask* task, NWC24DlSubTaskType type, u32 mask, u16 flags) {
    Lock lock;
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    if (type >= 5 || (type != NWC24_DL_STTYPE_NONE && mask == 0)) {
        return NWC24_ERR_INVALID_VALUE;
    }
    NWC24iDlTask* impl = Impl(task);
    if (impl->subTaskType != static_cast<u8>(type)) {
        std::memset(impl->lastUpdateSubTask, 0, sizeof(impl->lastUpdateSubTask));
        impl->lastUpdate = 0;
    }
    impl->subTaskType = static_cast<u8>(type);
    impl->subTaskMask = mask;
    impl->subTaskFlags = flags;
    impl->subTaskCounter = 0;
    return NWC24_OK;
}

NWC24Err NWC24SetDlUrl(NWC24DlTask* task, const char* url) {
    Lock lock;
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    if (url == NULL || std::strlen(url) < 7 || std::strlen(url) >= sizeof(Impl(task)->url)) {
        return NWC24_ERR_INVALID_VALUE;
    }
    if (std::strncmp(url, "http://", 7) != 0 && std::strncmp(url, "https://", 8) != 0) {
        return NWC24_ERR_FORMAT;
    }
    std::strcpy(Impl(task)->url, url);
    return NWC24_OK;
}

NWC24Err NWC24GetDlUrl(const NWC24DlTask* task, char* buf, u32 size) {
    Lock lock;
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    u32 length = static_cast<u32>(std::strlen(Impl(task)->url));
    if (length == 0) {
        return NWC24_ERR_NOT_FOUND;
    }
    if (length < 8) {
        return NWC24_ERR_FAILED;
    }
    if (length + 1 > size) {
        return NWC24_ERR_NOMEM;
    }
    std::strcpy(buf, Impl(task)->url);
    return NWC24_OK;
}

NWC24Err NWC24SetDlFilename(NWC24DlTask* task, const char* fileName) {
    Lock lock;
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    if (fileName == NULL || fileName[0] == '\0' || std::strlen(fileName) >= sizeof(Impl(task)->fileName)) {
        return NWC24_ERR_INVALID_VALUE;
    }
    if (!IsOctetStream(Impl(task)->type)) {
        return NWC24_ERR_INVALID_OPERATION;
    }
    std::strcpy(Impl(task)->fileName, fileName);
    return NWC24_OK;
}

// File name of sub-task `index`: with NWC24_DL_STFLAG_TRAILING_FILENAME the
// index is appended as ".NN".
NWC24Err NWC24GetDlFilename(const NWC24DlTask* task, char* buf, u32 size, u8 index) {
    Lock lock;
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    const NWC24iDlTask* impl = Impl(task);
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

// When sub-task `index` was last downloaded: seconds since 2000-01-01 UTC, to
// the minute; 0 if never.
NWC24Err NWC24GetDlSubTaskLastUpdate(const NWC24DlTask* task, u8 index, s64* time) {
    Lock lock;
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    if (time == NULL || index >= NWC24i_DL_SUBTASK_MAX) {
        return NWC24_ERR_INVALID_VALUE;
    }
    *time = static_cast<s64>(Impl(task)->lastUpdateSubTask[index]) * 60;
    return NWC24_OK;
}

// When the daemon would download next (seconds since 2000-01-01 UTC).
NWC24Err NWC24GetDlNextTime(const NWC24DlTask* task, s64* time) {
    Lock lock;
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    if (!sList.registered || Impl(task)->id != sList.task.id) {
        return NWC24_ERR_FATAL;
    }
    if (time == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }
    *time = static_cast<s64>(sList.nextTime) * 60;
    return NWC24_OK;
}

// The NAND file of the title's download archive.
NWC24Err NWC24GetDlVfPath(const NWC24DlTask* task, char* buf, u32 size) {
    Lock lock;
    NWC24Err result = CheckTask(task);
    if (result != NWC24_OK) {
        return result;
    }
    char path[NAND_MAX_PATH];
    VfPath(path, sizeof(path));
    if (std::strlen(path) + 1 > size) {
        return NWC24_ERR_NOMEM;
    }
    std::strcpy(buf, path);
    return NWC24_OK;
}

// Creates an empty archive of `size` bytes in the NAND file `path`.
NWC24Err NWC24CreateVF(const char* path, u32 size) {
    if (path == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }
    u8* image = static_cast<u8*>(std::malloc(size));
    if (image == nullptr) {
        return NWC24_ERR_NOMEM;
    }
    NWC24Err result = NWC24_OK;
    if (VFCreateSystemFileRAM(image, size) != 0) {
        result = NWC24_ERR_INTERNAL_VF;
    } else {
        s32 created = NANDCreate(path, NAND_PERM_RUSR | NAND_PERM_WUSR | NAND_PERM_RGRP | NAND_PERM_WGRP, 0);
        NANDFileInfo info;
        if (created == NAND_RESULT_EXISTS) {
            result = NWC24_ERR_FILE_EXISTS;
        } else if (created != NAND_RESULT_OK || NANDOpen(path, &info, NAND_ACCESS_WRITE) != NAND_RESULT_OK) {
            result = NWC24_ERR_FILE_OPEN;
        } else {
            if (NANDWrite(&info, image, size) != static_cast<s32>(size)) {
                result = NWC24_ERR_FILE_WRITE;
            }
            if (NANDClose(&info) != NAND_RESULT_OK && result == NWC24_OK) {
                result = NWC24_ERR_FILE_CLOSE;
            }
        }
    }
    std::free(image);
    return result;
}

NWC24Err NWC24CreateDlVf(const NWC24DlTask* task, u32 size) {
    {
        Lock lock;
        NWC24Err result = CheckTask(task);
        if (result != NWC24_OK) {
            return result;
        }
    }
    if (size < 0x2800) {
        return NWC24_ERR_INVALID_VALUE;
    }
    char path[NAND_MAX_PATH];
    VfPath(path, sizeof(path));
    return NWC24CreateVF(path, size);
}

// --- the downloader ------------------------------------------------------------------

// Downloads task `id` now and waits for it. `mask` chooses the sub-tasks (for
// the news: bit N = the file of hour N); a task without sub-tasks has one
// file. The game calls this with the library closed: it is a request to the
// daemon. (`flags` is 6 in the game; its meaning is not known.)
NWC24Err NWC24ExecDownloadTask(u32 /*flags*/, u32 id, u32 mask) {
    NWC24iDlTask task;
    {
        Lock lock;
        sErrorCode = 0;
        LoadList();
        if (!sList.registered || sList.task.id != id) {
            return NWC24_ERR_NOT_FOUND;
        }
        task = sList.task;
    }
    sShutdownRequested = false;

    const PCNewsSource* source = PCNewsGetSource();
    if (!source->available()) {
        sErrorCode = kErrorNoLink;
        return NWC24_ERR_NETWORK;
    }

    char vfPath[NAND_MAX_PATH];
    VfPath(vfPath, sizeof(vfPath));
    if (VFMountDriveNANDFlash(kDownloadDrive, vfPath) != 0) {
        std::fprintf(stderr, "NWC24: cannot open the download archive %s\n", vfPath);
        return NWC24_ERR_INTERNAL_VF;
    }

    bool subTasks = task.subTaskType != NWC24_DL_STTYPE_NONE;
    u32 count = subTasks ? NWC24i_DL_SUBTASK_MAX : 1;
    u32 wanted = subTasks ? (mask & task.subTaskMask) : 1;
    u32 done = 0;
    NWC24Err result = NWC24_OK;
    for (u32 i = 0; i < count && result == NWC24_OK; i++) {
        if ((wanted & (1u << i)) == 0) {
            continue;
        }
        if (sShutdownRequested) {
            result = NWC24_ERR_CANCELLED;
            break;
        }
        result = DownloadOne(source, &task, subTasks, static_cast<u8>(i));
        if (result == NWC24_OK) {
            task.lastUpdateSubTask[i] = static_cast<s32>(UniversalTime() / 60);
            done++;
        }
    }
    if (VFUnmountDrive(kDownloadDrive) != 0 && result == NWC24_OK) {
        result = NWC24_ERR_INTERNAL_VF;
    }

    std::fprintf(stderr, "NWC24: %u file(s) from %s%s\n", done, source->describe(),
                 result == NWC24_OK ? " (signatures not verified)" : "; the download failed");

    Lock lock;
    if (sList.registered && sList.task.id == id) {
        s64 now = UniversalTime();
        std::memcpy(sList.task.lastUpdateSubTask, task.lastUpdateSubTask, sizeof(task.lastUpdateSubTask));
        if (result == NWC24_OK) {
            sList.task.lastUpdate = static_cast<u32>(now / 60);
            sList.task.lastError = 0;
            sList.task.errorCount = 0;
        } else {
            sList.task.lastError = static_cast<u32>(sErrorCode);
            sList.task.errorCount++;
        }
        sList.lastAccess = static_cast<s32>(now / 60);
        sList.nextTime = static_cast<s32>((now + sList.task.interval * 60) / 60);
        SaveList();
    }
    return result;
}

} // extern "C"

// For the self-test: forget the task list in memory, so that the next
// NWC24OpenLib() reads the file of the current NAND directory.
void PCNWC24Reset() {
    Lock lock;
    sOpened = false;
    sListLoaded = false;
    sErrorCode = 0;
    std::memset(&sList, 0, sizeof(sList));
}
