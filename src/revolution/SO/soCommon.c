// Based on lib/rvl/so/soCommon.c from the Mario Kart Wii decompilation
// (github.com/doldecomp/mkw, formerly riidefi/mkw, CC0 1.0), which has the
// Dec 2007 build of this file. SOInit, which MKW does not link, follows
// libs/RVL_SDK/src/revolution/so/SOBasic.c from TheSoraHD/TheLastStoryDecomp
// (CC0 1.0). Adjusted to this channel's Jun 2007 build from its assembly.
#include <revolution/IPC.h>
#include <revolution/NCD.h>
#include <revolution/NWC24.h>
#include <revolution/OS.h>
#include <revolution/SO.h>
#include <revolution/version.h>
#include <cstring>

// soState
enum {
    SO_INTERNAL_STATE_TERMINATED = 0,
    SO_INTERNAL_STATE_READY = 1,
    SO_INTERNAL_STATE_ACTIVE = 2
};

// soWork.rmState
enum {
    SO_INTERNAL_RM_STATE_OPENED = 0,
    SO_INTERNAL_RM_STATE_WORKING = -1,
    SO_INTERNAL_RM_STATE_CLOSED = -2
};

// NCDGetLinkStatus results
enum {
    NCD_LINKSTATUS_WORKING = 1,
    NCD_LINKSTATUS_NONE = 2,
    NCD_RESULT_SUCCESS = 0,
    NCD_RESULT_INPROGRESS = -8
};

NWC24Err NWC24iStartupSocket(NWC24Err* pExResult);
NWC24Err NWC24iCleanupSocket(NWC24Err* pExResult);
NWC24Err NWC24iLockSocket(void);
NWC24Err NWC24iUnlockSocket(void);

RVL_LIB_VERSION(SO, "Jun 28 2007", "18:29:42", "0x4199_60831");

u8 soState = 0;
SOSysWork soWork;
s32 soError = 0;
static BOOL soRegistered;
BOOL soBufAddrCheck = TRUE;
static char NET_RM_SOCK[] = "/dev/net/ip/top";

// Stores the result as the calling thread's SO error
static inline void SOiSetLastError(s32 result) {
    OSThread* cur = OSGetCurrentThread();
    if (cur) {
        cur->error = result;
    } else {
        soError = result;
    }
}

s32 SOInit(const SOLibraryConfig* config) {
    s32 result = SO_SUCCESS;
    BOOL enabled = OSDisableInterrupts();

    if (!soRegistered) {
        OSRegisterVersion(__SOVersion);
        soRegistered = TRUE;
    }

    switch (soState) {
    case SO_INTERNAL_STATE_READY:
    case SO_INTERNAL_STATE_ACTIVE:
        result = SO_EALREADY;
        break;
    case SO_INTERNAL_STATE_TERMINATED:
    default:
        if (config == NULL || config->alloc == NULL || config->free == NULL) {
            result = SO_EINVAL;
            break;
        }
        memset(&soWork, 0, sizeof(soWork));
        soWork.allocFunc = config->alloc;
        soWork.freeFunc = config->free;
        soWork.allocCount = 0;
        soWork.rmState = SO_INTERNAL_RM_STATE_CLOSED;
        soWork.rmFd = -1;
        soWork.hostBuffer = SOiAlloc(0xB, 0x460);
        if (soWork.hostBuffer == NULL) {
            result = SO_ENOMEM;
        }
        if (soWork.hostBuffer != NULL) {
            soState = SO_INTERNAL_STATE_READY;
        }
        break;
    }

    SOiSetLastError(result);
    OSRestoreInterrupts(enabled);
    return result;
}

s32 SOStartup(void) {
    // 10 minutes
    return SOStartupEx(600000);
}

// Turns an NWC24 socket request result into an SO error.
static inline s32 SOiConvertNWC24Error(NWC24Err errNwc24, s32 exErr) {
    s32 result = SO_EINVAL;
    switch (errNwc24) {
    case NWC24_OK:
        result = SO_SUCCESS;
        break;
    case NWC24_ERR_MUTEX:
    case NWC24_ERR_NOT_FOUND:
        result = SO_ENOLINK;
        break;
    case NWC24_ERR_CONFIG_NETWORK:
    case NWC24_ERR_FAILED:
        result = exErr;
        break;
    case NWC24_ERR_FATAL:
        result = SO_EFATAL;
        break;
    case NWC24_ERR_INPROGRESS:
        result = SO_EINPROGRESS;
        break;
    }
    return result;
}

