// VF: the archive library WiiConnect24 stores downloaded files in.
//
// On the Wii a VF archive is a FAT file system (PrFILE2) inside one NAND file
// (wc24dl.vff) or a block of memory. The game never looks inside: it reads the
// NAND file into memory, mounts the memory as a drive and opens files by name.
// So the archive is the backend's business, and on PC it is NOT a FAT image:
//
//   Header (32 bytes)  magic "PCVF", version, image size, directory capacity,
//                      number of entries, start and end of the file data
//   Directory          capacity * 64 bytes: name (52), offset, size, reserved
//   File data          one extent per file, in directory order, no gaps
//
// All fields are in host byte order. A wc24dl.vff in the PC NAND directory is
// therefore not interchangeable with one from a console (docs/pc_port.md,
// "How the channel gets its news").
//
// A drive keeps no state of its own besides the pointer to its image: the
// game mounts a memory block first and fills it from NAND afterwards
// (CWiiConnect24::readFiles), so every call reads the image as it is then.
//
// Result codes: 0, or a PrFILE2 error number (they follow errno: 2 = no such
// file, 5 = I/O error, ...); VF_ERROR_B001 stands for "the drive has no usable
// file system". The game only tests for 0, VF_ERROR_0002 and VF_ERROR_B001.

#include <revolution/vf.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <pthread.h>

#include <revolution/nand.h>

