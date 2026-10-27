#include <revolution/IPC.h>
#include <revolution/NCD.h>
#include <revolution/NWC24.h>
#include <revolution/OS.h>
#include <revolution/SO.h>
#include <cstring>

// NWC24 socket helpers (no public header)
s32 NWC24iStartupSocket(s32* pExError);
s32 NWC24iCleanupSocket(s32* pExError);
s32 NWC24iLockSocket(void);
s32 NWC24iUnlockSocket(void);

typedef enum {
    SO_INTERNAL_STATE_TERMINATED = 0,
    SO_INTERNAL_STATE_READY = 1,
    SO_INTERNAL_STATE_ACTIVE = 2
} SOInternalState;

// Size of the DNS resolver's work area
#define SO_RESOLVER_SIZE 0x460

const char* __SO_VERSION =
    "<< RVL_SDK - SO \trelease build: Jun 28 2007 18:29:42 (0x4199_60831) >>";

static u8 soState = SO_INTERNAL_STATE_TERMINATED;
static SOSysWork soWork;
static s32 soError = SO_SUCCESS;
static BOOL soRegistered = FALSE;
static BOOL soBufAddrCheck = TRUE;
static char NET_RM_SOCK[] = "/dev/net/ip/top";

static void SOiSetError(int result) {
    OSThread* thread = OSGetCurrentThread();

    if (thread != NULL) {
        thread->error = result;
    } else {
        soError = result;
    }
}

static BOOL SOiIsBufferAddrCheck(void) {
    return soBufAddrCheck;
}

int SOInit(const SOLibraryConfig* config) {
    int result = SO_SUCCESS;
    BOOL enabled = OSDisableInterrupts();

    if (!soRegistered) {
        OSRegisterVersion(__SO_VERSION);
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

        memset(&soWork, 0, sizeof(SOSysWork));
        soWork.alloc = config->alloc;
        soWork.free = config->free;
        soWork.allocCount = 0;
        soWork.rmState = SO_INTERNAL_RM_STATE_CLOSED;
        soWork.rmFd = -1;

        soWork.pResolver = SOiAlloc(SO_MEM_RESOLVER, SO_RESOLVER_SIZE);
        if (soWork.pResolver == NULL) {
            result = SO_ENOMEM;
        }
        if (soWork.pResolver != NULL) {
            soState = SO_INTERNAL_STATE_READY;
        }
        break;
    }

    SOiSetError(result);
    OSRestoreInterrupts(enabled);
    return result;
}

int SOStartup(void) {
    return SOStartupEx(10 * 60 * 1000);
}

