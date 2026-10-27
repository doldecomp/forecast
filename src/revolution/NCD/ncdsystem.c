#include <revolution/IPC.h>
#include <revolution/NCD.h>
#include <revolution/OS.h>
#include <cstring>

#define NCD_PROFILE_COUNT 3
#define NCD_IPC_HEAP_SIZE 0x1B60

// /dev/net/ncd/manage ioctl commands (names unofficial)
#define NCD_IOCTL_READ_CONFIG 3
#define NCD_IOCTL_WRITE_CONFIG 4
#define NCD_IOCTL_GET_LINK_STATUS 7

// NCDProfile flags
#define NCD_PROFILE_FLAG_WIRED 0x01
#define NCD_PROFILE_FLAG_IN_USE 0x80

// NCDProfile configMethod of a wireless profile set up through the USB access point
#define NCD_CONFIG_METHOD_USBAP 1

// One connection setting
typedef struct NCDProfile {
    u8 flags;             // at 0x0
    u8 _001[0x7C2 - 0x1]; // at 0x1
    u8 configMethod;      // at 0x7C2
    u8 _7C3[0x91C - 0x7C3];
} NCDProfile;

typedef struct NCDConfig {
    u32 version;                           // at 0x0
    u8 selectedMedia;                      // at 0x4
    u8 nwc24Permission;                    // at 0x5
    u8 linkTimeout;                        // at 0x6
    u8 reserved;                           // at 0x7
    NCDProfile profiles[NCD_PROFILE_COUNT]; // at 0x8
} NCDConfig;

const char* __NCDVersion =
    "<< RVL_SDK - NCD \trelease build: Jun 28 2007 18:29:29 (0x4199_60831) >>";

// The zero initializers lay these out in definition order (the original
// order came from functions that are not linked in).
static u32 ncdInitFlags = 0;
static NCDConfig* ncdConfig = NULL;

static OSMutex ncdMutex ATTRIBUTE_ALIGN(32) = {0};
static s32 ncdResult[8] ATTRIBUTE_ALIGN(32) = {0};
static IPCIOVector ncdVec[4] ATTRIBUTE_ALIGN(32) = {0};

static NCDErr ExecConfigCommand(const char* funcName, NCDConfig* config,
                                s32 command);
static void LockRight(void);

NCDErr NCDGetLinkStatus(void) {
    NCDErr result;
    s32 fd;

    if (OSGetCurrentThread() == NULL) {
        return NCD_RESULT_NO_THREAD;
    }

    LockRight();

    fd = IOS_Open("/dev/net/ncd/manage", IPC_OPEN_NONE);
    if (fd < 0) {
        if (fd == IPC_RESULT_NOEXISTS_INTERNAL) {
            result = NCD_RESULT_INPROGRESS;
        } else {
            result = NCD_RESULT_IPC_ERROR;
        }
    } else {
        ncdVec[0].base = ncdResult;
        ncdVec[0].length = sizeof(ncdResult);

        if (IOS_Ioctlv(fd, NCD_IOCTL_GET_LINK_STATUS, 0, 1, ncdVec) < 0) {
            result = NCD_RESULT_IPC_ERROR;
        } else {
            result = ncdResult[0];
            if (result == NCD_RESULT_SUCCESS) {
                result = ncdResult[1];
                result = result >= 0 ? result : NCD_RESULT_FATAL;
            }
        }

        if (IOS_Close(fd) < 0) {
            result = NCD_RESULT_FATAL;
        }
    }

    OSUnlockMutex(&ncdMutex);
    return result;
}

