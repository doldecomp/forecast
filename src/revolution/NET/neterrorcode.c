#include <revolution/NCD.h>
#include <revolution/NET/neterrorcode.h>
#include <revolution/OS.h>
#include <revolution/SO.h>

// Connection types: the kind of connection setting plus the setting's index,
// or NONE when no single kind is enabled
#define NET_CONNECTION_TYPE_WIRED 20
#define NET_CONNECTION_TYPE_WIRELESS 30
#define NET_CONNECTION_TYPE_USBAP 40
#define NET_CONNECTION_TYPE_NONE 99

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
            connType = GetLowestBitIndex(wired) + NET_CONNECTION_TYPE_WIRED;
        }
    } else if (wireless != 0) {
        if (usbap == 0) {
            connType = GetLowestBitIndex(wireless) + NET_CONNECTION_TYPE_WIRELESS;
        }
    } else if (usbap != 0) {
        connType = GetLowestBitIndex(usbap) + NET_CONNECTION_TYPE_USBAP;
    }

    return connType;
}

// The error number the system shows for a failed SOStartup, negated (e.g. -51330:
// -51300 for the error, minus connection type 30), or 0 if soErr is not an error.
// GetStartupErrorCode is not inlined into it.
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
    case SO_ENOENT:
        return -50200;
    case SO_EINVAL:
        return -50300;
    case SO_ENXIO:
        return -50400;
    case SO_ERR_ADDR_COLLISION:
        return -52700;
    case SO_ERR_LINK_UP_TIMEOUT:
        if (connType >= NET_CONNECTION_TYPE_WIRED && connType < NET_CONNECTION_TYPE_WIRELESS) {
            return -51400;
        }
        return -51000;
    case SO_ERR_LINK_DOWN:
    case SO_ETIMEDOUT:
    case SO_ENOLINK:
    case SO_ENETRESET:
        if (connType >= NET_CONNECTION_TYPE_WIRED && connType < NET_CONNECTION_TYPE_WIRELESS) {
            return -51400;
        }
        return -51300;
    case SO_ERR_DHCP_NAK:
    case SO_ERR_DHCP_EXPIRED:
    case SO_ERR_DHCP_TIMEOUT:
        return -52000;
    case SO_EFATAL:
        return -50100;
    default:
        OSReport("Unknown SOStartup Error: %d\n", soErr);
        return -50100;
    }
}