namespace {

const char kMagic[4] = {'P', 'C', 'V', 'F'};
const u32 kVersion = 1;
const u32 kMaxEntries = 64;
const u32 kNameSize = 52;
const u32 kMinImageSize = 0x400;

enum {
    kErrNoEntry = VF_ERROR_0002,
    kErrIO = VF_ERROR_0005,
    kErrBadFile = 9,
    kErrExists = 17,
    kErrInvalid = 22,
    kErrTooManyOpen = 24,
    kErrNoSpace = 28,
    kErrNoFileSystem = VF_ERROR_B001,
};

struct ImageHeader {
    char magic[4];
    u32 version;
    u32 imageSize;
    u32 capacity; // directory entries
    u32 count;    // entries in use: the first `count`
    u32 dataStart;
    u32 dataEnd;
    u32 reserved;
};

struct ImageEntry {
    char name[kNameSize];
    u32 offset;
    u32 size;
    u32 reserved;
};

static_assert(sizeof(ImageHeader) == 32, "VF image header");
static_assert(sizeof(ImageEntry) == 64, "VF image entry");

struct Drive {
    bool used;
    char name[16];
    u8* image;
    // Drives mounted from a NAND file: the image is ours, and is written back
    // when it changed.
    bool fromNand;
    bool dirty;
    u32 nandSize;
    char nandPath[NAND_MAX_PATH];
};

struct OpenFile {
    bool used;
    Drive* drive;
    char name[kNameSize];
    u32 position;
    // Files opened for writing collect their contents here until they are closed.
    bool writing;
    u8* buffer;
    u32 length;
    u32 capacity;
};

// What VFFindFirst() leaves in the caller's buffer (0x448 bytes on the Wii).
struct FindState {
    u32 magic;
    Drive* drive;
    u32 next;
    char pattern[kNameSize];
    char name[kNameSize];
    u32 size;
};

const u32 kFindMagic = 0x56464454; // 'VFDT'

pthread_mutex_t sMutex = PTHREAD_MUTEX_INITIALIZER;
Drive sDrives[4];
Drive* sCurrentDrive; // for paths without a drive name: the last one mounted
OpenFile sFiles[8];
s32 sLastError;

struct Lock {
    Lock() { pthread_mutex_lock(&sMutex); }
    ~Lock() { pthread_mutex_unlock(&sMutex); }
};

s32 Fail(s32 error) {
    sLastError = error;
    return error;
}

// The header of a valid image, or NULL.
ImageHeader* Header(u8* image) {
    if (image == nullptr) {
        return nullptr;
    }
    ImageHeader* header = reinterpret_cast<ImageHeader*>(image);
    if (std::memcmp(header->magic, kMagic, 4) != 0 || header->version != kVersion ||
        header->capacity == 0 || header->capacity > kMaxEntries || header->count > header->capacity ||
        header->dataStart != sizeof(ImageHeader) + header->capacity * sizeof(ImageEntry) ||
        header->dataEnd < header->dataStart || header->dataEnd > header->imageSize) {
        return nullptr;
    }
    return header;
}

ImageEntry* Entries(ImageHeader* header) {
    return reinterpret_cast<ImageEntry*>(reinterpret_cast<u8*>(header) + sizeof(ImageHeader));
}

bool Format(u8* image, u32 size) {
    if (image == nullptr || size < kMinImageSize) {
        return false;
    }
    u32 capacity = kMaxEntries;
    while (capacity > 1 && sizeof(ImageHeader) + capacity * sizeof(ImageEntry) > size / 2) {
        capacity /= 2;
    }
    std::memset(image, 0, size);
    ImageHeader* header = reinterpret_cast<ImageHeader*>(image);
    std::memcpy(header->magic, kMagic, 4);
    header->version = kVersion;
    header->imageSize = size;
    header->capacity = capacity;
    header->count = 0;
    header->dataStart = sizeof(ImageHeader) + capacity * sizeof(ImageEntry);
    header->dataEnd = header->dataStart;
    return true;
}

// File names compare without case, as on FAT.
bool SameName(const char* a, const char* b) {
    for (;; a++, b++) {
        char x = *a >= 'A' && *a <= 'Z' ? static_cast<char>(*a + 32) : *a;
        char y = *b >= 'A' && *b <= 'Z' ? static_cast<char>(*b + 32) : *b;
        if (x != y) {
            return false;
        }
        if (x == '\0') {
            return true;
        }
    }
}

ImageEntry* FindEntry(ImageHeader* header, const char* name) {
    ImageEntry* entries = Entries(header);
    for (u32 i = 0; i < header->count; i++) {
        if (SameName(entries[i].name, name)) {
            return &entries[i];
        }
    }
    return nullptr;
}

// Removes an entry and closes the gap in the file data.
void RemoveEntry(ImageHeader* header, ImageEntry* entry) {
    u8* image = reinterpret_cast<u8*>(header);
    ImageEntry* entries = Entries(header);
    u32 index = static_cast<u32>(entry - entries);
    u32 offset = entry->offset;
    u32 size = entry->size;
    std::memmove(image + offset, image + offset + size, header->dataEnd - (offset + size));
    header->dataEnd -= size;
    for (u32 i = index + 1; i < header->count; i++) {
        entries[i - 1] = entries[i];
        entries[i - 1].offset -= size;
    }
    header->count--;
    std::memset(&entries[header->count], 0, sizeof(ImageEntry));
}

Drive* FindDrive(const char* name, size_t length) {
    for (Drive& drive : sDrives) {
        if (drive.used && std::strlen(drive.name) == length && std::strncmp(drive.name, name, length) == 0) {
            return &drive;
        }
    }
    return nullptr;
}

Drive* FindDrive(const char* name) {
    return name != nullptr ? FindDrive(name, std::strlen(name)) : nullptr;
}

// Splits "drive:/name" or "name" into the drive and the file name (which may
// be a pattern). Directories do not exist in these archives.
bool SplitPath(const char* path, Drive** drive, const char** name) {
    if (path == nullptr) {
        return false;
    }
    const char* colon = std::strchr(path, ':');
    if (colon != nullptr) {
        *drive = FindDrive(path, static_cast<size_t>(colon - path));
        path = colon + 1;
    } else {
        *drive = sCurrentDrive;
    }
    while (*path == '/' || *path == '\\') {
        path++;
    }
    if (*drive == nullptr || std::strlen(path) >= kNameSize || std::strchr(path, '/') != nullptr) {
        return false;
    }
    *name = path;
    return true;
}

// '*' matches any run of characters, '?' one; no case.
bool Match(const char* pattern, const char* name) {
    if (*pattern == '\0') {
        return *name == '\0';
    }
    if (*pattern == '*') {
        for (const char* rest = name;; rest++) {
            if (Match(pattern + 1, rest)) {
                return true;
            }
            if (*rest == '\0') {
                return false;
            }
        }
    }
    if (*name == '\0') {
        return false;
    }
    char x = *pattern >= 'A' && *pattern <= 'Z' ? static_cast<char>(*pattern + 32) : *pattern;
    char y = *name >= 'A' && *name <= 'Z' ? static_cast<char>(*name + 32) : *name;
    if (*pattern != '?' && x != y) {
        return false;
    }
    return Match(pattern + 1, name + 1);
}

Drive* NewDrive(const char* name) {
    if (name == nullptr || name[0] == '\0' || std::strlen(name) >= sizeof(sDrives[0].name) || FindDrive(name)) {
        return nullptr;
    }
    for (Drive& drive : sDrives) {
        if (!drive.used) {
            std::memset(&drive, 0, sizeof(drive));
            drive.used = true;
            std::strcpy(drive.name, name);
            return &drive;
        }
    }
    return nullptr;
}

// Writes the image of a NAND drive back to its file.
s32 Flush(Drive* drive) {
    if (!drive->fromNand || !drive->dirty) {
        return 0;
    }
    NANDFileInfo info;
    if (NANDOpen(drive->nandPath, &info, NAND_ACCESS_WRITE) != NAND_RESULT_OK) {
        return kErrIO;
    }
    s32 written = NANDWrite(&info, drive->image, drive->nandSize);
    s32 closed = NANDClose(&info);
    if (written != static_cast<s32>(drive->nandSize) || closed != NAND_RESULT_OK) {
        return kErrIO;
    }
    drive->dirty = false;
    return 0;
}

s32 SearchNext(FindState* state) {
    ImageHeader* header = Header(state->drive->image);
    if (header == nullptr) {
        return kErrNoFileSystem;
    }
    ImageEntry* entries = Entries(header);
    while (state->next < header->count) {
        ImageEntry* entry = &entries[state->next++];
        if (Match(state->pattern, entry->name)) {
            std::memcpy(state->name, entry->name, kNameSize);
            state->size = entry->size;
            return 0;
        }
    }
    return kErrNoEntry;
}

OpenFile* CheckFile(void* file) {
    OpenFile* f = static_cast<OpenFile*>(file);
    if (f < sFiles || f >= sFiles + sizeof(sFiles) / sizeof(sFiles[0]) || !f->used || !f->drive->used) {
        return nullptr;
    }
    return f;
}

} // namespace