// Bit masks of the enabled profiles of each connection kind
NCDErr NCDiGetEnabledConfigList(u32* wired, u32* wireless, u32* usbap) {
    NCDErr result;
    u32 wiredMask = 0;
    u32 wirelessMask = 0;
    u32 usbapMask = 0;
    u8 flags;
    s32 i;

    LockRight();

    result = ExecConfigCommand("NCDiGetEnabledConfigList", NULL, NCD_IOCTL_READ_CONFIG);
    if (result == NCD_RESULT_SUCCESS) {
        for (i = 0; i < NCD_PROFILE_COUNT; i++) {
            NCDProfile* profile = &ncdConfig->profiles[i];

            flags = profile->flags;
            if (flags & NCD_PROFILE_FLAG_IN_USE) {
                if (flags & NCD_PROFILE_FLAG_WIRED) {
                    wiredMask |= 1 << i;
                } else {
                    if (profile->configMethod != NCD_CONFIG_METHOD_USBAP) {
                        wirelessMask |= 1 << i;
                    }
                    if (profile->configMethod == NCD_CONFIG_METHOD_USBAP) {
                        usbapMask |= 1 << i;
                    }
                }
            }
        }
    }

    OSUnlockMutex(&ncdMutex);

    if (wired != NULL) {
        *wired = wiredMask;
    }
    if (wireless != NULL) {
        *wireless = wirelessMask;
    }
    if (usbap != NULL) {
        *usbap = usbapMask;
    }

    return result;
}

static NCDErr ExecConfigCommand(const char* funcName, NCDConfig* config,
                                s32 command) {
    NCDErr result = NCD_RESULT_SUCCESS;
    s32 fd;
#pragma unused(funcName)

    if (OSGetCurrentThread() == NULL) {
        return NCD_RESULT_NO_THREAD;
    }

    LockRight();

    fd = IOS_Open("/dev/net/ncd/manage", IPC_OPEN_NONE);
    if (fd < 0) {
        if (fd == IPC_RESULT_NOEXISTS_INTERNAL) {
            result = NCD_RESULT_INPROGRESS;
        } else {
            result = NCD_RESULT_IPC_ERROR;
        }
    } else {
        ncdVec[0].base = ncdConfig;
        ncdVec[0].length = sizeof(NCDConfig);
        ncdVec[1].base = ncdResult;
        ncdVec[1].length = sizeof(ncdResult);

        // 5 and 6 (not used by the channel) read and write the same way as 3 and 4.
        // config is compared with (void*)NULL: a plain NULL changes the generated code.
        switch (command) {
        case NCD_IOCTL_READ_CONFIG:
        case 5:
            if (IOS_Ioctlv(fd, command, 0, 2, ncdVec) < 0) {
                result = NCD_RESULT_IPC_ERROR;
            } else {
                result = ncdResult[0];
                if (result == NCD_RESULT_SUCCESS && config != (void*)NULL) {
                    memcpy(config, ncdConfig, sizeof(NCDConfig));
                }
            }
            break;
        case NCD_IOCTL_WRITE_CONFIG:
        case 6:
            if (config != (void*)NULL) {
                memcpy(ncdConfig, config, sizeof(NCDConfig));
            }

            if (IOS_Ioctlv(fd, command, 1, 1, ncdVec) < 0) {
                result = NCD_RESULT_IPC_ERROR;
            } else {
                result = ncdResult[0];
            }
            break;
        }

        if (IOS_Close(fd) < 0) {
            result = NCD_RESULT_FATAL;
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
        if ((u32)IPCGetBufferHi() - (u32)lo < NCD_IPC_HEAP_SIZE) {
            OSPanic("ncdsystem.c", 1457,
                    "Could not reserve heap for NCD library from IPC arena");
        }
        IPCSetBufferLo(lo + NCD_IPC_HEAP_SIZE);

        ncdConfig = (NCDConfig*)lo;
        memset(lo, 0, NCD_IPC_HEAP_SIZE);
        memset(ncdResult, 0, sizeof(ncdResult));
        memset(ncdVec, 0, sizeof(ncdVec));

        ncdInitFlags |= 1;
    }

    OSRestoreInterrupts(enabled);
    OSLockMutex(&ncdMutex);
}
