// CNT: the channel's contents (src/revolution/CNT/cnt.c).
//
// On the Wii a content is a file inside the installed title that ES opens by
// index (ESP_OpenContentFile). On PC it is the host file
// <contents dir>/NN.app, written by `tools/extract_wad.py --contents`
// (NN = content index; the game's archive number n is content n + 2).
//
// The functions follow the SDK: a handle holds the content's U8 header and
// node table in memory (ARC does the path lookups) and file data is read from
// the content file on demand.
//
// Byte order (docs/pc_port.md, "Byte order"): contentReadNAND() converts the
// data to host order when one call reads a whole file from its start, which
// is how the game loads uncompressed files (LoadContentFile() in System.cpp).
// Partial reads return the raw bytes.

#include <revolution/cnt.h>
#include <revolution/os.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#include <pc/endian.h>
#include <pc/files.h>

namespace {

// CNT error codes (the right-hand column of __CNTConvertErrorCode's table).
const s32 CNT_ERROR_UNKNOWN = -0x13C7;  // -5063
const s32 CNT_ERROR_NOEXISTS = -0x1391; // -5009: no such file / bad position
const s32 CNT_ERROR_ALLOC = -0x1389;    // -5001: the allocator returned NULL

char sContentsDir[1024];
bool sContentsDirSet;
pthread_mutex_t sMutex = PTHREAD_MUTEX_INITIALIZER;

bool IsDirectory(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

void FindContentsDir() {
    if (sContentsDirSet) {
        return;
    }
    sContentsDirSet = true;

    const char* env = std::getenv("NEWSCHANNEL_CONTENTS");
    if (env != nullptr && env[0] != '\0') {
        std::snprintf(sContentsDir, sizeof(sContentsDir), "%s", env);
        return;
    }

    std::snprintf(sContentsDir, sizeof(sContentsDir), "orig/HAGE/contents");
    if (IsDirectory(sContentsDir)) {
        return;
    }

    // build/pc/newschannel -> <repository>/orig/HAGE/contents
    char exe[768];
    const ssize_t length = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (length > 0) {
        exe[length] = '\0';
        if (char* slash = std::strrchr(exe, '/')) {
            *slash = '\0';
            char candidate[1024];
            std::snprintf(candidate, sizeof(candidate), "%s/../../orig/HAGE/contents", exe);
            if (IsDirectory(candidate)) {
                std::snprintf(sContentsDir, sizeof(sContentsDir), "%s", candidate);
            }
        }
    }
}

void ContentPath(s32 index, char* out, size_t outSize) {
    std::snprintf(out, outSize, "%s/%02d.app", PCGetContentsDir(), static_cast<int>(index));
}

// Reads exactly `length` bytes at `offset`, or as many as the file has.
s32 ReadAt(int fd, void* dst, u32 length, u32 offset) {
    u32 done = 0;
    while (done < length) {
        const ssize_t n = pread(fd, static_cast<u8*>(dst) + done, length - done, static_cast<off_t>(offset) + done);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return CNT_ERROR_UNKNOWN;
        }
        if (n == 0) {
            break; // end of the content file
        }
        done += static_cast<u32>(n);
    }
    return static_cast<s32>(done);
}

} // namespace

extern "C" {

void PCSetContentsDir(const char* path) {
    pthread_mutex_lock(&sMutex);
    std::snprintf(sContentsDir, sizeof(sContentsDir), "%s", path);
    sContentsDirSet = true;
    pthread_mutex_unlock(&sMutex);
}

const char* PCGetContentsDir(void) {
    pthread_mutex_lock(&sMutex);
    FindContentsDir();
    pthread_mutex_unlock(&sMutex);
    return sContentsDir;
}

BOOL PCContentExists(s32 index) {
    char path[1100];
    ContentPath(index, path, sizeof(path));
    return access(path, R_OK) == 0 ? TRUE : FALSE;
}

void CNTInit(void) {
    // The Wii opens the ES device here. Nothing to open on PC; report where
    // the contents come from, once.
    static bool sInitialized;
    if (!sInitialized) {
        sInitialized = true;
        if (!IsDirectory(PCGetContentsDir())) {
            OSReport("CNT: contents directory '%s' not found. Extract the WAD with\n"
                     "     tools/extract_wad.py --contents, or set NEWSCHANNEL_CONTENTS / --contents-dir.\n",
                     PCGetContentsDir());
        }
    }
}

s32 CNTShutdown(void) {
    return 0;
}

s32 contentInitHandleNAND(s32 contentNum, CNTHandle* handle, MEMAllocator* allocator) {
    ARCHeader header;
    ARCHandle arcHandle;
    void* buffer;
    u32 size;
    s32 result;
    int fd;
    char path[1100];

    ContentPath(contentNum, path, sizeof(path));
    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        OSReport("CNT: cannot open content %d (%s): %s\n", static_cast<int>(contentNum), path,
                 std::strerror(errno));
        return errno == ENOENT ? CNT_ERROR_NOEXISTS : CNT_ERROR_UNKNOWN;
    }

    result = ReadAt(fd, &header, sizeof(ARCHeader), 0);
    if (result < 0) {
        close(fd);
        return result;
    }

    // PC: a content that is not an archive is refused here, before
    // ARCInitHandle() can panic. The game opens contents 6 to 11 as archives
    // and ignores the result; content 11 of the News Channel is not a U8 file
    // and the game never opens a file from that handle. (What ES answers for
    // it on a console has not been checked.) The handle is left untouched:
    // the game's handles are zero-initialised globals, and contentOpenNAND()
    // below refuses a handle without an archive.
    if (result != static_cast<s32>(sizeof(ARCHeader)) || PCReadBE32(&header.magic) != 0x55AA382D) {
        OSReport("CNT: content %d is not an archive; handle not initialised\n", static_cast<int>(contentNum));
        close(fd);
        return CNT_ERROR_UNKNOWN;
    }

    // The header is still big-endian: it is converted with the node table
    // inside ARCInitHandle().
    size = OSRoundUp32B(PCReadBE32(&header.fileStart));

    // (MEMAllocFromAllocator(allocator, size), spelled out so that CNT also
    // works with a caller-made allocator before the MEM backend exists. An
    // allocator that was never initialised allocates nothing.)
    buffer = (allocator != nullptr && allocator->pFunc != nullptr) ? allocator->pFunc->pfAlloc(allocator, size)
                                                                   : nullptr;
    if (buffer == nullptr) {
        close(fd);
        return CNT_ERROR_ALLOC;
    }

    result = ReadAt(fd, buffer, size, 0);
    if (result < 0) {
        allocator->pFunc->pfFree(allocator, buffer);
        close(fd);
        return result;
    }

    if (!ARCInitHandle(buffer, &arcHandle)) {
        allocator->pFunc->pfFree(allocator, buffer);
        close(fd);
        return CNT_ERROR_UNKNOWN;
    }
    std::memcpy(&handle->arcHandle, &arcHandle, sizeof(ARCHandle));
    handle->fd = fd;
    handle->allocator = allocator;
    return 0;
}

