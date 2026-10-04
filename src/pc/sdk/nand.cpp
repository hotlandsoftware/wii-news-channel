// NAND: the Wii's internal file system, on a host directory.
//
// The API and its semantics follow src/revolution/NAND (nand.c, NANDCore.c,
// NANDOpenClose.c); the ISFS calls underneath are replaced by host files:
//
//   NAND path                       host path
//   /tmp/opera.arc                  <nand dir>/tmp/opera.arc
//   noerase/savedata.dat            <nand dir>/title/00010002/48414745/data/noerase/savedata.dat
//
// <nand dir> is PCGetNandDir() (include/pc/files.h): `--nand-dir`,
// $NEWSCHANNEL_NAND, or ~/.local/share/newschannel/nand. Relative paths are
// resolved against the current directory, which starts as the title's home
// directory, as NANDInit() sets it on the Wii.
//
// What is kept from the Wii:
//   - result codes (NAND_RESULT_*), including EXISTS / NOEXISTS / NOTEMPTY,
//     ACCESS for `/shared2` without the "Private" calls, INVALID for a bad
//     permission byte, a closed NANDFileInfo or a seek outside the file
//   - paths are at most 63 characters; NANDCreate() fails if the file exists
//     and does not create parent directories; NANDDelete() removes a
//     directory with its contents
//   - NANDFileInfo::mark (1 = open, 2 = closed)
//
// What differs (documented, not needed by the game):
//   - no owner/group IDs or attribute byte are stored: NANDGetStatus() reports
//     this title as the owner and the permission byte from the host mode
//   - no block/inode quotas: NANDCheck() always answers "enough space"
//   - asynchronous calls do the work at once and call the callback BEFORE
//     they return (on the Wii the callback runs later, from the IPC
//     interrupt). All of them go through Complete() below; to defer the
//     callbacks, change that one function.
//   - NANDSafeOpen/NANDSafeClose, banners and NANDLogging are not implemented.
//
// Byte order: NAND files are read and written as they are. A file the game
// writes from a struct (the save file) is therefore little-endian on PC and
// not interchangeable with a Wii save; see docs/pc_port.md, "Byte order".

#include <revolution/nand.h>
#include <revolution/os.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#include <pc/files.h>

namespace {

const char HOME_DIR[] = "/title/00010002/48414745/data"; // 00010002-HAGE
const u32 TITLE_OWNER_ID = 0x48414745;                    // 'HAGE'
const u16 TITLE_GROUP_ID = 0x3031;                        // '01' (Nintendo)

pthread_mutex_t sMutex = PTHREAD_MUTEX_INITIALIZER;
char sNandDir[1024];
bool sNandDirSet;
bool sInitialized;
char sCurrentDir[NAND_MAX_PATH] = "/";
char sHomeDir[NAND_MAX_PATH] = "";

struct Lock {
    Lock() { pthread_mutex_lock(&sMutex); }
    ~Lock() { pthread_mutex_unlock(&sMutex); }
};

s32 ErrnoToResult(int error) {
    switch (error) {
    case 0:
        return NAND_RESULT_OK;
    case ENOENT:
    case ENOTDIR:
        return NAND_RESULT_NOEXISTS;
    case EEXIST:
        return NAND_RESULT_EXISTS;
    case EACCES:
    case EPERM:
    case EROFS:
    case EISDIR:
        return NAND_RESULT_ACCESS;
    case ENOTEMPTY:
        return NAND_RESULT_NOTEMPTY;
    case EMFILE:
    case ENFILE:
        return NAND_RESULT_MAXFD;
    case ENOSPC:
    case EDQUOT:
        return NAND_RESULT_MAXBLOCKS;
    case ENAMETOOLONG:
    case EINVAL:
    case EBADF:
        return NAND_RESULT_INVALID;
    case ENOMEM:
        return NAND_RESULT_ALLOC_FAILED;
    case EBUSY:
        return NAND_RESULT_BUSY;
    default:
        return NAND_RESULT_UNKNOWN;
    }
}

void MakeDirs(const char* path) {
    char partial[1200];
    std::snprintf(partial, sizeof(partial), "%s", path);
    for (char* p = partial + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(partial, 0755);
            *p = '/';
        }
    }
    mkdir(partial, 0755);
}

