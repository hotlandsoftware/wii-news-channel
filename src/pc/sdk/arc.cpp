// ARC: U8 archives in memory (src/revolution/ARC/arc.c).
//
// The functions are the SDK's. PC additions (docs/pc_port.md, "Byte order"):
//
//   - ARCInitHandle() converts the archive's header and node table to host
//     byte order first (PCEndianSwapU8Archive, below). The archive's magic
//     records that this has happened, so a second handle on the same buffer
//     converts nothing.
//   - ARCGetStartAddrInMem() converts the member it returns if it is a file
//     of a known format (a layout, an animation, a font, a texture palette):
//     this is where nw4r::lyt::ArcResourceAccessor gets its resources.
//     Members are converted on first use, not when the archive is opened,
//     because an archive handle may cover only the node table (CNT reads just
//     that much of a content file).
//   - name comparison uses an ASCII tolower() instead of MSL's locale table.

#include <revolution/arc.h>
#include <revolution/os.h>

#include <pc/endian.h>

namespace {

struct FSTEntry {
    unsigned int isDirAndStringOff;
    unsigned int parentOrPosition;
    unsigned int nextEntryOrLength;
};

const u32 ARC_MAGIC = 0x55AA382D;

inline int ToLower(int c) {
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

} // namespace

#define entryIsDir(fstStart, i) ((((fstStart)[i].isDirAndStringOff & 0xFF000000) == 0) ? FALSE : TRUE)
#define stringOff(fstStart, i) ((fstStart)[i].isDirAndStringOff & 0x00FFFFFF)
#define parentDir(fstStart, i) ((fstStart)[i].parentOrPosition)
#define nextDir(fstStart, i) ((fstStart)[i].nextEntryOrLength)
#define filePosition(fstStart, i) ((fstStart)[i].parentOrPosition)
#define fileLength(fstStart, i) ((fstStart)[i].nextEntryOrLength)

extern "C" {

// Converts the header and the node table (FST) of a big-endian U8 archive.
// `size` is the number of valid bytes at `data` (at least the header and the
// node table), or 0xFFFFFFFF if unknown. The members are not touched.
BOOL PCEndianSwapU8Archive(void* data, u32 size) {
    u8* base = static_cast<u8*>(data);
    if (size < sizeof(ARCHeader)) {
        return FALSE;
    }

    // Validate on the big-endian data before changing anything.
    const u32 fstStart = PCReadBE32(base + 4);
    const u32 fstSize = PCReadBE32(base + 8);
    if (fstStart < sizeof(ARCHeader) || fstSize < sizeof(FSTEntry) || fstStart > size ||
        fstSize > size - fstStart) {
        return FALSE;
    }
    const u32 entryNum = PCReadBE32(base + fstStart + 8); // root: nextEntryOrLength
    if (entryNum == 0 || entryNum > fstSize / sizeof(FSTEntry)) {
        return FALSE;
    }

    ARCHeader* header = static_cast<ARCHeader*>(data);
    PCEndianSwap(header->magic);
    PCEndianSwap(header->fstStart);
    PCEndianSwap(header->fstSize);
    PCEndianSwap(header->fileStart);
    // pad[4]: not read

    FSTEntry* entries = reinterpret_cast<FSTEntry*>(base + fstStart);
    for (u32 i = 0; i < entryNum; i++) {
        PCEndianSwap(entries[i].isDirAndStringOff); // type in the top byte, name offset below
        PCEndianSwap(entries[i].parentOrPosition);
        PCEndianSwap(entries[i].nextEntryOrLength);
    }
    // The names that follow are chars.
    return TRUE;
}

BOOL ARCInitHandle(void* arcStart, ARCHandle* handle) {
    FSTEntry* FSTEntries;
    ARCHeader* arcHeader = (ARCHeader*)arcStart;

    PCEndianFixFile(arcStart, 0xFFFFFFFFu);

    if (arcHeader->magic != ARC_MAGIC) {
        OSPanic(__FILE__, 0x4A, "ARCInitHandle: bad archive format");
        return FALSE; // (OSPanic does not return on the Wii)
    }

    handle->archiveStartAddr = arcStart;
    handle->FSTStart = FSTEntries = (FSTEntry*)((u32)arcStart + arcHeader->fstStart);
    handle->fileStart = (void*)((u32)arcStart + arcHeader->fileStart);
    handle->entryNum = nextDir(FSTEntries, 0);
    handle->FSTStringStart = (char*)&(FSTEntries[handle->entryNum]);
    handle->FSTLength = (u32)arcHeader->fstSize;
    handle->currDir = 0;
    return TRUE;
}

BOOL ARCOpen(ARCHandle* handle, const char* fileName, ARCFileInfo* af) {
    s32 entry;
    char currentDir[128];
    FSTEntry* FSTEntries = (FSTEntry*)handle->FSTStart;
    entry = ARCConvertPathToEntrynum(handle, fileName);

    if (0 > entry) {
        ARCGetCurrentDir(handle, currentDir, 128);
        OSReport("Warning: ARCOpen(): file '%s' was not found under %s in the archive.\n", fileName,
                 currentDir);
        return FALSE;
    }

    if ((entry < 0) || entryIsDir(FSTEntries, entry)) {
        return FALSE;
    }

    af->handle = handle;
    af->startOffset = filePosition(FSTEntries, entry);
    af->length = fileLength(FSTEntries, entry);

    return TRUE;
}

BOOL ARCFastOpen(ARCHandle* handle, s32 entrynum, ARCFileInfo* af) {
    FSTEntry* FSTEntries = (FSTEntry*)handle->FSTStart;

    if ((entrynum < 0) || (entrynum >= (s32)handle->entryNum) || entryIsDir(FSTEntries, entrynum)) {
        return FALSE;
    }

    af->handle = handle;
    af->startOffset = filePosition(FSTEntries, entrynum);
    af->length = fileLength(FSTEntries, entrynum);
    return TRUE;
}

static BOOL isSame(const char* path, const char* string) {
    while (*string != '\0') {
        if (ToLower((unsigned char)*path++) != ToLower((unsigned char)*string++)) {
            return FALSE;
        }
    }

    if ((*path == '/') || (*path == '\0')) {
        return TRUE;
    }

    return FALSE;
}

s32 ARCConvertPathToEntrynum(ARCHandle* handle, const char* pathPtr) {
    const char* ptr;
    char* stringPtr;
    BOOL isDir;
    s32 length;
    u32 dirLookAt;
    u32 i;
    FSTEntry* FSTEntries;

    dirLookAt = handle->currDir;
    FSTEntries = (FSTEntry*)handle->FSTStart;

    while (1) {
        if (*pathPtr == '\0') {
            return (s32)dirLookAt;
        } else if (*pathPtr == '/') {
            dirLookAt = 0;
            pathPtr++;
            continue;
        } else if (*pathPtr == '.') {
            if (*(pathPtr + 1) == '.') {
                if (*(pathPtr + 2) == '/') {
                    dirLookAt = parentDir(FSTEntries, dirLookAt);
                    pathPtr += 3;
                    continue;
                } else if (*(pathPtr + 2) == '\0') {
                    return (s32)parentDir(FSTEntries, dirLookAt);
                }
            } else if (*(pathPtr + 1) == '/') {
                pathPtr += 2;
                continue;
            } else if (*(pathPtr + 1) == '\0') {
                return (s32)dirLookAt;
            }
        }

        for (ptr = pathPtr; (*ptr != '\0') && (*ptr != '/'); ptr++)
            ;
        isDir = (*ptr == '\0') ? FALSE : TRUE;
        length = (s32)(ptr - pathPtr);
        ptr = pathPtr;

        for (i = dirLookAt + 1; i < nextDir(FSTEntries, dirLookAt);
             i = entryIsDir(FSTEntries, i) ? nextDir(FSTEntries, i) : (i + 1)) {
        dot:
            if ((entryIsDir(FSTEntries, i) == FALSE) && (isDir == TRUE)) {
                continue;
            }

            stringPtr = handle->FSTStringStart + stringOff(FSTEntries, i);

            if (*stringPtr == '.' && *(stringPtr + 1) == '\0') {
                i++;
                goto dot;
            }

            if (isSame(ptr, stringPtr) == TRUE) {
                goto next_hier;
            }
        }

        return -1;

    next_hier:
        if (!isDir) {
            return (s32)i;
        }

        dirLookAt = i;
        pathPtr += length + 1;
    }

    // the world ends if this is reached
}

BOOL ARCEntrynumIsDir(ARCHandle* handle, s32 entrynum) {
    FSTEntry* FSTEntries = (FSTEntry*)handle->FSTStart;
    return entryIsDir(FSTEntries, entrynum);
}

static u32 myStrncpy(char* dest, char* src, u32 maxlen) {
    u32 i = maxlen;

    while ((i > 0) && (*src != 0)) {
        *dest++ = *src++;
        i--;
    }

    return (maxlen - i);
}

static u32 entryToPath(ARCHandle* handle, u32 entry, char* path, u32 maxlen) {
    char* name;
    u32 loc;
    FSTEntry* FSTEntries = (FSTEntry*)handle->FSTStart;

    if (entry == 0) {
        return 0;
    }

    name = handle->FSTStringStart + stringOff(FSTEntries, entry);
    loc = entryToPath(handle, parentDir(FSTEntries, entry), path, maxlen);

    if (loc == maxlen) {
        return loc;
    }

    *(path + loc++) = '/';
    loc += myStrncpy(path + loc, name, maxlen - loc);
    return loc;
}

static BOOL ARCConvertEntrynumToPath(ARCHandle* handle, s32 entrynum, char* path, u32 maxlen) {
    u32 loc;
    FSTEntry* FSTEntries = (FSTEntry*)handle->FSTStart;

    loc = entryToPath(handle, (u32)entrynum, path, maxlen);

    if (loc == maxlen) {
        path[maxlen - 1] = '\0';
        return FALSE;
    }

    if (entryIsDir(FSTEntries, entrynum)) {
        if (loc == maxlen - 1) {
            path[loc] = '\0';
            return FALSE;
        }

        path[loc++] = '/';
    }

    path[loc] = '\0';
    return TRUE;
}

BOOL ARCGetCurrentDir(ARCHandle* handle, char* path, u32 maxlen) {
    return ARCConvertEntrynumToPath(handle, (s32)handle->currDir, path, maxlen);
}

void* ARCGetStartAddrInMem(ARCFileInfo* af) {
    ARCHandle* handle = af->handle;
    void* addr = (void*)((u32)handle->archiveStartAddr + af->startOffset);
    // PC: a member is converted to host byte order the first time it is used.
    PCEndianFixFile(addr, af->length);
    return addr;
}

u32 ARCGetStartOffset(ARCFileInfo* af) {
    return af->startOffset;
}

u32 ARCGetLength(ARCFileInfo* af) {
    return af->length;
}

BOOL ARCClose(ARCFileInfo* af) {
    (void)af;
    return TRUE;
}

BOOL ARCChangeDir(ARCHandle* handle, const char* dirName) {
    s32 entry;
    FSTEntry* FSTEntries;

    entry = ARCConvertPathToEntrynum(handle, dirName);
    FSTEntries = (FSTEntry*)handle->FSTStart;

    if ((entry < 0) || (entryIsDir(FSTEntries, entry) == FALSE)) {
        return FALSE;
    }

    handle->currDir = (u32)entry;
    return TRUE;
}

BOOL ARCOpenDir(ARCHandle* handle, const char* dirName, ARCDir* dir) {
    s32 entry;
    FSTEntry* FSTEntries;

    entry = ARCConvertPathToEntrynum(handle, dirName);
    FSTEntries = (FSTEntry*)handle->FSTStart;

    if ((entry < 0) || (entryIsDir(FSTEntries, entry) == FALSE)) {
        return FALSE;
    }

    dir->handle = handle;
    dir->entryNum = (u32)entry;
    dir->location = (u32)entry + 1;
    dir->next = nextDir(FSTEntries, entry);
    return TRUE;
}

BOOL ARCReadDir(ARCDir* dir, ARCDirEntry* dirent) {
    u32 loc;
    FSTEntry* FSTEntries;
    ARCHandle* handle;

    handle = dir->handle;
    FSTEntries = (FSTEntry*)handle->FSTStart;
    loc = dir->location;
retry:
    if ((loc <= dir->entryNum) || (dir->next <= loc)) {
        return FALSE;
    }

    dirent->handle = handle;
    dirent->entryNum = loc;
    dirent->isDir = entryIsDir(FSTEntries, loc);
    dirent->name = handle->FSTStringStart + stringOff(FSTEntries, loc);

    if (dirent->name[0] == '.' && dirent->name[1] == '\0') {
        loc++;
        goto retry;
    }

    dir->location = entryIsDir(FSTEntries, loc) ? nextDir(FSTEntries, loc) : (loc + 1);
    return TRUE;
}

BOOL ARCCloseDir(ARCDir* dir) {
    (void)dir;
    return TRUE;
}

} // extern "C"
