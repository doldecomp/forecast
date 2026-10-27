#include <revolution/NCD.h>
#include <revolution/NET/neterrorcode.h>
#include <revolution/OS.h>

// Connection type when no single kind of connection setting is enabled.
#define NET_CONNECTION_TYPE_NONE 99

// SO error code for a fatal error (SO_EFATAL)
#define SO_EFATAL ((s32)0x80000000)

static s32 GetStartupErrorCode(s32 soErr, s32 connType);

static inline s32 GetLowestBitIndex(u32 list) {
    s32 i;
    u32 bit;

    for (i = 0, bit = 1; i < 32; i++, bit <<= 1) {
        if (list & bit) {
            return i;
        }
    }

    return -1;
}

s32 NETiGetConnectionTypeFromConfigList(u32 wired, u32 wireless, u32 usbap) {
    s32 connType = NET_CONNECTION_TYPE_NONE;

    if (wired != 0) {
        if (wireless == 0 && usbap == 0) {
            connType = GetLowestBitIndex(wired) + 20;
        }
    } else if (wireless != 0) {
        if (usbap == 0) {
            connType = GetLowestBitIndex(wireless) + 30;
        }
    } else if (usbap != 0) {
        connType = GetLowestBitIndex(usbap) + 40;
    }

    return connType;
}

#pragma push
#pragma dont_inline on
s32 NETGetStartupErrorCode(s32 soErr) {
    u32 usbap;
    u32 wireless;
    u32 wired;
    s32 connType = NET_CONNECTION_TYPE_NONE;

    if (NCDiGetEnabledConfigList(&wired, &wireless, &usbap) >= 0) {
        connType = NETiGetConnectionTypeFromConfigList(wired, wireless, usbap);
    }

    if (connType < 0) {
        soErr = SO_EFATAL;
        connType = NET_CONNECTION_TYPE_NONE;
    }

    return GetStartupErrorCode(soErr, connType) - connType;
}
#pragma pop

static s32 GetStartupErrorCode(s32 soErr, s32 connType) {
    if (soErr >= 0) {
        return 0;
    }

    switch (soErr) {
    case -45:
        return -50200;
    case -28:
        return -50300;
    case -62:
        return -50400;
    case -111:
        return -52700;
    case -121:
        if (connType >= 20 && connType < 30) {
            return -51400;
        }
        return -51000;
    case -112:
    case -76:
    case -48:
    case -39:
        if (connType >= 20 && connType < 30) {
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
        OSReport("Unknown SOStartup Error: %d\n", soErr);
        return -50100;
    }
}
