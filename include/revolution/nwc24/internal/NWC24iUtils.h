#ifndef RVL_SDK_NWC24_INTERNAL_UTILS_H
#define RVL_SDK_NWC24_INTERNAL_UTILS_H
#include <types.h>
#include <macros.h>
#include <stddef.h>

#include <revolution/nwc24/NWC24Types.h>
#ifdef __cplusplus
extern "C" {
#endif

#define NWC24i_KSTRLEN(x) ((int)(sizeof(x)))

void NWC24iConvIdToStr(u64 addr, char* pBuffer);
NWC24Err NWC24iCheckStrLength(const char* pStr, u32 minLen, u32 maxLen);
s32 NWC24iStrLCpy(char* pDst, const char* pSrc, s32 n);

#ifdef __cplusplus
}
#endif
#endif
