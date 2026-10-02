#include <revolution/so.h>
#include <revolution/ipc/ipcclt.h>
#include <revolution/os.h>
#include <string.h>

#define SO_OPTLEN(p) (((p) == NULL || *(p) < 0) ? 0 : *(p))

long SOGetHostID(void) {
    s32 rmId;
    int result;
    long hostId = 0;

    result = SOiPrepare(NULL, &rmId);
    if (result == SO_SUCCESS) {
        hostId = IOS_Ioctl(rmId, 0x10, NULL, 0, NULL, 0);
        SOiConclude(NULL, result);
    }

    return hostId;
}

int SOGetInterfaceOpt(IPInterface* interface, int level, int optname, void* optval, int* optlen) {
    u32 bufSize;
    u8* buf;
    s32 rmId;
    int result;
    int isTempRm;
    IOSIoVector* vec;
    int* in;
    int* outLen;
    void* outVal;
#pragma unused(interface)

    result = SOiPrepareTempRm(NULL, &rmId, &isTempRm);
    if (result == SO_SUCCESS) {
    if (optname == SO_CONFIG_FILTER_INPUT || optname == SO_CONFIG_FILTER_OUTPUT) {
        result = SO_EINVAL;
    } else {
        bufSize = OSRoundUp32B(SO_OPTLEN(optlen) + 0x60);
        buf = (u8*)SOiAlloc(0x0C, (s32)bufSize);
        if (buf == NULL) {
            result = SO_ENOMEM;
        } else {
            vec = (IOSIoVector*)buf;
            in = (int*)(buf + 0x20);
            outLen = (int*)((u8*)in + 0x20);
            outVal = (u8*)outLen + 0x20;

            in[0] = level;
            in[1] = optname;
            *outLen = SO_OPTLEN(optlen);

            vec[0].base = (u8*)in;
            vec[0].length = 8;
            vec[1].base = (u8*)outVal;
            vec[1].length = SO_OPTLEN(optlen);
            vec[2].base = (u8*)outLen;
            vec[2].length = 4;

            result = IOS_Ioctlv(rmId, 0x1C, 1, 2, vec);
            if (result >= 0 && optlen != NULL) {
                if (*optlen >= *outLen) {
                    if (optval != NULL) {
                        memcpy(optval, outVal, *outLen);
                    }
                    *optlen = *outLen;
                } else {
                    *optlen = *outLen;
                    result = SO_EINVAL;
                }
            }

            SOiFree(0x0C, buf, (s32)bufSize);
        }
    }

    result = SOiConcludeTempRm(NULL, result, isTempRm);
    }

    return result;
}