s32 VFIsAvailable() {
    return 1;
}

extern "C" {

void VFInit(void) {}

void VFInitEx(void* /*heap*/, u32 /*size*/) {}

// Creates an empty archive in `memory`.
s32 VFCreateSystemFileRAM(void* memory, u32 size) {
    Lock lock;
    if (!Format(static_cast<u8*>(memory), size)) {
        return Fail(kErrInvalid);
    }
    return 0;
}

// Mounts the archive at `memory`. The game mounts first and reads the NAND file
// into the block afterwards, so the contents are not looked at here.
s32 VFMountDriveRAM(const char* driveName, void* memory) {
    Lock lock;
    if (memory == nullptr) {
        return Fail(kErrInvalid);
    }
    Drive* drive = NewDrive(driveName);
    if (drive == nullptr) {
        return Fail(kErrInvalid);
    }
    drive->image = static_cast<u8*>(memory);
    sCurrentDrive = drive;
    return 0;
}

// Mounts the archive in a NAND file. The file is read whole and written back
// when the drive is synchronised or unmounted, if something changed.
s32 VFMountDriveNANDFlash(const char* driveName, const char* systemFileName) {
    Lock lock;
    if (systemFileName == nullptr || std::strlen(systemFileName) >= NAND_MAX_PATH || driveName == nullptr ||
        FindDrive(driveName)) {
        return Fail(kErrInvalid);
    }
    NANDFileInfo info;
    s32 result = NANDOpen(systemFileName, &info, NAND_ACCESS_READ);
    if (result != NAND_RESULT_OK) {
        return Fail(result == NAND_RESULT_NOEXISTS ? kErrNoEntry : kErrIO);
    }
    u32 length = 0;
    u8* image = nullptr;
    bool ok = NANDGetLength(&info, &length) == NAND_RESULT_OK && length >= kMinImageSize &&
              (image = static_cast<u8*>(std::malloc(length))) != nullptr &&
              NANDRead(&info, image, length) == static_cast<s32>(length);
    NANDClose(&info);
    ImageHeader* header = ok ? Header(image) : nullptr;
    if (header == nullptr || header->imageSize != length) {
        std::free(image);
        return Fail(kErrNoFileSystem);
    }
    Drive* drive = NewDrive(driveName);
    if (drive == nullptr) {
        std::free(image);
        return Fail(kErrInvalid);
    }
    drive->image = image;
    drive->fromNand = true;
    drive->nandSize = length;
    std::strcpy(drive->nandPath, systemFileName);
    sCurrentDrive = drive;
    return 0;
}

s32 VFMountDriveNANDFlashEx(const char* driveName, const char* systemFileName) {
    return VFMountDriveNANDFlash(driveName, systemFileName);
}

s32 VFUnmountDrive(const char* driveName) {
    Lock lock;
    Drive* drive = FindDrive(driveName);
    if (drive == nullptr) {
        return Fail(kErrInvalid);
    }
    for (OpenFile& file : sFiles) {
        if (file.used && file.drive == drive) {
            std::free(file.buffer);
            file.used = false;
        }
    }
    s32 result = Flush(drive);
    if (drive->fromNand) {
        std::free(drive->image);
    }
    drive->used = false;
    if (sCurrentDrive == drive) {
        sCurrentDrive = nullptr;
        for (Drive& other : sDrives) {
            if (other.used) {
                sCurrentDrive = &other;
            }
        }
    }
    return result != 0 ? Fail(result) : 0;
}

s32 VFSyncDrive(const char* driveName, u32 /*mode*/) {
    Lock lock;
    Drive* drive = FindDrive(driveName);
    if (drive == nullptr) {
        return Fail(kErrInvalid);
    }
    s32 result = Flush(drive);
    return result != 0 ? Fail(result) : 0;
}

s32 VFSetSyncMode(const char* driveName, u32 /*mode*/) {
    Lock lock;
    return FindDrive(driveName) != nullptr ? 0 : Fail(kErrInvalid);
}

// Modes: "r" reads an existing file; "w" creates or replaces one, and the
// contents reach the archive when the file is closed.
void* VFOpenFile(const char* path, const char* mode, u32 /*attr*/) {
    Lock lock;
    Drive* drive;
    const char* name;
    if (mode == nullptr || !SplitPath(path, &drive, &name) || name[0] == '\0') {
        Fail(kErrInvalid);
        return nullptr;
    }
    bool writing = mode[0] == 'w';
    if (!writing && mode[0] != 'r') {
        Fail(kErrInvalid);
        return nullptr;
    }
    ImageHeader* header = Header(drive->image);
    if (header == nullptr) {
        Fail(kErrNoFileSystem);
        return nullptr;
    }
    if (!writing && FindEntry(header, name) == nullptr) {
        Fail(kErrNoEntry);
        return nullptr;
    }
    for (OpenFile& file : sFiles) {
        if (!file.used) {
            std::memset(&file, 0, sizeof(file));
            file.used = true;
            file.drive = drive;
            std::strcpy(file.name, name);
            file.writing = writing;
            return &file;
        }
    }
    Fail(kErrTooManyOpen);
    return nullptr;
}

s32 VFCloseFile(void* file) {
    Lock lock;
    OpenFile* f = CheckFile(file);
    if (f == nullptr) {
        return Fail(kErrBadFile);
    }
    s32 result = 0;
    if (f->writing) {
        ImageHeader* header = Header(f->drive->image);
        if (header == nullptr) {
            result = kErrNoFileSystem;
        } else {
            if (ImageEntry* old = FindEntry(header, f->name)) {
                RemoveEntry(header, old);
            }
            if (header->count == header->capacity || f->length > header->imageSize - header->dataEnd) {
                result = kErrNoSpace;
            } else {
                ImageEntry* entry = &Entries(header)[header->count++];
                std::memset(entry, 0, sizeof(*entry));
                std::strcpy(entry->name, f->name);
                entry->offset = header->dataEnd;
                entry->size = f->length;
                if (f->length != 0) {
                    std::memcpy(f->drive->image + entry->offset, f->buffer, f->length);
                }
                header->dataEnd += f->length;
            }
            f->drive->dirty = true;
        }
        std::free(f->buffer);
    }
    f->used = false;
    return result != 0 ? Fail(result) : 0;
}

s32 VFGetFileSizeByFd(void* file) {
    Lock lock;
    OpenFile* f = CheckFile(file);
    if (f == nullptr) {
        return 0;
    }
    if (f->writing) {
        return static_cast<s32>(f->length);
    }
    ImageHeader* header = Header(f->drive->image);
    ImageEntry* entry = header != nullptr ? FindEntry(header, f->name) : nullptr;
    return entry != nullptr ? static_cast<s32>(entry->size) : 0;
}

// Reads up to `size` bytes. Reading less than was asked for (the end of the
// file) is not an error; *readSize says how much there was.
s32 VFReadFile(void* file, void* buf, u32 size, u32* readSize) {
    Lock lock;
    if (readSize != nullptr) {
        *readSize = 0;
    }
    OpenFile* f = CheckFile(file);
    if (f == nullptr || f->writing || buf == nullptr) {
        return Fail(kErrBadFile);
    }
    ImageHeader* header = Header(f->drive->image);
    if (header == nullptr) {
        return Fail(kErrNoFileSystem);
    }
    ImageEntry* entry = FindEntry(header, f->name);
    if (entry == nullptr || entry->offset > header->dataEnd || entry->size > header->dataEnd - entry->offset) {
        return Fail(kErrIO);
    }
    u32 left = f->position < entry->size ? entry->size - f->position : 0;
    u32 count = size < left ? size : left;
    std::memcpy(buf, f->drive->image + entry->offset + f->position, count);
    f->position += count;
    if (readSize != nullptr) {
        *readSize = count;
    }
    return 0;
}

s32 VFWriteFile(void* file, void* buf, u32 size) {
    Lock lock;
    OpenFile* f = CheckFile(file);
    if (f == nullptr || !f->writing || (buf == nullptr && size != 0)) {
        return Fail(kErrBadFile);
    }
    if (size > f->capacity - f->length) {
        u32 capacity = f->capacity != 0 ? f->capacity : 0x10000;
        while (capacity - f->length < size) {
            if (capacity >= 0x40000000) {
                return Fail(kErrNoSpace);
            }
            capacity *= 2;
        }
        u8* grown = static_cast<u8*>(std::realloc(f->buffer, capacity));
        if (grown == nullptr) {
            return Fail(kErrNoSpace);
        }
        f->buffer = grown;
        f->capacity = capacity;
    }
    if (size != 0) {
        std::memcpy(f->buffer + f->length, buf, size);
    }
    f->length += size;
    return 0;
}

// origin: 0 = start, 1 = current position, 2 = end (reading only).
s32 VFSeekFile(void* file, s32 offset, s32 origin) {
    Lock lock;
    OpenFile* f = CheckFile(file);
    if (f == nullptr || f->writing) {
        return Fail(kErrBadFile);
    }
    ImageHeader* header = Header(f->drive->image);
    ImageEntry* entry = header != nullptr ? FindEntry(header, f->name) : nullptr;
    if (entry == nullptr) {
        return Fail(kErrIO);
    }
    s64 base = origin == 0 ? 0 : origin == 1 ? static_cast<s64>(f->position) : static_cast<s64>(entry->size);
    s64 position = base + offset;
    if (origin < 0 || origin > 2 || position < 0 || position > static_cast<s64>(entry->size)) {
        return Fail(kErrInvalid);
    }
    f->position = static_cast<u32>(position);
    return 0;
}

s32 VFDeleteFile(const char* path) {
    Lock lock;
    Drive* drive;
    const char* name;
    if (!SplitPath(path, &drive, &name)) {
        return Fail(kErrInvalid);
    }
    ImageHeader* header = Header(drive->image);
    if (header == nullptr) {
        return Fail(kErrNoFileSystem);
    }
    ImageEntry* entry = FindEntry(header, name);
    if (entry == nullptr) {
        return Fail(kErrNoEntry);
    }
    RemoveEntry(header, entry);
    drive->dirty = true;
    return 0;
}

// Directory search: 0 for each entry that matches, then an error. `dta` is the
// caller's 0x448-byte buffer; the entry's name is PC-specific in there.
s32 VFFindFirst(void* dta, const char* path, u32 /*attr*/) {
    Lock lock;
    Drive* drive;
    const char* pattern;
    if (dta == nullptr || !SplitPath(path, &drive, &pattern)) {
        return Fail(kErrInvalid);
    }
    FindState* state = static_cast<FindState*>(dta);
    std::memset(state, 0, sizeof(*state));
    state->magic = kFindMagic;
    state->drive = drive;
    std::strcpy(state->pattern, pattern);
    s32 result = SearchNext(state);
    return result != 0 ? Fail(result) : 0;
}

s32 VFFindNext(void* dta) {
    Lock lock;
    FindState* state = static_cast<FindState*>(dta);
    if (state == nullptr || state->magic != kFindMagic || state->drive < sDrives ||
        state->drive >= sDrives + sizeof(sDrives) / sizeof(sDrives[0]) || !state->drive->used) {
        return Fail(kErrInvalid);
    }
    s32 result = SearchNext(state);
    return result != 0 ? Fail(result) : 0;
}

s32 VFGetLastError() {
    Lock lock;
    return sLastError;
}

s32 VFGetLastDeviceError(const char* /*drive*/) {
    return 0;
}

s32 VFGetDriveFreeSize(const char* driveName) {
    Lock lock;
    Drive* drive = FindDrive(driveName);
    ImageHeader* header = drive != nullptr ? Header(drive->image) : nullptr;
    if (header == nullptr) {
        Fail(drive != nullptr ? kErrNoFileSystem : kErrInvalid);
        return 0;
    }
    return static_cast<s32>(header->imageSize - header->dataEnd);
}

} // extern "C"