s32 SOStartupEx(s32 timeout) {
    s32 result;
    BOOL enabled;
    NWC24Err exErr;
    s64 limitTime;
    s32 linkupRetryCount;

    limitTime = 0;
    if (timeout != 0) {
        limitTime = __OSGetSystemTime() + OS_MSEC_TO_TICKS(timeout);
    }

    linkupRetryCount = 4;

begin_startup:
    result = SO_SUCCESS;

    enabled = OSDisableInterrupts();

    switch (soState) {
    case SO_INTERNAL_STATE_TERMINATED:
    default:
        result = SO_ENETRESET;
        break;
    case SO_INTERNAL_STATE_ACTIVE:
        result = SO_EALREADY;
        break;
    case SO_INTERNAL_STATE_READY:
        if (soWork.rmState > SO_INTERNAL_RM_STATE_CLOSED) {
            result = SO_EBUSY;
            break;
        } else if (!OSGetCurrentThread()) {
            result = SO_EFATAL;
            break;
        } else {
            s32 resultNCD;

            soWork.rmState = SO_INTERNAL_RM_STATE_WORKING;
            OSRestoreInterrupts(enabled);

            // Wait for the network link to come up
            while (TRUE) {
                resultNCD = NCDGetLinkStatus();
                if (resultNCD != NCD_RESULT_INPROGRESS &&
                    resultNCD != NCD_LINKSTATUS_WORKING) {
                    break;
                }
                OSSleepTicks(OS_MSEC_TO_TICKS((s64)100));
                if (limitTime != 0 && limitTime < __OSGetSystemTime()) {
                    if (resultNCD == NCD_LINKSTATUS_WORKING) {
                        result = SO_ERR_LINK_UP_TIMEOUT;
                    } else {
                        result = SO_EFATAL;
                    }
                    goto change_state;
                }
            }
            if (resultNCD < NCD_RESULT_SUCCESS) {
                result = SO_EFATAL;
                goto change_state;
            } else if (resultNCD == NCD_LINKSTATUS_NONE) {
                result = SO_ENOENT;
                goto change_state;
            }

            while (TRUE) {
                soWork.rmFd = IOS_Open(NET_RM_SOCK, IPC_OPEN_NONE);
                if (soWork.rmFd != -6) {
                    break;
                }
                OSSleepTicks(OS_MSEC_TO_TICKS((s64)100));
                if (limitTime != 0 && limitTime < __OSGetSystemTime()) {
                    result = SO_EFATAL;
                    goto change_state;
                }
            }

            if (soWork.rmFd < 0) {
                result = SO_EFATAL;
                goto change_state;
            } else {
                NWC24Err errNwc24;

                result = SO_SUCCESS;
                exErr = NWC24_OK;

                // Ask the NWC24 daemon to hand over the network
                while (TRUE) {
                    errNwc24 = NWC24iStartupSocket(&exErr);
                    if (errNwc24 != NWC24_ERR_INPROGRESS) {
                        break;
                    }
                    OSSleepTicks(OS_MSEC_TO_TICKS((s64)100));
                    if (limitTime != 0 && limitTime < __OSGetSystemTime()) {
                        result = SO_EFATAL;
                        break;
                    }
                }
                if (result == SO_SUCCESS) {
                    result = SOiConvertNWC24Error(errNwc24, exErr);
                }
                if (result != SO_SUCCESS) {
                    if (IOS_Close(soWork.rmFd) < 0) {
                        result = SO_EFATAL;
                        goto change_state;
                    } else {
                        soWork.rmFd = -1;
                    }
                }
            }

        change_state:
            enabled = OSDisableInterrupts();
            if (result == SO_SUCCESS) {
                soState = SO_INTERNAL_STATE_ACTIVE;
                soWork.rmState = SO_INTERNAL_RM_STATE_OPENED;
            } else {
                soState = SO_INTERNAL_STATE_READY;
                if (result != SO_EFATAL) {
                    soWork.rmState = SO_INTERNAL_RM_STATE_CLOSED;
                }
            }
        }
        break;
    }

    SOiSetLastError(result);
    OSRestoreInterrupts(enabled);

    if (result == SO_SUCCESS) {
        s64 dhcpTimeOutTicks;

        if (limitTime != 0) {
            dhcpTimeOutTicks = limitTime - __OSGetSystemTime();
        } else {
            dhcpTimeOutTicks = 0;
        }
        if (limitTime != 0 && dhcpTimeOutTicks <= 0) {
            result = SO_ETIMEDOUT;
        } else {
            result = SOiWaitForDHCPEx((s32)OS_TICKS_TO_MSEC(dhcpTimeOutTicks));
        }

        if (result != SO_SUCCESS) {
            SOCleanup();
        }
    }

    SOiSetLastError(result);

    if (result == -112) {
        linkupRetryCount--;
        if (linkupRetryCount >= 0) {
            goto begin_startup;
        }
    }

    return result;
}

