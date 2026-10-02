#include <revolution/ncd.h>
#include <revolution/ipc.h>
#include <revolution/ipc/ipcclt.h>
#include <revolution/os.h>
#include <string.h>

// No public source; written from the DOL (names from MKW's symbol map).

const char* __NCDVersion = "<< RVL_SDK - NCD \trelease build: Jun 28 2007 18:29:29 (0x4199_60831) >>";

static u32 ncdInitFlags;
static NCDiConfig* ncdConfig;

static OSMutex ncdMutex ATTRIBUTE_ALIGN(32);
static s32 ncdResult[8] ATTRIBUTE_ALIGN(32);
static IOSIoVector ncdVec[4] ATTRIBUTE_ALIGN(32);

static s32 ExecConfigCommand(const char* funcName, void* buf, s32 cmd);
static void LockRight(void);

// Unreferenced (dead-stripped). The first function to use the .bss work
// variables fixes their order (mutex, result, vectors); LockRight comes last
// in .text, so something before it touched them first.
void NCDiClearWork(void) {
    memset(&ncdMutex, 0, sizeof(ncdMutex));
    memset(&ncdResult, 0, sizeof(ncdResult));
    memset(&ncdVec, 0, sizeof(ncdVec));
}

s32 NCDGetLinkStatus(void) {
    s32 result;
    s32 fd;

    if (OSGetCurrentThread() == NULL) {
        return -5;
    }

    LockRight();

    fd = IOS_Open("/dev/net/ncd/manage", 0);
    if (fd < 0) {
        if (fd == -6) {
            result = -8;
        } else {
            result = -2;
        }
    } else {
        ncdVec[0].base = (u8*)&ncdResult;
        ncdVec[0].length = sizeof(ncdResult);

        if (IOS_Ioctlv(fd, 7, 0, 1, ncdVec) < 0) {
            result = -2;
        } else {
            result = ncdResult[0];
            if (result == 0) {
                result = ncdResult[1];
                if (result >= 0) {
                    result = result;
                } else {
                    result = -1;
                }
            }
        }

        if (IOS_Close(fd) < 0) {
            result = -1;
        }
    }

    OSUnlockMutex(&ncdMutex);
    return result;
}

s32 NCDiGetEnabledConfigList(u32* pEnabled, u32* pWireless, u32* pWired) {
    s32 result;
    u32 enabled = 0;
    u32 wireless = 0;
    u32 wired = 0;
    s32 i;

    LockRight();

    result = ExecConfigCommand("NCDiGetEnabledConfigList", NULL, 3);
    if (result == 0) {
        for (i = 0; i < NCD_CONFIG_COUNT; i++) {
            NCDiConfigEntry* entry = &ncdConfig->entries[i];

            if (entry->flags & 0x80) {
                if (entry->flags & 1) {
                    enabled |= 1 << i;
                } else {
                    if (entry->type != 1) {
                        wireless |= 1 << i;
                    }
                    if (entry->type == 1) {
                        wired |= 1 << i;
                    }
                }
            }
        }
    }

    OSUnlockMutex(&ncdMutex);

    if (pEnabled != NULL) {
        *pEnabled = enabled;
    }
    if (pWireless != NULL) {
        *pWireless = wireless;
    }
    if (pWired != NULL) {
        *pWired = wired;
    }

    return result;
}

static s32 ExecConfigCommand(const char* funcName, void* buf, s32 cmd) {
    s32 result = 0;
    s32 fd;
#pragma unused(funcName)

    if (OSGetCurrentThread() == NULL) {
        return -5;
    }

    LockRight();

    fd = IOS_Open("/dev/net/ncd/manage", 0);
    if (fd < 0) {
        if (fd == -6) {
            result = -8;
        } else {
            result = -2;
        }
    } else {
        ncdVec[0].base = (u8*)ncdConfig;
        ncdVec[0].length = sizeof(NCDiConfig);
        ncdVec[1].base = (u8*)&ncdResult;
        ncdVec[1].length = sizeof(ncdResult);

        switch (cmd) {
        case 3:
        case 5:
            if (IOS_Ioctlv(fd, cmd, 0, 2, ncdVec) < 0) {
                result = -2;
            } else {
                result = ncdResult[0];
                if (result == 0 && buf != (void*)NULL) {
                    memcpy(buf, ncdConfig, sizeof(NCDiConfig));
                }
            }
            break;
        case 4:
        case 6:
            if (buf != (void*)NULL) {
                memcpy(ncdConfig, buf, sizeof(NCDiConfig));
            }
            if (IOS_Ioctlv(fd, cmd, 1, 1, ncdVec) < 0) {
                result = -2;
            } else {
                result = ncdResult[0];
            }
            break;
        }

        if (IOS_Close(fd) < 0) {
            result = -1;
        }
    }

    OSUnlockMutex(&ncdMutex);
    return result;
}

static void LockRight(void) {
    BOOL enabled = OSDisableInterrupts();

    if (!(ncdInitFlags & 1)) {
        u8* lo;

        OSRegisterVersion(__NCDVersion);
        OSInitMutex(&ncdMutex);

        lo = (u8*)OSRoundUp32B(IPCGetBufferLo());
        if ((u32)IPCGetBufferHi() - (u32)lo < NCD_CONFIG_HEAP_SIZE) {
            OSPanic("ncdsystem.c", 1457, "Could not reserve heap for NCD library from IPC arena");
        }
        IPCSetBufferLo(lo + NCD_CONFIG_HEAP_SIZE);

        ncdConfig = (NCDiConfig*)lo;
        memset(lo, 0, NCD_CONFIG_HEAP_SIZE);
        memset(&ncdResult, 0, sizeof(ncdResult));
        memset(&ncdVec, 0, sizeof(ncdVec));

        ncdInitFlags |= 1;
    }

    OSRestoreInterrupts(enabled);
    OSLockMutex(&ncdMutex);
}
