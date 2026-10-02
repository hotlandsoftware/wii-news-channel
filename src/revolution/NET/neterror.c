#include <revolution/ncd.h>
#include <revolution/os.h>
#include <revolution/so.h>

// No public source; written from the DOL (names from MKW's symbol map).

#define NET_CONNECTION_TYPE_NONE 99

static inline int GetFirstBitIndex(u32 list) {
    int i;
    u32 mask = 1;

    for (i = 0; i < 32; i++) {
        if (list & mask) {
            return i;
        }
        mask <<= 1;
    }

    return -1;
}

int NETiGetConnectionTypeFromConfigList(u32 enabled, u32 wireless, u32 wired) {
    int type = NET_CONNECTION_TYPE_NONE;

    if (enabled != 0) {
        if (wireless == 0 && wired == 0) {
            type = GetFirstBitIndex(enabled) + 20;
        }
    } else if (wireless != 0) {
        if (wired == 0) {
            type = GetFirstBitIndex(wireless) + 30;
        }
    } else if (wired != 0) {
        type = GetFirstBitIndex(wired) + 40;
    }

    return type;
}

static int GetStartupErrorCode(int err, int type);

int NETGetStartupErrorCode(int err) {
    u32 enabled;
    u32 wireless;
    u32 wired;
    int type = NET_CONNECTION_TYPE_NONE;

    if (NCDiGetEnabledConfigList(&enabled, &wireless, &wired) >= 0) {
        type = NETiGetConnectionTypeFromConfigList(enabled, wireless, wired);
    }

    if (type < 0) {
        err = SO_EFATAL;
        type = NET_CONNECTION_TYPE_NONE;
    }

    return GetStartupErrorCode(err, type) - type;
}

static int GetStartupErrorCode(int err, int type) {
    if (err >= 0) {
        return 0;
    }

    switch (err) {
    case -45:
        return -50200;
    case -28:
        return -50300;
    case -62:
        return -50400;
    case -111:
        return -52700;
    case -121:
        if (type >= 20 && type < 30) {
            return -51400;
        }
        return -51000;
    case -112:
    case -76:
    case -39:
    case -48:
        if (type >= 20 && type < 30) {
            return -51400;
        }
        return -51300;
    case -102:
    case -101:
    case -100:
        return -52000;
    case SO_EFATAL:
        return -50100;
    default:
        OSReport("Unknown SOStartup Error: %d\n", err);
        return -50100;
    }
}
