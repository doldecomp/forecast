#include <revolution/NWC24.h>
#include <revolution/NWC24/NWC24Internal.h>
#include <revolution/OS.h>

#include <cstddef>
#include <cstring>

#define CHECK_CALLING_STATUS(block) CheckCallingStatus(__FUNCTION__, block)

enum {
    NWC24_IOCTL_SUSPEND_SCHEDULER = 1,
    NWC24_IOCTL_TRY_SUSPEND_SCHEDULER = 2,
    NWC24_IOCTL_RESUME_SCHEDULER = 3,
    NWC24_IOCTL_STARTUP_SOCKET = 6,
    NWC24_IOCTL_CLEANUP_SOCKET = 7,
    NWC24_IOCTL_LOCK_SOCKET = 8,
    NWC24_IOCTL_UNLOCK_SOCKET = 9,
    NWC24_IOCTL_SAVE_MAIL_NOW = 13,
    NWC24_IOCTL_DOWNLOAD_NOW_EX = 14,
    NWC24_IOCTL_GENERATE_USER_ID = 15,
    NWC24_IOCTL_GET_SCHEDULER_STAT = 30,
    NWC24_IOCTL_SET_SCRIPT_MODE = 34,
};

#pragma pack(push, 1)
typedef struct CommonBuffer {
    u32 WORD_0x0; // at 0x0
    u32 WORD_0x4; // at 0x4
    u32 WORD_0x8; // at 0x8
    u8 padding[32 - 0xC];
} CommonBuffer;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct CommonResult {
    s32 result; // at 0x0

    union {
        u64 userid;
        s32 exResult;

        struct {
            u32 WORD_0x4;
            BOOL saveMail; // at 0x8
        };
    }; // at 0x4

    u32 WORD_0xC;
    u8 padding[32 - 0x10];
} CommonResult;
#pragma pack(pop)

static u32 nwc24ScdInitialized = 0;
static s32 nwc24ScdSuspendCnt = 0;
static s32 nwc24ScdOpenCnt = 0;

static OSMutex nwc24ScdCommandMutex;
static OSMutex nwc24ScdCounterMutex;

static CommonBuffer nwc24ScdCommonBuffer ALIGN(32);
static CommonResult nwc24ScdCommonResult ALIGN(32);

static u8 nwc24ScdStatBuf[256] ALIGN(32);

// Forward declarations
NWC24Err NWC24iSaveMailNow(void) DECOMP_DONT_INLINE;
NWC24Err NWC24iDownloadNowEx(BOOL* pSaveMail, s32 mode, u32 taskId,
                             s32 flags);

static NWC24Err ExecSuspendScheduler(void) DECOMP_DONT_INLINE;
static NWC24Err ExecTrySuspendScheduler(u32 arg0);
static NWC24Err ExecResumeScheduler(void) DECOMP_DONT_INLINE;
static NWC24Err ExecNoParamCommand(const char* pUser, s32 type,
                                   NWC24Err* pExResult);

static void InitScdMutex(void);

static void LockRight(void);
static BOOL TryLockRight(void);
static void UnlockRight(void);

static void LockCounters(void);
static BOOL TryLockCounters(void);
static void UnlockCounters(void);

static NWC24Err CheckCallingStatus(const char* pUser, BOOL block);

s32 NWC24SuspendScheduler(void) {
    s32 count;

    LockCounters();
    {
        count = ExecSuspendScheduler();

        if (count >= 0) {
            nwc24ScdSuspendCnt++;
            count -= nwc24ScdOpenCnt;
        }
    }
    UnlockCounters();

    return count;
}

// Not called in this program
inline s32 NWC24ResumeScheduler(void) {
    s32 count;

    LockCounters();
    {
        if (nwc24ScdOpenCnt > 0 && nwc24ScdSuspendCnt == 0) {
            count = 0;
        } else {
            count = ExecResumeScheduler();

            if (nwc24ScdSuspendCnt > 0) {
                nwc24ScdSuspendCnt--;
                count -= nwc24ScdOpenCnt;
            }
        }
    }
    UnlockCounters();

    return count;
}

