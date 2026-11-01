// Based on NCD/ncdsystem.c from the Monster Hunter Tri decompilation
// (InusualZ/mhtri-decomp, src/NCD/ncdsystem.c, CC0) and
// libs/RVL_SDK/src/revolution/ncd/ncdsystem.c from The Last Story
// decompilation (TheSoraHD/TheLastStoryDecomp, CC0). Both are 2009 builds;
// this channel has the Jun 2007 build, of which only the four functions
// below are linked. Adjusted to it from its assembly.
#include <revolution/IPC.h>
#include <revolution/NCD.h>
#include <revolution/OS.h>
#include <revolution/version.h>
#include <cstring>

// Pointers are checked against a void pointer (li rX,0; cmplw)
#undef NULL
#define NULL ((void*)0)

// One connection profile of the network configuration
typedef struct NCDProfile {
    u8 flags;           // at 0x0
    u8 _01[3];          // at 0x1
    u8 ip[0x14];        // at 0x4
    u8 dns[0xC];        // at 0x18
    u8 proxy[0x79C];    // at 0x24
    u8 wireless[0x15C]; // at 0x7C0
} NCDProfile;

// NCDProfile.flags
#define NCD_PROFILE_ENABLED 0x80
#define NCD_PROFILE_WIRED 0x01

// NCDProfile.wireless[2]: the profile was set up with AOSS
#define NCD_WIRELESS_AOSS 1

// The network configuration that /dev/net/ncd/manage reads and writes
typedef struct NCDConfig {
    u8 _00[6];              // at 0x0
    u8 linkType;            // at 0x6
    u8 _07;                 // at 0x7
    NCDProfile profiles[3]; // at 0x8
} NCDConfig;

// The configuration buffer is taken from the IPC arena, 32-byte aligned
#define NCD_CONFIG_HEAP_SIZE OSRoundUp32B(sizeof(NCDConfig))

RVL_LIB_VERSION(NCD, "Jun 28 2007", "18:29:29", "0x4199_60831");

static NCDConfig* ncdConfigBuffer;
static u32 ncdInitFlags;

// Zero-initialised so that .bss keeps the order of definition
static OSMutex ncdMutex = {0};
static s32 ncdReply[8] ALIGN(32) = {0};
static IPCIOVector ncdVectors[4] ALIGN(32) = {0};

NCDErr NCDiConfigRequest(const char* function, NCDConfig* config, s32 command);
void NCDiInitAndLock(void);

NCDErr NCDGetLinkStatus(void) {
    NCDErr result;
    s32 fd;

    if (OSGetCurrentThread() == 0) {
        return -5;
    }

    NCDiInitAndLock();
    fd = IOS_Open("/dev/net/ncd/manage", 0);
    if (fd < 0) {
        if (fd == -6) {
            result = -8;
        } else {
            result = -2;
        }
    } else {
        ncdVectors[0].base = ncdReply;
        ncdVectors[0].length = sizeof(ncdReply);
        if (IOS_Ioctlv(fd, 7, 0, 1, ncdVectors) < 0) {
            result = -2;
        } else {
            result = ncdReply[0];
            if (result == 0) {
                result = ncdReply[1];
                result = (result >= 0) ? result : -1;
            }
        }
        if (IOS_Close(fd) < 0) {
            result = -1;
        }
    }
    OSUnlockMutex(&ncdMutex);
    return result;
}

NCDErr NCDiGetEnabledConfigList(u32* wiredList, u32* wirelessList,
                                u32* aossList) {
    NCDErr result;
    u32 wired = 0;
    u32 wireless = 0;
    u32 aoss = 0;
    NCDConfig* config;
    NCDProfile* profile;
    u8 flags;
    s32 i;

    NCDiInitAndLock();
    result = NCDiConfigRequest("NCDiGetEnabledConfigList", NULL, 3);
    if (result == 0) {
        config = ncdConfigBuffer;
        for (i = 0; i < 3; i++) {
            profile = &config->profiles[i];
            flags = profile->flags;
            if (flags & NCD_PROFILE_ENABLED) {
                if (flags & NCD_PROFILE_WIRED) {
                    wired |= 1 << i;
                } else {
                    if (profile->wireless[2] != NCD_WIRELESS_AOSS) {
                        wireless |= 1 << i;
                    }
                    // Indexed again rather than through profile, so the
                    // byte is read a second time
                    if (config->profiles[i].wireless[2] == NCD_WIRELESS_AOSS) {
                        aoss |= 1 << i;
                    }
                }
            }
        }
    }
    OSUnlockMutex(&ncdMutex);

    if (wiredList != NULL) {
        *wiredList = wired;
    }
    if (wirelessList != NULL) {
        *wirelessList = wireless;
    }
    if (aossList != NULL) {
        *aossList = aoss;
    }
    return result;
}

// Reads (3, 5) or writes (4, 6) the configuration through the IPC buffer.
// function is the caller's name; this build does not use it.
NCDErr NCDiConfigRequest(const char* function, NCDConfig* config,
                         s32 command) {
    NCDErr result = 0;
    s32 fd;

    if (OSGetCurrentThread() == 0) {
        return -5;
    }

    NCDiInitAndLock();
    fd = IOS_Open("/dev/net/ncd/manage", 0);
    if (fd < 0) {
        if (fd == -6) {
            result = -8;
        } else {
            result = -2;
        }
    } else {
        ncdVectors[0].base = ncdConfigBuffer;
        ncdVectors[0].length = sizeof(NCDConfig);
        ncdVectors[1].base = ncdReply;
        ncdVectors[1].length = sizeof(ncdReply);

        switch (command) {
        case 3:
        case 5:
            if (IOS_Ioctlv(fd, command, 0, 2, ncdVectors) < 0) {
                result = -2;
            } else {
                result = ncdReply[0];
                if (result == 0 && config != NULL) {
                    memcpy(config, ncdConfigBuffer, sizeof(NCDConfig));
                }
            }
            break;
        case 4:
        case 6:
            if (config != NULL) {
                memcpy(ncdConfigBuffer, config, sizeof(NCDConfig));
            }
            if (IOS_Ioctlv(fd, command, 1, 1, ncdVectors) < 0) {
                result = -2;
            } else {
                result = ncdReply[0];
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

// Sets the library up on first use, then takes the library lock
void NCDiInitAndLock(void) {
    BOOL enabled = OSDisableInterrupts();
    u8* lo;

    if (!(ncdInitFlags & 1)) {
        OSRegisterVersion(__NCDVersion);
        OSInitMutex(&ncdMutex);

        lo = (u8*)OSRoundUp32B(IPCGetBufferLo());
        if ((u32)((u8*)IPCGetBufferHi() - lo) < NCD_CONFIG_HEAP_SIZE) {
            OSPanic("ncdsystem.c", 1457,
                    "Could not reserve heap for NCD library from IPC arena");
        }
        IPCSetBufferLo(lo + NCD_CONFIG_HEAP_SIZE);

        ncdConfigBuffer = (NCDConfig*)lo;
        memset(lo, 0, NCD_CONFIG_HEAP_SIZE);
        memset(ncdReply, 0, sizeof(ncdReply));
        memset(ncdVectors, 0, sizeof(ncdVectors));
        ncdInitFlags |= 1;
    }
    OSRestoreInterrupts(enabled);
    OSLockMutex(&ncdMutex);
}
