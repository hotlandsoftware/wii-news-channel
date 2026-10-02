#include <revolution/cnt.h>
#include <revolution/esp.h>
#include <revolution/mem/allocator.h>
#include <revolution/nand/nandlogging.h>
#include <revolution/os.h>
#include <stdio.h>
#include <string.h>

const char* __CNTVersion = "<< RVL_SDK - CNT \trelease build: May 10 2007 19:28:13 (0x4199_60831) >>";

static BOOL __CNTInitialized = FALSE;

static s32 __CNTConvertErrorCode(s32 error);

void CNTInit(void) {
    BOOL enabled;

    if (!__CNTInitialized) {
        ESP_InitLib();
        enabled = OSDisableInterrupts();
        __CNTInitialized = TRUE;
        OSRestoreInterrupts(enabled);
        OSRegisterVersion(__CNTVersion);
    }
}

s32 CNTShutdown(void) {
    return ESP_CloseLib();
}

s32 contentInitHandleNAND(s32 contentNum, CNTHandle* handle, MEMAllocator* allocator) {
    ARCHeader header ATTRIBUTE_ALIGN(32);
    ARCHandle arcHandle;
    void* buffer;
    u32 size;
    s32 result;
    s32 fd;

    fd = ESP_OpenContentFile(contentNum);
    if (fd < 0) {
        return __CNTConvertErrorCode(fd);
    }

    result = ESP_ReadContentFile(fd, &header, sizeof(ARCHeader));
    if (result < 0) {
        return __CNTConvertErrorCode(result);
    }

    size = OSRoundUp32B(header.fileStart);

    result = ESP_SeekContentFile(fd, 0, 0);
    if (result < 0) {
        return __CNTConvertErrorCode(result);
    }

    buffer = MEMAllocFromAllocator(allocator, size);
    if (buffer == (void*)NULL) {
        return -0x1389;
    }

    result = ESP_ReadContentFile(fd, buffer, size);
    if (result < 0) {
        MEMFreeToAllocator(allocator, buffer);
        return __CNTConvertErrorCode(result);
    }

    ARCInitHandle(buffer, &arcHandle);
    memcpy(&handle->arcHandle, &arcHandle, sizeof(ARCHandle));
    handle->fd = fd;
    handle->allocator = allocator;
    return 0;
}

// Not linked: only its strings remain in the DOL ("/content%d" and the warning).
s32 contentInitHandleDVD(s32 contentNum, CNTHandle* handle, MEMAllocator* allocator) {
    char path[32];
#pragma unused(handle, allocator)

    sprintf(path, "/content%d", contentNum);
    OSReport("Warning: CNTInitHandle(): directory '%s' is not found under '/'\n", path);
    return -1;
}

s32 contentOpenNAND(CNTHandle* handle, const char* path, CNTFileInfo* info) {
    ARCFileInfo arcInfo;
    s32 entrynum;

    entrynum = ARCConvertPathToEntrynum(&handle->arcHandle, path);
    if (entrynum < 0) {
        return -0x1391;
    }

    if (!ARCFastOpen(&handle->arcHandle, entrynum, &arcInfo)) {
        return -0x1391;
    }

    info->handle = handle;
    info->offset = arcInfo.startOffset;
    info->length = arcInfo.length;
    info->position = 0;
    return 0;
}

s32 contentFastOpenNAND(CNTHandle* handle, s32 entrynum, CNTFileInfo* info) {
    ARCFileInfo arcInfo;

    if (!ARCFastOpen(&handle->arcHandle, entrynum, &arcInfo)) {
        return -0x1391;
    }

    info->handle = handle;
    info->offset = arcInfo.startOffset;
    info->length = arcInfo.length;
    info->position = 0;
    return 0;
}

s32 contentConvertPathToEntrynumNAND(CNTHandle* handle, const char* path) {
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
        return -0x1391;
    }

    if (pos < 0 || pos > info->length) {
        return -0x1391;
    }

    info->position = pos;
    return 0;
}

s32 contentReadNAND(CNTFileInfo* info, void* dst, u32 len, s32 offset) {
    s32 result;
    s32 pos = info->position + offset;

    if (pos < 0 || pos > info->length) {
        return -0x1391;
    }

    result = ESP_SeekContentFile(info->handle->fd, info->offset + pos, 0);
    if (result < 0) {
        return __CNTConvertErrorCode(result);
    }

    return __CNTConvertErrorCode(ESP_ReadContentFile(info->handle->fd, dst, len));
}