s32 contentOpenNAND(CNTHandle* handle, const char* path, CNTFileInfo* info) {
    ARCFileInfo arcInfo;
    s32 entrynum;

    if (handle->arcHandle.archiveStartAddr == nullptr) {
        return CNT_ERROR_NOEXISTS; // PC: the handle was never initialised
    }

    entrynum = ARCConvertPathToEntrynum(&handle->arcHandle, path);
    if (entrynum < 0) {
        return CNT_ERROR_NOEXISTS;
    }

    if (!ARCFastOpen(&handle->arcHandle, entrynum, &arcInfo)) {
        return CNT_ERROR_NOEXISTS;
    }

    info->handle = handle;
    info->offset = arcInfo.startOffset;
    info->length = arcInfo.length;
    info->position = 0;
    return 0;
}

s32 contentFastOpenNAND(CNTHandle* handle, s32 entrynum, CNTFileInfo* info) {
    ARCFileInfo arcInfo;

    if (handle->arcHandle.archiveStartAddr == nullptr || !ARCFastOpen(&handle->arcHandle, entrynum, &arcInfo)) {
        return CNT_ERROR_NOEXISTS;
    }

    info->handle = handle;
    info->offset = arcInfo.startOffset;
    info->length = arcInfo.length;
    info->position = 0;
    return 0;
}

s32 contentConvertPathToEntrynumNAND(CNTHandle* handle, const char* path) {
    if (handle->arcHandle.archiveStartAddr == nullptr) {
        return -1;
    }
    return ARCConvertPathToEntrynum(&handle->arcHandle, path);
}

u32 contentGetLengthNAND(const CNTFileInfo* info) {
    return info->length;
}

s32 contentSeekNAND(CNTFileInfo* info, s32 offset, s32 origin) {
    s32 pos;
    switch (origin) {
    case 0:
        pos = offset;
        break;
    case 1:
        pos = info->position + offset;
        break;
    case 2:
        pos = info->length + offset;
        break;
    default:
        return CNT_ERROR_NOEXISTS;
    }

    if (pos < 0 || pos > static_cast<s32>(info->length)) {
        return CNT_ERROR_NOEXISTS;
    }

    info->position = pos;
    return 0;
}

// Returns the number of bytes read. As on the Wii, the read is not limited to
// the file: the game rounds lengths up to 32 bytes and gets the bytes that
// follow in the content (the next file's alignment padding).
s32 contentReadNAND(CNTFileInfo* info, void* dst, u32 len, s32 offset) {
    s32 result;
    s32 pos = info->position + offset;

    if (pos < 0 || pos > static_cast<s32>(info->length)) {
        return CNT_ERROR_NOEXISTS;
    }

    result = ReadAt(info->handle->fd, dst, len, info->offset + pos);

    // PC: a whole file read in one call is converted to host byte order if it
    // is of a known format (.brfna fonts are loaded like this).
    if (result > 0 && pos == 0 && static_cast<u32>(result) >= info->length) {
        PCEndianFixFile(dst, info->length);
    }
    return result;
}

s32 contentCloseNAND(CNTFileInfo* info) {
    (void)info;
    return 0;
}

s32 contentReleaseHandleNAND(CNTHandle* handle) {
    if (handle->arcHandle.archiveStartAddr == nullptr) {
        return CNT_ERROR_UNKNOWN;
    }
    handle->allocator->pFunc->pfFree(handle->allocator, handle->arcHandle.archiveStartAddr);
    handle->arcHandle.archiveStartAddr = nullptr;
    return close(handle->fd) == 0 ? 0 : CNT_ERROR_UNKNOWN;
}

BOOL contentOpenDirNAND(CNTHandle* handle, const char* path, ARCDir* dir) {
    return ARCOpenDir(&handle->arcHandle, path, dir);
}

} // extern "C"