s32 SOCleanup(void) {
    s32 result = SO_SUCCESS;
    BOOL enabled = OSDisableInterrupts();
    NWC24Err exErr = NWC24_OK;
    NWC24Err errNwc24;

    switch (soState) {
    case SO_INTERNAL_STATE_TERMINATED:
    default:
        result = SO_ENETRESET;
        break;
    case SO_INTERNAL_STATE_READY:
        result = SO_EALREADY;
        break;
    case SO_INTERNAL_STATE_ACTIVE:
        if (soWork.rmState < SO_INTERNAL_RM_STATE_OPENED) {
            result = SO_EBUSY;
            break;
        } else if (!OSGetCurrentThread()) {
            result = SO_EFATAL;
            break;
        } else {
            soWork.rmState = SO_INTERNAL_RM_STATE_WORKING;
            OSRestoreInterrupts(enabled);

            errNwc24 = NWC24iCleanupSocket(&exErr);
            result = SOiConvertNWC24Error(errNwc24, exErr);
            if (result == SO_SUCCESS) {
                if (IOS_Close(soWork.rmFd) < 0) {
                    result = SO_EFATAL;
                } else {
                    soWork.rmFd = -1;
                }
            }

            enabled = OSDisableInterrupts();
            if (result == SO_SUCCESS) {
                soState = SO_INTERNAL_STATE_READY;
                soWork.rmState = SO_INTERNAL_RM_STATE_CLOSED;
            } else if (result != SO_EFATAL) {
                soState = SO_INTERNAL_STATE_ACTIVE;
                soWork.rmState = SO_INTERNAL_RM_STATE_OPENED;
            }
        }
        break;
    }

    SOiSetLastError(result);
    OSRestoreInterrupts(enabled);
    return result;
}

static inline BOOL SOiIsBufferAddrCheck(void) {
    return soBufAddrCheck;
}

void* SOiAlloc(u32 name, s32 size) {
    if (size > 0 && soWork.allocFunc) {
        void* ptr = soWork.allocFunc(name, size);
        if (ptr) {
            soWork.allocCount++;
            // IOS can only use buffers in MEM2
            if (SOiIsBufferAddrCheck() &&
                !(((u32)ptr & 0x1FFFFFFF) >= 0x10000000 &&
                  ((u32)ptr & 0x1FFFFFFF) < 0x18000000)) {
                SOiFree(name, ptr, size);
                ptr = NULL;
            }
        }
        return ptr;
    }
    return NULL;
}

void SOiFree(u32 name, void* ptr, s32 size) {
    if (ptr != NULL && soWork.freeFunc != NULL) {
        soWork.allocCount--;
        soWork.freeFunc(name, ptr, size);
    }
}

s32 SOiPrepare(const char* funcName, s32* pRmFd) {
#pragma unused(funcName)
    s32 result = SO_SUCCESS;
    BOOL enabled = OSDisableInterrupts();

    switch (soState) {
    case SO_INTERNAL_STATE_TERMINATED:
        result = SO_ENETRESET;
        break;
    case SO_INTERNAL_STATE_READY:
    default:
        result = SO_EINVAL;
        break;
    case SO_INTERNAL_STATE_ACTIVE:
        if (soWork.rmState < SO_INTERNAL_RM_STATE_OPENED) {
            result = SO_EBUSY;
            break;
        } else if (!OSGetCurrentThread()) {
            result = SO_EFATAL;
            break;
        }
        *pRmFd = soWork.rmFd;
        break;
    }

    if (result != SO_SUCCESS) {
        SOiSetLastError(result);
    }
    OSRestoreInterrupts(enabled);
    return result;
}

s32 SOiConclude(const char* funcName, s32 result) {
#pragma unused(funcName)
    BOOL enabled = OSDisableInterrupts();
    SOiSetLastError(result);
    OSRestoreInterrupts(enabled);
    return result;
}