s32 contentCloseNAND(CNTFileInfo* info) {
#pragma unused(info)
    return 0;
}

s32 contentReleaseHandleNAND(CNTHandle* handle) {
    MEMFreeToAllocator(handle->allocator, handle->arcHandle.archiveStartAddr);
    return __CNTConvertErrorCode(ESP_CloseContentFile(handle->fd));
}

BOOL contentOpenDirNAND(CNTHandle* handle, const char* path, ARCDir* dir) {
    return ARCOpenDir(&handle->arcHandle, path, dir);
}

static s32 __CNTConvertErrorCode(s32 error) {
    int i;

    const s32 errorMap[] = {
        0x0000, 0x0000, -0x03E9, -0x13C7, -0x03EA, -0x13C7, -0x03EB, -0x13C7,
        -0x03EC, -0x13C7, -0x03ED, -0x13C7, -0x03EE, -0x13C7, -0x03EF, -0x13C7,
        -0x03F0, -0x13C7, -0x03F1, -0x13C7, -0x03F2, -0x13C7, -0x03F3, -0x13C7,
        -0x03F4, -0x13C7, -0x03F5, -0x13C7, -0x03F6, -0x13C7, -0x03F7, -0x13C7,
        -0x03F8, -0x1388, -0x03F9, -0x1391, -0x03FA, -0x13C7, -0x03FB, -0x13C7,
        -0x03FC, -0x13C7, -0x03FD, -0x13C7, -0x03FE, -0x13C7, -0x03FF, -0x13C7,
        -0x0400, -0x1390, -0x0401, -0x13C7, -0x0402, -0x1392, -0x0403, -0x13C7,
        -0x0404, -0x13C7, -0x0405, -0x13C7, -0x0406, -0x13C7, -0x0407, -0x13C7,
        -0x0408, -0x13C7, -0x0409, -0x13C7, -0x040A, -0x13C7, -0x040B, -0x13C7,
        -0x040C, -0x13C7, -0x040D, -0x13C7, -0x040E, -0x13C7, 0x0000, 0x0000,
        -0x0066, -0x1392, -0x0067, -0x1393, -0x0072, -0x1394, -0x0069, -0x13C7,
        -0x0074, -0x1395, -0x0065, -0x1391, -0x006C, -0x13C7, -0x006D, -0x1388,
        -0x006B, -0x13C7, -0x006A, -0x1391, -0x0073, -0x13C7, -0x0068, -0x13C7,
        -0x006F, -0x13C7, -0x0075, -0x13C7, -0x0076, -0x1390, -0x0077, -0x1407,
        -0x0001, -0x1392, -0x0002, -0x13C7, -0x0003, -0x13C7, -0x0004, -0x1391,
        -0x0005, -0x13C7, -0x0006, -0x1391, -0x0007, -0x13C7, -0x0008, -0x1390,
        -0x0009, -0x13C7, -0x000A, -0x13C7, -0x000B, -0x13C7, -0x000C, -0x1394,
        -0x000D, -0x13C7, -0x000E, -0x13C7, -0x000F, -0x13C7, -0x0010, -0x13C7,
        -0x0011, -0x13C7, -0x0012, -0x13C7, -0x0013, -0x13C7, -0x0014, -0x13C7,
        -0x0015, -0x13C7, -0x0016, -0x1390, -0x0017, -0x13C7,
    };

    i = 0;

    if (error >= 0) {
        return error;
    }

    for (; i < ARRAY_SIZE(errorMap); i += 2) {
        if (error == errorMap[i]) {
            if (error == -0x72 || error == -0x74 || error == -0x75 || error == -0x9 || error == -0xC) {
                char msg[128] ATTRIBUTE_ALIGN(64);
                sprintf(msg, "ES error code: %d", error);
                NANDLoggingAddMessageAsync(NULL, msg);
            }
            return errorMap[i + 1];
        }
    }

    OSReport("CAUTION!  Unexpected error code [%d] was found.\n", error);
    {
        char msg[128] ATTRIBUTE_ALIGN(64);
        sprintf(msg, "ES unexpected error code: %d", error);
        NANDLoggingAddMessageAsync(NULL, msg);
    }
    return -0x13C7;
}