// sMutex held.
void FindNandDir() {
    if (sNandDirSet) {
        return;
    }
    sNandDirSet = true;

    const char* env = std::getenv("NEWSCHANNEL_NAND");
    if (env != nullptr && env[0] != '\0') {
        std::snprintf(sNandDir, sizeof(sNandDir), "%s", env);
        return;
    }
    const char* data = std::getenv("XDG_DATA_HOME");
    if (data != nullptr && data[0] == '/') {
        std::snprintf(sNandDir, sizeof(sNandDir), "%s/newschannel/nand", data);
        return;
    }
    const char* home = std::getenv("HOME");
    if (home != nullptr && home[0] == '/') {
        std::snprintf(sNandDir, sizeof(sNandDir), "%s/.local/share/newschannel/nand", home);
        return;
    }
    std::snprintf(sNandDir, sizeof(sNandDir), "nand");
}

// What NANDInit() does on the Wii: find the title's home directory and make
// it the current one. On PC it also creates the directories every Wii has.
// sMutex held.
void Initialize() {
    if (sInitialized) {
        return;
    }
    sInitialized = true;
    FindNandDir();

    char host[1200];
    std::snprintf(host, sizeof(host), "%s%s", sNandDir, HOME_DIR);
    MakeDirs(host);
    std::snprintf(host, sizeof(host), "%s/tmp", sNandDir);
    MakeDirs(host);
    std::snprintf(host, sizeof(host), "%s/shared2", sNandDir);
    MakeDirs(host);

    std::strcpy(sHomeDir, HOME_DIR);
    std::strcpy(sCurrentDir, HOME_DIR);
}

// --- path handling, as in NANDCore.c -------------------------------------

void RemoveTailToken(char* newpath, const char* oldpath) {
    if (oldpath[0] == '/' && oldpath[1] == '\0') {
        newpath[0] = '/';
        newpath[1] = '\0';
        return;
    }
    for (int i = static_cast<int>(std::strlen(oldpath)) - 1; i >= 0; --i) {
        if (oldpath[i] == '/') {
            if (i != 0) {
                std::strncpy(newpath, oldpath, static_cast<size_t>(i));
                newpath[i] = '\0';
            } else {
                newpath[0] = '/';
                newpath[1] = '\0';
            }
            break;
        }
    }
}

// Absolute form of `path` (nandGenerateAbsPath + nandConvertPath), with the
// SDK's limit of NAND_MAX_PATH. FALSE if it does not fit or is empty.
// sMutex held.
bool GenerateAbsPath(char* absPath, const char* path) {
    char work[256];
    size_t length;

    if (path == nullptr || path[0] == '\0') {
        return false;
    }

    if (path[0] == '/') {
        if (std::strlen(path) >= sizeof(work)) {
            return false;
        }
        std::strcpy(work, path);
        length = std::strlen(work);
        if (length > 1 && work[length - 1] == '/') {
            work[length - 1] = '\0';
        }
    } else {
        std::strcpy(work, sCurrentDir);
        const char* rest = path;
        while (*rest != '\0') {
            char token[128];
            size_t n = 0;
            while (rest[n] != '\0' && rest[n] != '/') {
                n++;
            }
            if (n >= sizeof(token)) {
                return false;
            }
            std::memcpy(token, rest, n);
            token[n] = '\0';
            rest += n;
            if (*rest == '/') {
                rest++;
            }

            if (token[0] == '\0' || std::strcmp(token, ".") == 0) {
                continue;
            }
            if (std::strcmp(token, "..") == 0) {
                char parent[256];
                RemoveTailToken(parent, work);
                std::strcpy(work, parent);
                continue;
            }
            if (std::strlen(work) + 1 + n >= sizeof(work)) {
                return false;
            }
            if (std::strcmp(work, "/") != 0) {
                std::strcat(work, "/");
            }
            std::strcat(work, token);
        }
    }

    if (std::strlen(work) >= NAND_MAX_PATH) {
        return false;
    }
    std::strcpy(absPath, work);
    return true;
}