s32 SOiPrepareTempRm(const char* funcName, s32* pRmFd, BOOL* pIsTempRm) {
    s32 result = SO_SUCCESS;
    BOOL enabled = OSDisableInterrupts();
    NWC24Err errNwc24;

    switch (soState) {
    case SO_INTERNAL_STATE_TERMINATED:
        result = SO_ENETRESET;
        break;
    case SO_INTERNAL_STATE_READY:
    default:
        // Not started up: open a temporary connection
        if (soWork.rmState > SO_INTERNAL_RM_STATE_CLOSED) {
            result = SO_EBUSY;
            break;
        } else if (!OSGetCurrentThread()) {
            result = SO_EFATAL;
            break;
        }
        soWork.rmState = SO_INTERNAL_RM_STATE_WORKING;
        OSRestoreInterrupts(enabled);
        soWork.rmFd = IOS_Open(NET_RM_SOCK, IPC_OPEN_NONE);
        if (soWork.rmFd < 0) {
            enabled = OSDisableInterrupts();
            if (soWork.rmFd == -6) {
                result = SO_EINPROGRESS;
                soWork.rmState = SO_INTERNAL_RM_STATE_CLOSED;
            } else {
                result = SO_EFATAL;
            }
        } else {
            *pRmFd = soWork.rmFd;
            if ((errNwc24 = NWC24iLockSocket()) == NWC24_OK) {
                s32 errNcd = NCDGetLinkStatus();
                switch (errNcd) {
                case 3:
                case 4:
                case 5:
                    result = IOS_Ioctl(soWork.rmFd, 0x1F, NULL, 0, NULL, 0);
                    if (result == SO_SUCCESS) {
                        *pIsTempRm = TRUE;
                    } else {
                        result = SOiConcludeTempRm(funcName, result, TRUE);
                    }
                    break;
                case NCD_RESULT_INPROGRESS:
                    result = SOiConcludeTempRm(funcName, SO_EINPROGRESS, TRUE);
                    break;
                case -1:
                case -2:
                    result = SOiConcludeTempRm(funcName, SO_EFATAL, TRUE);
                    break;
                default:
                    result = SOiConcludeTempRm(funcName, SO_ENOLINK, TRUE);
                }
                enabled = OSDisableInterrupts();
            } else {
                switch (errNwc24) {
                case NWC24_ERR_INPROGRESS:
                    result = SO_EINPROGRESS;
                    break;
                case NWC24_ERR_FAILED:
                    result = SO_ENETRESET;
                    break;
                case NWC24_ERR_DONE:
                    result = SO_ENOLINK;
                    break;
                case NWC24_ERR_MUTEX:
                case NWC24_ERR_FATAL:
                default:
                    result = SO_EFATAL;
                    break;
                }
                if (IOS_Close(soWork.rmFd) < 0) {
                    result = SO_EFATAL;
                }
                enabled = OSDisableInterrupts();
                if (result != SO_EFATAL) {
                    soWork.rmFd = -1;
                    soWork.rmState = SO_INTERNAL_RM_STATE_CLOSED;
                }
            }
        }
        break;
    case SO_INTERNAL_STATE_ACTIVE:
        if (soWork.rmState < SO_INTERNAL_RM_STATE_OPENED) {
            result = SO_EBUSY;
            break;
        } else if (!OSGetCurrentThread()) {
            result = SO_EFATAL;
            break;
        }
        *pIsTempRm = FALSE;
        *pRmFd = soWork.rmFd;
        break;
    }

    if (result != SO_SUCCESS) {
        SOiSetLastError(result);
    }
    OSRestoreInterrupts(enabled);
    return result;
}

s32 SOiConcludeTempRm(const char* funcName, s32 result, BOOL isTempRm) {
#pragma unused(funcName)
    BOOL enabled;

    if (isTempRm == TRUE) {
        switch (NWC24iUnlockSocket()) {
        case NWC24_OK:
            break;
        case NWC24_ERR_INPROGRESS:
            result = SO_EINPROGRESS;
            break;
        case NWC24_ERR_FATAL:
        default:
            result = SO_EFATAL;
        }
        if (IOS_Close(soWork.rmFd) < 0) {
            result = SO_EFATAL;
        }
        enabled = OSDisableInterrupts();
        if (result != SO_EFATAL) {
            soWork.rmFd = -1;
            soWork.rmState = SO_INTERNAL_RM_STATE_CLOSED;
        }
    } else {
        enabled = OSDisableInterrupts();
    }

    SOiSetLastError(result);
    OSRestoreInterrupts(enabled);
    return result;
}

s32 SOiWaitForDHCPEx(s32 timeout) {
    s32 result = SO_SUCCESS;
    s32 gioResult;
    s32 ifError;
    s32 ifErrorSize;
    s64 limitTime;

    limitTime = 0;
    if (timeout != 0) {
        limitTime = __OSGetSystemTime() + OS_MSEC_TO_TICKS(timeout);
    }

    while (TRUE) {
        OSSleepTicks(OS_MSEC_TO_TICKS((s64)10));

        ifErrorSize = sizeof(ifError);
        gioResult = SOGetInterfaceOpt(NULL, SO_SOL_CONFIG, SO_CONFIG_ERROR,
                                      &ifError, &ifErrorSize);
        if (gioResult != SO_SUCCESS) {
            result = gioResult;
            break;
        }
        if (gioResult == SO_SUCCESS && ifError != SO_SUCCESS) {
            result = ifError;
            break;
        }
        if (SOGetHostID() != 0) {
            break;
        }
        if (limitTime != 0 && limitTime < __OSGetSystemTime()) {
            result = SO_ETIMEDOUT;
            break;
        }
    }

    return result;
}
