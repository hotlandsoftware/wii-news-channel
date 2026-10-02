#include <revolution/nwc24.h>
#include <revolution/nwc24/NWC24Internal.h>

void NWC24Data_Init(NWC24Data* pData) {
    pData->pData = NULL;
    pData->size = 0;
}

void NWC24Data_SetDataP(NWC24Data* pData, const void* pPtr, u32 size) {
    pData->pData = pPtr;
    pData->size = size;
}

void NWC24Date_Init(NWC24Date* pDate) {
    pDate->year = 2000;
    pDate->month = 1;
    pDate->day = 1;
    pDate->hour = 12;
    pDate->min = 0;
    pDate->sec = 0;
    pDate->BYTE_0x7 = 0;
}

void NWC24iConvIdToStr(u64 addr, char* pBuffer) {
    u64 temp = addr;
    int i;

    for (i = NWC24i_WII_ID_LEN - 1; i >= 0; i--) {
        pBuffer[i] = '0' + temp % 10;
        temp /= 10;
    }

    pBuffer[NWC24i_WII_ID_LEN] = '\0';
}

// The two functions below are not in Petari/ogws (dead-stripped there).
// They are used by the download task setters; names are guesses.
NWC24Err NWC24iCheckStrLength(const char* pStr, u32 minLen, u32 maxLen) {
    u32 len;

    if (pStr == NULL) {
        return NWC24_ERR_NULL;
    }

    len = STD_strnlen(pStr, maxLen + 1);
    if (len == 0) {
        return NWC24_ERR_NULL;
    }

    if (len > maxLen) {
        return NWC24_ERR_FULL;
    }

    if (len < minLen) {
        return NWC24_ERR_FORMAT;
    }

    return NWC24_OK;
}

s32 NWC24iStrLCpy(char* pDst, const char* pSrc, s32 n) {
    char* pIt;
    s32 i;

    if (pDst == NULL || pSrc == NULL) {
        return 0;
    }

    pIt = pDst;
    for (i = 0; i + 1 < n; pSrc++, i++, pIt++) {
        *pIt = *pSrc;
        if (*pSrc == '\0') {
            break;
        }
    }

    if (i + 1 >= n && n != 0) {
        pDst[i] = '\0';
    }

    return i;
}