// Never leave the NAND directory: an absolute path is taken as given by the
// SDK, so refuse ".." components in the final path.
bool HasDotDot(const char* absPath) {
    for (const char* p = absPath; *p != '\0'; p++) {
        if (p[0] == '/' && p[1] == '.' && p[2] == '.' && (p[3] == '/' || p[3] == '\0')) {
            return true;
        }
    }
    return false;
}

// NAND path -> absolute NAND path and host path. Returns a NAND result.
s32 Resolve(const char* path, bool privileged, bool underPrivateOnly, char* absPath, char* host,
            size_t hostSize) {
    Lock lock;
    Initialize();
    if (!GenerateAbsPath(absPath, path) || HasDotDot(absPath)) {
        return NAND_RESULT_INVALID;
    }
    if (!privileged) {
        const bool denied = underPrivateOnly ? (nandIsPrivatePath(absPath) && absPath[8] == '/' && absPath[9] != '\0')
                                             : nandIsPrivatePath(absPath);
        if (denied) {
            return NAND_RESULT_ACCESS;
        }
    }
    if (static_cast<size_t>(std::snprintf(host, hostSize, "%s%s", sNandDir, absPath)) >= hostSize) {
        return NAND_RESULT_INVALID;
    }
    return NAND_RESULT_OK;
}

// nandInspectPermission(): only the six RW bits may be set.
bool InspectPermission(u8 perm) {
    return (perm & 0xC0) == 0;
}

// The host mode for a NAND permission byte. The owner always keeps read and
// write on the host, or the program could lock itself out of its own files.
mode_t PermissionToMode(u8 perm, bool directory) {
    mode_t mode = S_IRUSR | S_IWUSR;
    if (perm & NAND_PERM_RGRP) {
        mode |= S_IRGRP;
    }
    if (perm & NAND_PERM_WGRP) {
        mode |= S_IWGRP;
    }
    if (perm & NAND_PERM_ROTH) {
        mode |= S_IROTH;
    }
    if (perm & NAND_PERM_WOTH) {
        mode |= S_IWOTH;
    }
    if (directory) {
        mode |= S_IXUSR | ((mode & S_IRGRP) ? S_IXGRP : 0) | ((mode & S_IROTH) ? S_IXOTH : 0);
    }
    return mode;
}

u8 ModeToPermission(mode_t mode) {
    u8 perm = 0;
    if (mode & S_IRUSR) {
        perm |= NAND_PERM_RUSR;
    }
    if (mode & S_IWUSR) {
        perm |= NAND_PERM_WUSR;
    }
    if (mode & S_IRGRP) {
        perm |= NAND_PERM_RGRP;
    }
    if (mode & S_IWGRP) {
        perm |= NAND_PERM_WGRP;
    }
    if (mode & S_IROTH) {
        perm |= NAND_PERM_ROTH;
    }
    if (mode & S_IWOTH) {
        perm |= NAND_PERM_WOTH;
    }
    return perm;
}

// Removes a file, or a directory with everything in it (ISFS_Delete).
int RemoveTree(const char* host) {
    struct stat st;
    if (lstat(host, &st) != 0) {
        return errno;
    }
    if (!S_ISDIR(st.st_mode)) {
        return unlink(host) == 0 ? 0 : errno;
    }
    DIR* dir = opendir(host);
    if (dir == nullptr) {
        return errno;
    }
    int error = 0;
    while (struct dirent* entry = readdir(dir)) {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char child[1400];
        std::snprintf(child, sizeof(child), "%s/%s", host, entry->d_name);
        error = RemoveTree(child);
        if (error != 0) {
            break;
        }
    }
    closedir(dir);
    if (error != 0) {
        return error;
    }
    return rmdir(host) == 0 ? 0 : errno;
}

// --- the operations ----------------------------------------------------------