NWC24Err NWC24iGetSchedulerStat(NWC24ScdStat* pStat, u32 size) {
    s32 fd;
    NWC24Err result;
    NWC24Err close;

    if (size > sizeof(nwc24ScdStatBuf)) {
        return NWC24_ERR_OVERFLOW;
    }

    if (OSGetCurrentThread() == NULL) {
        return NWC24_ERR_FATAL;
    }

    LockRight();
    {
        result = NWC24_OPEN_DEVICE(NWC24i_SCHEDULER_DEVICE, &fd, IPC_OPEN_NONE);

        if (result >= 0) {
            result = NWC24_IOCTL_DEVICE(            //
                fd, NWC24_IOCTL_GET_SCHEDULER_STAT, //
                NULL, 0,                            //
                pStat, size);

            if (result >= 0) {
                result = pStat->result;
            }

            close = NWC24_CLOSE_DEVICE(fd);
            if (result >= 0) {
                result = close;
            }
        }

        if (size > offsetof(NWC24ScdStat, newMsgFlag) + sizeof(u32)) {
            NWC24iSetNewMsgArrived(pStat->newMsgFlag);
        }
    }
    UnlockRight();

    return result;
}

NWC24Err NWC24iSetScriptMode(s32 mode) {
    NWC24Err status;

    if (OSGetCurrentThread() == NULL) {
        status = NWC24_ERR_FATAL;
    } else if (NWC24IsMsgLibOpened() || NWC24IsMsgLibOpenedByTool()) {
        status = NWC24_ERR_LIB_OPENED;
    } else if (NWC24IsMsgLibOpenBlocking()) {
        status = NWC24_ERR_BUSY;
    } else {
        status = NWC24_OK;
    }

    if (status < 0) {
        return status;
    } else {
        s32 fd;
        NWC24Err result;
        NWC24Err close;

        LockRight();
        memset(&nwc24ScdCommonBuffer, 0, sizeof(CommonBuffer));
        result =
            NWC24_OPEN_DEVICE(NWC24i_SCHEDULER_DEVICE, &fd, IPC_OPEN_NONE);

        if (result >= 0) {
            nwc24ScdCommonBuffer.WORD_0x0 = mode;

            result = NWC24_IOCTL_DEVICE(                     //
                fd, NWC24_IOCTL_SET_SCRIPT_MODE,             //
                &nwc24ScdCommonBuffer, sizeof(CommonBuffer), //
                &nwc24ScdCommonResult, sizeof(CommonResult));

            if (result >= 0) {
                result = nwc24ScdCommonResult.result;
            }

            close = NWC24_CLOSE_DEVICE(fd);
            if (result >= 0) {
                result = close;
            }
        }

        UnlockRight();
        return result;
    }
}

// Not called in this program
inline NWC24Err NWC24iRequestGenerateUserId(u64* pId, u32* arg1) {
    s32 fd;
    NWC24Err result;
    NWC24Err close;

    result = CHECK_CALLING_STATUS(TRUE);
    if (result < 0) {
        return result;
    }

    LockRight();
    {
        result = NWC24_OPEN_DEVICE(NWC24i_SCHEDULER_DEVICE, &fd, IPC_OPEN_NONE);

        if (result >= 0) {
            result = NWC24_IOCTL_DEVICE(          //
                fd, NWC24_IOCTL_GENERATE_USER_ID, //
                NULL, 0,                          //
                &nwc24ScdCommonResult, sizeof(CommonResult));

            if (result >= 0) {
                result = nwc24ScdCommonResult.result;

                if (result == NWC24_OK || result == NWC24_ERR_ID_GENERATED ||
                    result == NWC24_ERR_ID_REGISTERED) {

                    if (pId != (void*)NULL) {
                        *pId = nwc24ScdCommonResult.userid;
                    }

                    if (arg1 != (void*)NULL) {
                        *arg1 = nwc24ScdCommonResult.WORD_0xC;
                    }
                }
            }

            close = NWC24_CLOSE_DEVICE(fd);
            if (result >= 0) {
                result = close;
            }
        }
    }
    UnlockRight();

    return result;
}