static inline int SOiConvertNWC24Error(s32 errNwc24, s32 exError) {
    int result = SO_EINVAL;

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
        result = exError;
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

int SOStartupEx(int timeOut) {
    int result;
    BOOL enabled;
    s32 exError;
    s64 limitTime;
    s32 linkupRetry;

    limitTime = 0;
    if (timeOut != 0) {
        limitTime = __OSGetSystemTime() + OS_MSEC_TO_TICKS(timeOut);
    }

    linkupRetry = 4;

retry:
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
        } else if (OSGetCurrentThread() == NULL) {
            result = SO_EFATAL;
            break;
        } else {
            s32 linkStatus;

            soWork.rmState = SO_INTERNAL_RM_STATE_WORKING;
            OSRestoreInterrupts(enabled);

            while (TRUE) {
                linkStatus = NCDGetLinkStatus();
                if (linkStatus != NCD_RESULT_INPROGRESS &&
                    linkStatus != NCD_LINKSTATUS_WORKING) {
                    break;
                }

                OSSleepTicks(OS_MSEC_TO_TICKS((s64)100));
                if (limitTime != 0 && limitTime < __OSGetSystemTime()) {
                    if (linkStatus == NCD_LINKSTATUS_WORKING) {
                        result = SO_ERR_LINK_UP_TIMEOUT;
                    } else {
                        result = SO_EFATAL;
                    }
                    goto change_state;
                }
            }

            if (linkStatus < NCD_RESULT_SUCCESS) {
                result = SO_EFATAL;
                goto change_state;
            } else if (linkStatus == NCD_LINKSTATUS_NONE) {
                result = SO_ENOENT;
                goto change_state;
            }

            while (TRUE) {
                soWork.rmFd = IOS_Open(NET_RM_SOCK, IPC_OPEN_NONE);
                if (soWork.rmFd != IPC_RESULT_NOEXISTS_INTERNAL) {
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
                s32 errNwc24;

                result = SO_SUCCESS;
                exError = SO_SUCCESS;

                while (TRUE) {
                    errNwc24 = NWC24iStartupSocket(&exError);
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
                    result = SOiConvertNWC24Error(errNwc24, exError);
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

    SOiSetError(result);
    OSRestoreInterrupts(enabled);

    if (result == SO_SUCCESS) {
        s64 dhcpTimeOut;

        if (limitTime != 0) {
            dhcpTimeOut = limitTime - __OSGetSystemTime();
        } else {
            dhcpTimeOut = 0;
        }

        if (limitTime != 0 && dhcpTimeOut <= 0) {
            result = SO_ETIMEDOUT;
        } else {
            result = SOiWaitForDHCPEx(
                (int)(dhcpTimeOut / OS_MSEC_TO_TICKS(1)));
        }

        if (result != SO_SUCCESS) {
            SOCleanup();
        }
    }

    SOiSetError(result);

    if (result == SO_ERR_LINK_DOWN) {
        linkupRetry--;
        if (linkupRetry >= 0) {
            goto retry;
        }
    }

    return result;
}

int SOCleanup(void) {
    int result = SO_SUCCESS;
    BOOL enabled = OSDisableInterrupts();
    s32 exError = SO_SUCCESS;
    s32 errNwc24;

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
        } else if (OSGetCurrentThread() == NULL) {
            result = SO_EFATAL;
            break;
        } else {
            soWork.rmState = SO_INTERNAL_RM_STATE_WORKING;
            OSRestoreInterrupts(enabled);

            errNwc24 = NWC24iCleanupSocket(&exError);
            result = SOiConvertNWC24Error(errNwc24, exError);
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

    SOiSetError(result);
    OSRestoreInterrupts(enabled);
    return result;
}

void* SOiAlloc(u32 name, s32 size) {
    if (size > 0 && soWork.alloc != NULL) {
        void* ptr = soWork.alloc(name, size);

        if (ptr != NULL) {
            soWork.allocCount++;

            // IOS can only access MEM2
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
    if (ptr != NULL && soWork.free != NULL) {
        soWork.allocCount--;
        soWork.free(name, ptr, size);
    }
}

int SOiPrepare(const char* funcName, s32* pRmId) {
    int result = SO_SUCCESS;
    BOOL enabled = OSDisableInterrupts();
#pragma unused(funcName)

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
        } else if (OSGetCurrentThread() == NULL) {
            result = SO_EFATAL;
            break;
        }

        *pRmId = soWork.rmFd;
        break;
    }

    if (result != SO_SUCCESS) {
        SOiSetError(result);
    }

    OSRestoreInterrupts(enabled);
    return result;
}

int SOiConclude(const char* funcName, int result) {
    BOOL enabled = OSDisableInterrupts();
#pragma unused(funcName)

    SOiSetError(result);
    OSRestoreInterrupts(enabled);
    return result;
}

int SOiPrepareTempRm(const char* funcName, s32* pRmId, BOOL* pIsTempRm) {
    int result = SO_SUCCESS;
    BOOL enabled = OSDisableInterrupts();
    s32 errNwc24;

    switch (soState) {
    case SO_INTERNAL_STATE_TERMINATED:
        result = SO_ENETRESET;
        break;
    case SO_INTERNAL_STATE_READY:
    default:
        if (soWork.rmState > SO_INTERNAL_RM_STATE_CLOSED) {
            result = SO_EBUSY;
            break;
        } else if (OSGetCurrentThread() == NULL) {
            result = SO_EFATAL;
            break;
        }

        soWork.rmState = SO_INTERNAL_RM_STATE_WORKING;
        OSRestoreInterrupts(enabled);

        soWork.rmFd = IOS_Open(NET_RM_SOCK, IPC_OPEN_NONE);
        if (soWork.rmFd < 0) {
            enabled = OSDisableInterrupts();
            if (soWork.rmFd == IPC_RESULT_NOEXISTS_INTERNAL) {
                result = SO_EINPROGRESS;
                soWork.rmState = SO_INTERNAL_RM_STATE_CLOSED;
            } else {
                result = SO_EFATAL;
            }
        } else {
            *pRmId = soWork.rmFd;

            if ((errNwc24 = NWC24iLockSocket()) == NWC24_OK) {
                switch (NCDGetLinkStatus()) {
                case NCD_LINKSTATUS_WIRED:
                case NCD_LINKSTATUS_WIRELESS_DOWN:
                case NCD_LINKSTATUS_WIRELESS_UP:
                    result = IOS_Ioctl(soWork.rmFd, SO_IOCTL_STARTUP, NULL, 0, NULL, 0);
                    if (result == SO_SUCCESS) {
                        *pIsTempRm = TRUE;
                    } else {
                        result = SOiConcludeTempRm(funcName, result, TRUE);
                    }
                    break;
                case NCD_RESULT_INPROGRESS:
                    result = SOiConcludeTempRm(funcName, SO_EINPROGRESS, TRUE);
                    break;
                case NCD_RESULT_FATAL:
                case NCD_RESULT_IPC_ERROR:
                    result = SOiConcludeTempRm(funcName, SO_EFATAL, TRUE);
                    break;
                default:
                    result = SOiConcludeTempRm(funcName, SO_ENOLINK, TRUE);
                    break;
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
        } else if (OSGetCurrentThread() == NULL) {
            result = SO_EFATAL;
            break;
        }

        *pIsTempRm = FALSE;
        *pRmId = soWork.rmFd;
        break;
    }

    if (result != SO_SUCCESS) {
        SOiSetError(result);
    }

    OSRestoreInterrupts(enabled);
    return result;
}

int SOiConcludeTempRm(const char* funcName, int result, BOOL isTempRm) {
    BOOL enabled;
#pragma unused(funcName)

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
    } else {
        enabled = OSDisableInterrupts();
    }

    SOiSetError(result);
    OSRestoreInterrupts(enabled);
    return result;
}

int SOiWaitForDHCPEx(int timeOut) {
    int result = SO_SUCCESS;
    int optResult;
    int ifError;
    int ifErrorLen;
    s64 limitTime;

    limitTime = 0;
    if (timeOut != 0) {
        limitTime = __OSGetSystemTime() + OS_MSEC_TO_TICKS(timeOut);
    }

    while (TRUE) {
        OSSleepTicks(OS_MSEC_TO_TICKS((s64)10));

        ifErrorLen = sizeof(ifError);
        optResult = SOGetInterfaceOpt(NULL, SO_SOL_CONFIG, SO_CONFIG_ERROR,
                                      &ifError, &ifErrorLen);
        if (optResult != SO_SUCCESS) {
            result = optResult;
            break;
        }

        if (optResult == SO_SUCCESS && ifError != SO_SUCCESS) {
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