s32 DoCreate(const char* path, u8 perm, u8 attr, bool privileged, bool directory) {
    (void)attr;
    char absPath[NAND_MAX_PATH];
    char host[1200];
    s32 result = Resolve(path, privileged, false, absPath, host, sizeof(host));
    if (result != NAND_RESULT_OK) {
        return result;
    }
    if (!InspectPermission(perm)) {
        return NAND_RESULT_INVALID;
    }
    const mode_t mode = PermissionToMode(perm, directory);
    // The mode is set again after creation: the permission byte must not
    // depend on the process's umask.
    if (directory) {
        if (mkdir(host, mode) != 0) {
            return ErrnoToResult(errno);
        }
        chmod(host, mode);
        return NAND_RESULT_OK;
    }
    const int fd = open(host, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, mode);
    if (fd < 0) {
        return ErrnoToResult(errno);
    }
    fchmod(fd, mode);
    close(fd);
    return NAND_RESULT_OK;
}

s32 DoOpen(const char* path, NANDFileInfo* info, u8 accType, bool privileged) {
    char absPath[NAND_MAX_PATH];
    char host[1200];
    s32 result = Resolve(path, privileged, false, absPath, host, sizeof(host));
    if (result != NAND_RESULT_OK) {
        return result;
    }

    int flags;
    switch (accType) {
    case NAND_ACCESS_READ:
        flags = O_RDONLY;
        break;
    case NAND_ACCESS_WRITE:
        flags = O_WRONLY;
        break;
    case NAND_ACCESS_RW:
        flags = O_RDWR;
        break;
    default:
        return NAND_RESULT_INVALID;
    }

    struct stat st;
    if (stat(host, &st) == 0 && S_ISDIR(st.st_mode)) {
        return NAND_RESULT_ACCESS;
    }
    const int fd = open(host, flags | O_CLOEXEC);
    if (fd < 0) {
        return ErrnoToResult(errno);
    }
    info->fileDescriptor = fd;
    info->mark = 1;
    return NAND_RESULT_OK;
}

s32 DoClose(NANDFileInfo* info) {
    if (info->mark != 1) {
        return NAND_RESULT_INVALID;
    }
    if (close(info->fileDescriptor) != 0) {
        return ErrnoToResult(errno);
    }
    info->mark = 2;
    return NAND_RESULT_OK;
}

s32 DoRead(NANDFileInfo* info, void* buf, u32 length) {
    u32 done = 0;
    while (done < length) {
        const ssize_t n = read(info->fileDescriptor, static_cast<u8*>(buf) + done, length - done);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return errno == EBADF ? NAND_RESULT_ACCESS : ErrnoToResult(errno);
        }
        if (n == 0) {
            break;
        }
        done += static_cast<u32>(n);
    }
    return static_cast<s32>(done);
}

s32 DoWrite(NANDFileInfo* info, const void* buf, u32 length) {
    u32 done = 0;
    while (done < length) {
        const ssize_t n = write(info->fileDescriptor, static_cast<const u8*>(buf) + done, length - done);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return errno == EBADF ? NAND_RESULT_ACCESS : ErrnoToResult(errno);
        }
        done += static_cast<u32>(n);
    }
    return static_cast<s32>(done);
}

s32 DoSeek(NANDFileInfo* info, s32 offset, s32 whence) {
    int hostWhence;
    switch (whence) {
    case NAND_SEEK_BEG:
        hostWhence = SEEK_SET;
        break;
    case NAND_SEEK_CUR:
        hostWhence = SEEK_CUR;
        break;
    case NAND_SEEK_END:
        hostWhence = SEEK_END;
        break;
    default:
        return NAND_RESULT_INVALID;
    }

    // ISFS does not seek outside the file.
    struct stat st;
    const off_t current = lseek(info->fileDescriptor, 0, SEEK_CUR);
    if (current < 0 || fstat(info->fileDescriptor, &st) != 0) {
        return ErrnoToResult(errno);
    }
    const long long base = hostWhence == SEEK_SET ? 0 : hostWhence == SEEK_CUR ? current : st.st_size;
    const long long target = base + offset;
    if (target < 0 || target > st.st_size) {
        return NAND_RESULT_INVALID;
    }
    const off_t position = lseek(info->fileDescriptor, static_cast<off_t>(target), SEEK_SET);
    if (position < 0) {
        return ErrnoToResult(errno);
    }
    return static_cast<s32>(position);
}