NWC24Err NWC24ExecDownloadTask(s32 mode, u16 taskId, s32 flags) {
    static const char* path = "dlcnt.bin";
    NWC24Err result = NWC24_OK;
    BOOL saveMail = FALSE;
    NANDStatus stat;
    NWC24ScdStat* pStat;
    u32 errorIdx;

    if (NANDPrivateGetStatus(path, &stat) == NAND_RESULT_OK) {
        NANDPrivateDelete(path);
    }

    result = NWC24iGetSchedulerStat((NWC24ScdStat*)nwc24ScdStatBuf,
                                    sizeof(nwc24ScdStatBuf));
    if (result < 0) {
        return result;
    }

    pStat = (NWC24ScdStat*)nwc24ScdStatBuf;
    errorIdx = pStat->numErrors;

    result = NWC24iDownloadNowEx(&saveMail, mode, taskId, flags);
    if (result >= 0 && saveMail) {
        result = NWC24iSaveMailNow();
    } else if (result == NWC24_ERR_PROTECTED) {
        result = result;
    }

    if (result < 0) {
        if (NWC24iGetSchedulerStat((NWC24ScdStat*)nwc24ScdStatBuf,
                                   sizeof(nwc24ScdStatBuf)) >= 0) {
            pStat = (NWC24ScdStat*)nwc24ScdStatBuf;
            NWC24iSetErrorCode(pStat->errorLog[errorIdx]);
        } else {
            NWC24iSetErrorCode(result - 107200);
        }
    } else {
        NWC24iSetErrorCode(0);
    }

    return result;
}

NWC24Err NWC24iStartupSocket(NWC24Err* pExResult) {
    return ExecNoParamCommand(NULL, NWC24_IOCTL_STARTUP_SOCKET, pExResult);
}

NWC24Err NWC24iCleanupSocket(NWC24Err* pExResult) {
    return ExecNoParamCommand(NULL, NWC24_IOCTL_CLEANUP_SOCKET, pExResult);
}

NWC24Err NWC24iLockSocket(void) {
    return ExecNoParamCommand(NULL, NWC24_IOCTL_LOCK_SOCKET, NULL);
}

NWC24Err NWC24iUnlockSocket(void) {
    return ExecNoParamCommand(NULL, NWC24_IOCTL_UNLOCK_SOCKET, NULL);
}

NWC24Err NWC24iSaveMailNow(void) {
    NWC24Err result;

    result = CHECK_CALLING_STATUS(FALSE);
    if (result < 0) {
        return result;
    }

    return ExecNoParamCommand(NULL, NWC24_IOCTL_SAVE_MAIL_NOW, NULL);
}

NWC24Err NWC24iDownloadNowEx(BOOL* pSaveMail, s32 mode, u32 taskId,
                             s32 flags) {
    s32 fd;
    NWC24Err result;
    NWC24Err close;

    result = CHECK_CALLING_STATUS(FALSE);
    if (result < 0) {
        return result;
    }

    LockRight();
    {
        result = NWC24_OPEN_DEVICE(NWC24i_SCHEDULER_DEVICE, &fd, IPC_OPEN_NONE);

        if (result >= 0) {
            nwc24ScdCommonBuffer.WORD_0x0 = mode;
            nwc24ScdCommonBuffer.WORD_0x4 = taskId;
            nwc24ScdCommonBuffer.WORD_0x8 = flags;

            result = NWC24_IOCTL_DEVICE(                     //
                fd, NWC24_IOCTL_DOWNLOAD_NOW_EX,             //
                &nwc24ScdCommonBuffer, sizeof(CommonBuffer), //
                &nwc24ScdCommonResult, sizeof(CommonResult));

            if (result >= 0) {
                result = nwc24ScdCommonResult.result;
                *pSaveMail = nwc24ScdCommonResult.saveMail;
            }

            close = NWC24_CLOSE_DEVICE(fd);
            if (result >= 0) {
                result = close;
            }
        }
    }
    UnlockRight();

    return result;
}

NWC24Err NWC24iTrySuspendForOpenLib(void) {
    NWC24Err result;

    if (!TryLockCounters()) {
        return NWC24_ERR_MUTEX;
    }

    result = ExecTrySuspendScheduler(0);
    if (result >= 0) {
        nwc24ScdOpenCnt++;
        result = NWC24_OK;
    }

    UnlockCounters();
    return result;
}

NWC24Err NWC24iResumeForCloseLib(void) {
    NWC24Err result;

    LockCounters();
    {
        result = ExecResumeScheduler();
        if (result >= 0) {
            nwc24ScdOpenCnt--;
            result = NWC24_OK;
        }
    }
    UnlockCounters();

    return result;
}

static NWC24Err ExecSuspendScheduler(void) {
    return ExecNoParamCommand(NULL, NWC24_IOCTL_SUSPEND_SCHEDULER, NULL);
}

