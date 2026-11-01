// Based on NETStartupErrorCode.c from The Last Story decompilation
// (TheSoraHD/TheLastStoryDecomp, libs/RVL_SDK/src/revolution/net/, CC0)
// and NET/netmisc.c from the Monster Hunter Tri decompilation
// (InusualZ/mhtri-decomp, CC0). Both are 2009 RevoEX builds; this file was
// checked against this channel's RevoEX-2.0PR4 (2007) code, which is the same.
#include <revolution/NCD.h>
#include <revolution/NET.h>
#include <revolution/OS.h>

// None of the three functions is inlined into its caller
#pragma auto_inline off

s32 GetStartupErrorCode(s32 result, int type);

// Index of the lowest set bit in mask, or -1 if none is set
static inline int GetLowestBit(u32 mask) {
    int i;
    u32 bit;

    for (i = 0, bit = 1; i < 32; i++, bit <<= 1) {
        if (mask & bit) {
            return i;
        }
    }
    return -1;
}

// Detail code for the kind of enabled connection settings: 20+n for wired,
// 30+n for wireless and 40+n for the third list (n = first enabled slot).
// 99 if none or more than one kind is enabled.
int NETiGetConnectionTypeFromConfigList(u32 wired, u32 wireless, u32 wirelessAoss) {
    int type = 99;

    if (wired != 0) {
        if (wireless == 0 && wirelessAoss == 0) {
            type = GetLowestBit(wired) + 20;
        }
    } else if (wireless != 0) {
        if (wirelessAoss == 0) {
            type = GetLowestBit(wireless) + 30;
        }
    } else if (wirelessAoss != 0) {
        type = GetLowestBit(wirelessAoss) + 40;
    }
    return type;
}

// Network error code (negative) for a failed SOStartup() result
s32 NETGetStartupErrorCode(s32 result) {
    u32 wirelessAoss;
    u32 wireless;
    u32 wired;
    int type = 99;

    if (NCDiGetEnabledConfigList(&wired, &wireless, &wirelessAoss) >= 0) {
        type = NETiGetConnectionTypeFromConfigList(wired, wireless, wirelessAoss);
    }
    if (type < 0) {
        result = (s32)0x80000000;
        type = 99;
    }
    return GetStartupErrorCode(result, type) - type;
}

// Base error code for a SOStartup() result; some depend on a wired connection
s32 GetStartupErrorCode(s32 result, int type) {
    if (result >= 0) {
        return 0;
    }

    switch (result) {
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
    case -48:
    case -39:
        if (type >= 20 && type < 30) {
            return -51400;
        }
        return -51300;
    case -102:
    case -101:
    case -100:
        return -52000;
    case (s32)0x80000000:
        return -50100;
    default:
        OSReport("Unknown SOStartup Error: %d\n", result);
        return -50100;
    }
}