s32 DoDelete(const char* path, bool privileged) {
    char absPath[NAND_MAX_PATH];
    char host[1200];
    s32 result = Resolve(path, privileged, false, absPath, host, sizeof(host));
    if (result != NAND_RESULT_OK) {
        return result;
    }
    if (std::strcmp(absPath, "/") == 0) {
        return NAND_RESULT_ACCESS;
    }
    return ErrnoToResult(RemoveTree(host));
}

s32 DoGetStatus(const char* path, NANDStatus* stat_, bool privileged) {
    char absPath[NAND_MAX_PATH];
    char host[1200];
    s32 result = Resolve(path, privileged, true, absPath, host, sizeof(host));
    if (result != NAND_RESULT_OK) {
        return result;
    }
    struct stat st;
    if (stat(host, &st) != 0) {
        return ErrnoToResult(errno);
    }
    stat_->ownerId = TITLE_OWNER_ID;
    stat_->groupId = TITLE_GROUP_ID;
    stat_->attribute = 0;
    stat_->permission = ModeToPermission(st.st_mode);
    return NAND_RESULT_OK;
}

s32 DoGetType(const char* path, u8* type, bool privileged) {
    char absPath[NAND_MAX_PATH];
    char host[1200];
    s32 result = Resolve(path, privileged, true, absPath, host, sizeof(host));
    if (result != NAND_RESULT_OK) {
        return result;
    }
    struct stat st;
    if (stat(host, &st) != 0) {
        return ErrnoToResult(errno);
    }
    *type = S_ISDIR(st.st_mode) ? 2 : 1; // NAND_TYPE_DIR : NAND_TYPE_FILE
    return NAND_RESULT_OK;
}

// Every asynchronous call ends here. PC: the work has been done already; the
// callback runs now, before the *Async function returns NAND_RESULT_OK.
s32 Complete(s32 result, NANDCallback callback, NANDCommandBlock* block) {
    if (block != nullptr) {
        block->callback = reinterpret_cast<void*>(callback);
    }
    if (callback != nullptr) {
        callback(result, block);
    }
    return NAND_RESULT_OK;
}

} // namespace

