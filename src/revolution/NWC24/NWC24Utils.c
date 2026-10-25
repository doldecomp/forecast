#include <revolution/NWC24.h>
#include <revolution/NWC24/NWC24Internal.h>

// Not in the DOL (dead-stripped): inline, so that no copy is emitted
inline void NWC24Data_Init(NWC24Data* pData) {
    pData->pData = NULL;
    pData->size = 0;
}

// Not in the DOL (dead-stripped): inline, so that no copy is emitted
inline void NWC24Data_SetDataP(NWC24Data* pData, const void* pPtr, u32 size) {
    pData->pData = pPtr;
    pData->size = size;
}

// Not in the DOL (dead-stripped): inline, so that no copy is emitted
inline void NWC24Date_Init(NWC24Date* pDate) {
    pDate->year = 2000;
    pDate->month = 1;
    pDate->day = 1;
    pDate->hour = 12;
    pDate->min = 0;
    pDate->sec = 0;
    pDate->BYTE_0x7 = 0;
}

// Not in the DOL (dead-stripped): inline, so that no copy is emitted
inline void NWC24iConvIdToStr(u64 addr, char* pBuffer) {
    u64 temp = addr;
    int i;

    for (i = NWC24i_WII_ID_LEN - 1; i >= 0; i--) {
        pBuffer[i] = '0' + temp % 10;
        temp /= 10;
    }

    pBuffer[NWC24i_WII_ID_LEN] = '\0';
}

NWC24Err NWC24iCheckStringLength(const char* pStr, int minLen, int maxLen) {
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

    return len < minLen ? NWC24_ERR_FORMAT : NWC24_OK;
}

s32 NWC24iStrLCpy(char* pDst, const char* pSrc, s32 len) {
    int i;

    if (pDst == NULL || pSrc == NULL) {
        return 0;
    }

    for (i = 0; i + 1 < len; pSrc++, i++) {
        pDst[i] = *pSrc;
        if (*pSrc == '\0') {
            break;
        }
    }

    if (i + 1 >= len && len != 0) {
        pDst[i] = '\0';
    }

    return i;
}