static NWC24Err ExecTrySuspendScheduler(u32 arg0) {
    s32 fd;
    NWC24Err result;
    NWC24Err close;

    result = CHECK_CALLING_STATUS(TRUE);
    if (result < 0) {
        return result;
    }

    if (!TryLockRight()) {
        return NWC24_ERR_MUTEX;
    }

    result = NWC24_OPEN_DEVICE(NWC24i_SCHEDULER_DEVICE, &fd, IPC_OPEN_NONE);

    if (result >= 0) {
        nwc24ScdCommonBuffer.WORD_0x0 = arg0;

        result = NWC24_IOCTL_DEVICE(                     //
            fd, NWC24_IOCTL_TRY_SUSPEND_SCHEDULER,       //
            &nwc24ScdCommonBuffer, sizeof(CommonBuffer), //
            &nwc24ScdCommonResult, sizeof(CommonResult));

        if (result >= 0) {
            result = nwc24ScdCommonResult.result;
        }

        close = NWC24_CLOSE_DEVICE(fd);
        if (close < 0) {
            result = close;
        }
    }

    UnlockRight();
    return result;
}

static NWC24Err ExecResumeScheduler(void) {
    return ExecNoParamCommand(NULL, NWC24_IOCTL_RESUME_SCHEDULER, NULL);
}

static NWC24Err ExecNoParamCommand(const char* pUser, s32 type,
                                   NWC24Err* pExResult) {
    s32 fd;
    NWC24Err result;
    NWC24Err close;

    if (OSGetCurrentThread() == NULL) {
        return NWC24_ERR_FATAL;
    }

    LockRight();
    {
        result = NWC24iOpenResourceManager(pUser, NWC24i_SCHEDULER_DEVICE, &fd,
                                           IPC_OPEN_NONE);

        if (result >= 0) {
            result = NWC24iIoctlResourceManager(pUser, fd, type, NULL, 0,
                                                &nwc24ScdCommonResult,
                                                sizeof(CommonResult));

            if (result >= 0) {
                result = nwc24ScdCommonResult.result;

                if (result == NWC24_ERR_FAILED ||
                    result == NWC24_ERR_CONFIG_NETWORK) {

                    if (pExResult != (void*)NULL) {
                        *pExResult = nwc24ScdCommonResult.exResult;
                    }
                }
            }

            close = NWC24iCloseResourceManager(pUser, fd);
            if (close < 0) {
                result = close;
            }
        }
    }
    UnlockRight();

    return result;
}

static void InitScdMutex(void) {
    BOOL enabled = OSDisableInterrupts();

    if (!(nwc24ScdInitialized & 1)) {
        OSInitMutex(&nwc24ScdCommandMutex);
        OSInitMutex(&nwc24ScdCounterMutex);

        memset(&nwc24ScdCommonBuffer, 0, sizeof(CommonBuffer));
        memset(&nwc24ScdCommonResult, 0, sizeof(CommonResult));

        nwc24ScdInitialized |= 1;
    }

    OSRestoreInterrupts(enabled);
}

static void LockRight(void) {
    if (!(nwc24ScdInitialized & 1)) {
        InitScdMutex();
    }

    OSLockMutex(&nwc24ScdCommandMutex);
}

static BOOL TryLockRight(void) {
    if (!(nwc24ScdInitialized & 1)) {
        InitScdMutex();
    }

    return OSTryLockMutex(&nwc24ScdCommandMutex);
}

static void UnlockRight(void) {
    OSUnlockMutex(&nwc24ScdCommandMutex);
}

static void LockCounters(void) {
    if (!(nwc24ScdInitialized & 1)) {
        InitScdMutex();
    }

    OSLockMutex(&nwc24ScdCounterMutex);
}

static BOOL TryLockCounters(void) {
    if (!(nwc24ScdInitialized & 1)) {
        InitScdMutex();
    }

    return OSTryLockMutex(&nwc24ScdCounterMutex);
}

static void UnlockCounters(void) {
    OSUnlockMutex(&nwc24ScdCounterMutex);
}

static NWC24Err CheckCallingStatus(const char* pUser, BOOL block) {
#pragma unused(pUser)

    if (OSGetCurrentThread() == NULL) {
        return NWC24_ERR_FATAL;
    }

    if (NWC24IsMsgLibOpened() || NWC24IsMsgLibOpenedByTool()) {
        return NWC24_ERR_LIB_OPENED;
    }

    if (!block && NWC24IsMsgLibOpenBlocking()) {
        return NWC24_ERR_BUSY;
    }

    return NWC24_OK;
}