extern "C" {

// --- include/pc/files.h ----------------------------------------------------

void PCSetNandDir(const char* path) {
    Lock lock;
    if (path == nullptr || path[0] == '\0') {
        sNandDirSet = false; // back to the default
    } else {
        std::snprintf(sNandDir, sizeof(sNandDir), "%s", path);
        sNandDirSet = true;
    }
    std::strcpy(sCurrentDir, "/");
    sInitialized = false; // create the standard directories in the new place
}

const char* PCGetNandDir(void) {
    Lock lock;
    FindNandDir();
    return sNandDir;
}

BOOL PCNandHostPath(const char* nandPath, char* out, u32 outSize) {
    char absPath[NAND_MAX_PATH];
    return Resolve(nandPath, true, false, absPath, out, outSize) == NAND_RESULT_OK ? TRUE : FALSE;
}

// --- NANDCore.c ------------------------------------------------------------

s32 NANDInit(void) {
    Lock lock;
    Initialize();
    return NAND_RESULT_OK;
}

BOOL nandIsInitialized(void) {
    return TRUE; // initialised on first use
}

BOOL nandIsPrivatePath(const char* path) {
    return std::strncmp(path, "/shared2", 8) == 0 ? TRUE : FALSE;
}

void nandGenerateAbsPath(char* absPath, const char* path) {
    Lock lock;
    Initialize();
    if (!GenerateAbsPath(absPath, path)) {
        absPath[0] = '\0';
    }
}

void nandGetParentDirectory(char* parentDir, const char* absPath) {
    int i;
    for (i = static_cast<int>(std::strlen(absPath)); i >= 0; --i) {
        if (absPath[i] == '/') {
            break;
        }
    }
    if (i <= 0) {
        std::strcpy(parentDir, "/");
    } else {
        std::strncpy(parentDir, absPath, static_cast<size_t>(i));
        parentDir[i] = '\0';
    }
}

void nandGetRelativeName(char* name, const char* path) {
    if (std::strcmp(path, "/") == 0) {
        std::strcpy(name, "");
        return;
    }
    const char* slash = std::strrchr(path, '/');
    std::strcpy(name, slash != nullptr ? slash + 1 : path);
}

const char* nandGetHomeDir(void) {
    Lock lock;
    Initialize();
    return sHomeDir;
}

s32 NANDGetCurrentDir(char* path) {
    Lock lock;
    Initialize();
    std::strcpy(path, sCurrentDir);
    return NAND_RESULT_OK;
}

s32 NANDGetHomeDir(char* path) {
    Lock lock;
    Initialize();
    std::strcpy(path, sHomeDir);
    return NAND_RESULT_OK;
}

s32 NANDGetType(const char* path, u8* type) {
    return DoGetType(path, type, false);
}

s32 NANDPrivateGetTypeAsync(const char* path, u8* type, NANDCallback cb, NANDCommandBlock* block) {
    return Complete(DoGetType(path, type, true), cb, block);
}

// --- nand.c ----------------------------------------------------------------

s32 NANDCreate(const char* path, u8 perm, u8 attr) {
    return DoCreate(path, perm, attr, false, false);
}

s32 NANDPrivateCreate(const char* path, u8 perm, u8 attr) {
    return DoCreate(path, perm, attr, true, false);
}

s32 NANDPrivateCreateAsync(const char* path, u8 perm, u8 attr, NANDCallback cb, NANDCommandBlock* block) {
    return Complete(DoCreate(path, perm, attr, true, false), cb, block);
}

s32 NANDCreateDir(const char* path, u8 perm, u8 attr) {
    return DoCreate(path, perm, attr, false, true);
}

s32 NANDPrivateCreateDirAsync(const char* path, u8 perm, u8 attr, NANDCallback cb, NANDCommandBlock* block) {
    return Complete(DoCreate(path, perm, attr, true, true), cb, block);
}

s32 NANDDelete(const char* path) {
    return DoDelete(path, false);
}

s32 NANDPrivateDelete(const char* path) {
    return DoDelete(path, true);
}

s32 NANDPrivateDeleteAsync(const char* path, NANDCallback cb, NANDCommandBlock* block) {
    return Complete(DoDelete(path, true), cb, block);
}

s32 NANDRead(NANDFileInfo* info, void* buf, u32 length) {
    return DoRead(info, buf, length);
}

s32 NANDReadAsync(NANDFileInfo* info, void* buf, u32 length, NANDCallback cb, NANDCommandBlock* block) {
    return Complete(DoRead(info, buf, length), cb, block);
}

s32 NANDWrite(NANDFileInfo* info, const void* buf, u32 length) {
    return DoWrite(info, buf, length);
}

s32 NANDWriteAsync(NANDFileInfo* info, const void* buf, u32 length, NANDCallback cb, NANDCommandBlock* block) {
    return Complete(DoWrite(info, buf, length), cb, block);
}

s32 NANDSeek(NANDFileInfo* info, s32 offset, s32 whence) {
    return DoSeek(info, offset, whence);
}

s32 NANDSeekAsync(NANDFileInfo* info, s32 offset, s32 whence, NANDCallback cb, NANDCommandBlock* block) {
    return Complete(DoSeek(info, offset, whence), cb, block);
}

s32 NANDGetLength(NANDFileInfo* info, u32* length) {
    struct stat st;
    if (fstat(info->fileDescriptor, &st) != 0) {
        return ErrnoToResult(errno);
    }
    if (length != nullptr) {
        *length = static_cast<u32>(st.st_size);
    }
    return NAND_RESULT_OK;
}

// `nameList` receives the names one after another, each NUL-terminated; `num`
// is the capacity on entry and the count on return. With nameList == NULL
// only the count is returned.
s32 NANDReadDir(const char* path, char* nameList, u32* num) {
    char absPath[NAND_MAX_PATH];
    char host[1200];
    s32 result = Resolve(path, false, false, absPath, host, sizeof(host));
    if (result != NAND_RESULT_OK) {
        return result;
    }
    DIR* dir = opendir(host);
    if (dir == nullptr) {
        return errno == ENOTDIR ? NAND_RESULT_INVALID : ErrnoToResult(errno);
    }
    const u32 capacity = nameList != nullptr ? *num : 0;
    u32 count = 0;
    while (struct dirent* entry = readdir(dir)) {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (nameList != nullptr) {
            if (count >= capacity) {
                break;
            }
            const size_t length = std::strlen(entry->d_name) + 1;
            std::memcpy(nameList, entry->d_name, length);
            nameList += length;
        }
        count++;
    }
    closedir(dir);
    *num = count;
    return NAND_RESULT_OK;
}

// Moves `path` into the directory `destDir`, keeping its name.
s32 NANDMove(const char* path, const char* destDir) {
    char absPath[NAND_MAX_PATH], absDir[NAND_MAX_PATH];
    char host[1200], hostDir[1200], hostDest[1400];
    s32 result = Resolve(path, false, false, absPath, host, sizeof(host));
    if (result == NAND_RESULT_OK) {
        result = Resolve(destDir, false, false, absDir, hostDir, sizeof(hostDir));
    }
    if (result != NAND_RESULT_OK) {
        return result;
    }
    char name[NAND_MAX_PATH];
    nandGetRelativeName(name, absPath);
    if (std::strlen(absDir) + 1 + std::strlen(name) >= NAND_MAX_PATH) {
        return NAND_RESULT_INVALID;
    }
    std::snprintf(hostDest, sizeof(hostDest), "%s/%s", hostDir, name);
    struct stat st;
    if (stat(hostDest, &st) == 0) {
        return NAND_RESULT_EXISTS;
    }
    return rename(host, hostDest) == 0 ? NAND_RESULT_OK : ErrnoToResult(errno);
}

s32 NANDGetStatus(const char* path, NANDStatus* stat_) {
    return DoGetStatus(path, stat_, false);
}

s32 NANDPrivateGetStatus(const char* path, NANDStatus* stat_) {
    return DoGetStatus(path, stat_, true);
}

s32 NANDPrivateGetStatusAsync(const char* path, NANDStatus* stat_, NANDCallback cb, NANDCommandBlock* block) {
    return Complete(DoGetStatus(path, stat_, true), cb, block);
}

s32 NANDPrivateSetStatus(const char* path, const NANDStatus* stat_) {
    char absPath[NAND_MAX_PATH];
    char host[1200];
    s32 result = Resolve(path, true, false, absPath, host, sizeof(host));
    if (result != NAND_RESULT_OK) {
        return result;
    }
    if (!InspectPermission(stat_->permission)) {
        return NAND_RESULT_INVALID;
    }
    struct stat st;
    if (stat(host, &st) != 0) {
        return ErrnoToResult(errno);
    }
    return chmod(host, PermissionToMode(stat_->permission, S_ISDIR(st.st_mode))) == 0 ? NAND_RESULT_OK
                                                                                       : ErrnoToResult(errno);
}

// Free space check: the host has no block or inode quota.
s32 NANDCheck(u32 fsBlock, u32 inode, u32* answer) {
    (void)fsBlock;
    (void)inode;
    if (answer != nullptr) {
        *answer = 0;
    }
    return NAND_RESULT_OK;
}

// --- NANDOpenClose.c ---------------------------------------------------------

s32 NANDOpen(const char* path, NANDFileInfo* info, u8 accType) {
    return DoOpen(path, info, accType, false);
}

s32 NANDPrivateOpen(const char* path, NANDFileInfo* info, u8 accType) {
    return DoOpen(path, info, accType, true);
}

s32 NANDOpenAsync(const char* path, NANDFileInfo* info, u8 accType, NANDCallback cb, NANDCommandBlock* block) {
    return Complete(DoOpen(path, info, accType, false), cb, block);
}

s32 NANDPrivateOpenAsync(const char* path, NANDFileInfo* info, const u8 accType, NANDCallback cb,
                         NANDCommandBlock* block) {
    return Complete(DoOpen(path, info, accType, true), cb, block);
}

s32 NANDClose(NANDFileInfo* info) {
    return DoClose(info);
}

s32 NANDCloseAsync(NANDFileInfo* info, NANDCallback cb, NANDCommandBlock* block) {
    return Complete(DoClose(info), cb, block);
}

} // extern "C"
