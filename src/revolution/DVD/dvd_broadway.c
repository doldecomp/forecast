#include <revolution/DVD.h>
#include <revolution/ESP.h>
#include <revolution/ESP/esp.h>
#include <revolution/IPC.h>
#include <revolution/OS.h>

#include <cstring>

#define DVD_LOW_CTX_MAX 4
#define DVD_LOW_CMD_MAX 4
#define DVD_LOW_CTX_MAGIC 0xFEEBDAED

// Your guess is as good as mine
#define DVD_LOW_CTX_IN_USE(ctx) ((u8)((ctx).inUse != FALSE))

#define DVD_LOW_H3_HASH_SIZE 0x18000

typedef enum {
    DVD_IOCTL_INQUIRY = 0x12,
    DVD_IOCTL_READ_DISK_ID = 0x70,
    DVD_IOCTL_READ = 0x71,
    DVD_IOCTL_PREPARE_COVER_REGISTER = 0x7A,
    DVD_IOCTL_CLEAR_COVER_INTERRUPT = 0x86,
    DVD_IOCTL_RESET = 0x8A,
    DVD_IOCTL_CLOSE_PARTITION = 0x8C,
    DVD_IOCTL_UNENCRYPTED_READ = 0x8D,
    DVD_IOCTL_PREPARE_STATUS_REGISTER = 0x95,
    DVD_IOCTL_SEEK = 0xAB,
    DVD_IOCTL_SET_MAX_ROTATION = 0xDD,
    DVD_IOCTL_REQUEST_ERROR = 0xE0,
    DVD_IOCTL_STOP_MOTOR = 0xE3,
    DVD_IOCTL_AUDIO_BUFFER_CONFIG = 0xE4,
} DVDLowIoctl;

typedef enum {
    DVD_IOCTLV_OPEN_PARTITION = 0x8B,
    DVD_IOCTLV_GET_NO_DISC_OPEN_PARTITION_PARAMS = 0x90,
    DVD_IOCTLV_GET_NO_DISC_BUFFER_SIZES = 0x92,
    DVD_IOCTLV_OPEN_PARTITION_WITH_TMD_AND_TICKET_VIEW = 0x94,
} DVDLowIoctlv;

typedef struct DVDLowContext {
    DVDLowCallback callback; // at 0x0
    UNKWORD WORD_0x4;
    u8 inUse;  // at 0x8
    u32 magic; // at 0xC
    u32 id;    // at 0x10
    char padding[32 - 0x14];
} DVDLowContext;

typedef struct DVDLowDICommand {
    u8 command; // at 0x0
    u32 arg1;   // at 0x4
    u32 arg2;   // at 0x8
    u32 arg3;   // at 0xC
    u32 arg4;   // at 0x10
    u32 arg5;   // at 0x14
    u32 arg6;   // at 0x18
    u32 arg7;   // at 0x1C
} DVDLowDICommand;

typedef struct DVDLowDiRegs {
    u32 diimmbuf; // at 0x0
    u32 dicvr;    // at 0x4
    char padding[32 - 0x8];
} DVDLowDiRegs;

typedef struct DVDLowRegBuffer {
    u32 reg; // at 0x0
    char padding[32 - 0x4];
} DVDLowRegBuffer;

static u8 requestInProgress = FALSE;
static u8 breakRequested;
static u8 callbackInProgress = FALSE;

s32 DiFD = -1;

static s32 freeCommandBuf = 0;

static s32 freeDvdContext = 0;
static u8 dvdContextsInited = FALSE;
static u8 DVDLowInitCalled = FALSE;
static DVDLowDICommand* diCommand;
static char* pathBuf;

static u32 readLength;
static BOOL spinUpValue;

static DVDLowContext dvdContexts[DVD_LOW_CTX_MAX] ALIGN(32) = {0};
static DVDLowDiRegs diRegValCache ALIGN(32) = {0};
static DVDLowRegBuffer registerBuf ALIGN(32) = {0};
static IPCIOVector ioVec[10] ALIGN(32) = {0};
static u8 lastTicketError[32] ALIGN(32) = {0};
// Only used by functions that are not in this program
DVDLowRegBuffer coverStatus ALIGN(32) = {0};
DVDLowRegBuffer coverRegister ALIGN(32) = {0};
static DVDLowRegBuffer statusRegister ALIGN(32) = {0};

static void nextCommandBuf(void);
static DVDLowContext* newContext(DVDLowCallback callback, UNKWORD arg2);

static void* ddrAllocAligned32(size_t size) {
    u8* lo = IPCGetBufferLo();
    u8* hi = IPCGetBufferHi();

    // @bug Incorrect rounding (& 31) results in pointer mod 32
    if ((u32)lo % 32 != 0) {
        lo = (u8*)(((u32)lo + 31) & 31);
    }

    if (lo + size > hi) {
        OSReport("(ddrAllocAligned32) Not enough space to allocate %d bytes\n",
                 size);
    }

    IPCSetBufferLo(lo + size);
    return lo;
}

static BOOL allocateStructures(void) {
    diCommand = ddrAllocAligned32(sizeof(DVDLowDICommand) * DVD_LOW_CMD_MAX);
    if (diCommand == NULL) {
        OSReport("Allocation of diCommand blocks failed\n");
        return FALSE;
    }

    pathBuf = ddrAllocAligned32(32);
    if (pathBuf == NULL) {
        OSReport("Allocation of pathBuf failed\n");
        return FALSE;
    }

    return TRUE;
}

static void initDvdContexts(void) {
    int i;
    for (i = 0; i < DVD_LOW_CTX_MAX; i++) {
        dvdContexts[i].callback = NULL;
        dvdContexts[i].WORD_0x4 = 0;
        dvdContexts[i].inUse = FALSE;
        dvdContexts[i].magic = DVD_LOW_CTX_MAGIC;
        dvdContexts[i].id = i;
    }

    freeDvdContext = 0;
}

static s32 doTransactionCallback(s32 intType, void* arg) {
    DVDLowContext* ctx = (DVDLowContext*)arg;

    if (ctx->magic != DVD_LOW_CTX_MAGIC) {
        OSReport("(doTransactionCallback) Error - context mangled!\n");
        ctx->magic = DVD_LOW_CTX_MAGIC;
    } else {
        requestInProgress = FALSE;

        if (ctx->callback != NULL) {
            callbackInProgress = TRUE;

            if (breakRequested == TRUE) {
                breakRequested = FALSE;
                intType |= DVD_INTTYPE_BR;
            }

            if (intType & DVD_INTTYPE_TC) {
                readLength = 0;
            }

            ctx->callback(intType);

            callbackInProgress = FALSE;
        }
    }

    ctx->inUse = FALSE;
    return IPC_RESULT_OK;
}

// Message of doCoverCallback (not in this program)
char sCoverCallbackMangledMsg[] = "(doCoverCallback) Error - context mangled!\n";

static s32 doPrepareCoverRegisterCallback(s32 intType, void* arg) {
    DVDLowContext* ctx = (DVDLowContext*)arg;

    requestInProgress = FALSE;
    diRegValCache.dicvr = registerBuf.reg;

    if (ctx->magic != DVD_LOW_CTX_MAGIC) {
        OSReport("(doTransactionCallback) Error - context mangled!\n");
        ctx->magic = DVD_LOW_CTX_MAGIC;
    } else {
        if (ctx->callback != NULL) {
            callbackInProgress = TRUE;

            if (breakRequested == TRUE) {
                breakRequested = FALSE;
                intType |= DVD_INTTYPE_BR;
            }

            ctx->callback(intType);

            callbackInProgress = FALSE;
        }
    }

    ctx->inUse = FALSE;
    return IPC_RESULT_OK;
}

BOOL DVDLowFinalize(void) {
    if (IOS_Close(DiFD) != IPC_RESULT_OK) {
        OSReport("(DVDLowFinish) Error: IOS_Close failed\n");
        return FALSE;
    }

    DVDLowInitCalled = FALSE;
    return TRUE;
}

BOOL DVDLowInit(void) {
    IPCResult result;

    if (!DVDLowInitCalled) {
        DVDLowInitCalled = TRUE;

        result = IPCCltInit();
        if (result != IPC_RESULT_OK) {
            OSReport("IPCCltInit returned error: %d\n", result);
            return FALSE;
        }

        if (!allocateStructures()) {
            return FALSE;
        }

        if (!dvdContextsInited) {
            initDvdContexts();
            dvdContextsInited = TRUE;
        }
    }

    strncpy(pathBuf, "/dev/di", 32);
    DiFD = IOS_Open(pathBuf, IPC_OPEN_NONE);

    if (DiFD >= 0) {
        return TRUE;
    }

    switch (DiFD) {
    case IPC_RESULT_NOEXISTS_INTERNAL:
        OSReport("(DVDLowInit) Error: IOS_Open failed - pathname '/dev/di' "
                 "does not exist\n");
        return FALSE;
    case IPC_RESULT_ACCESS_INTERNAL:
        OSReport("(DVDLowInit) Error: IOS_Open failed - calling thread lacks "
                 "permission\n");
        return FALSE;
    case IPC_RESULT_CONN_MAX_INTERNAL:
        OSReport("(DVDLowInit) Error: IOS_Open failed - connection limit has "
                 "been reached\n");
        return FALSE;
    default:
        OSReport("(DVDLowInit) IOS_Open failed, errorcode = %d\n", DiFD);
        return FALSE;
    }
}

BOOL DVDLowReadDiskID(DVDDiskID* out, DVDLowCallback callback) {
    DVDLowContext* ctx;
    IPCResult result;

    requestInProgress = TRUE;
    ctx = newContext(callback, 1);

    if (out == NULL) {
        OSReport("@@@@@@ WARNING - Calling DVDLowReadDiskId with NULL ptr\n");
    }

    nextCommandBuf();
    diCommand[freeCommandBuf].command = DVD_IOCTL_READ_DISK_ID;

    result = IOS_IoctlAsync(DiFD, DVD_IOCTL_READ_DISK_ID,
                            &diCommand[freeCommandBuf], sizeof(DVDLowDICommand),
                            out, sizeof(DVDDiskID), doTransactionCallback, ctx);

    if (result != IPC_RESULT_OK) {
        OSReport("@@@ (DVDLowReadDiskID) IOS_IoctlAsync returned error: %d\n",
                 result);
        ctx->inUse = FALSE;

        return FALSE;
    }

    return TRUE;
}

static void nextCommandBuf(void) {
    if (++freeCommandBuf >= DVD_LOW_CMD_MAX) {
        freeCommandBuf = 0;
    }
}

static DECOMP_INLINE DVDLowContext* newContext(DVDLowCallback callback,
                                               UNKWORD arg2) {
    s32 id;

    // The last operation somehow did not complete
    if (DVD_LOW_CTX_IN_USE(dvdContexts[freeDvdContext]) == TRUE) {
        OSReport("(newContext) ERROR: freeDvdContext.inUse (#%d) is true\n",
                 freeDvdContext);
        OSReport("(newContext) Now spinning in infinite loop\n");

        while (TRUE) {
            ;
        }
    }

    // Memory corruption
    if (dvdContexts[freeDvdContext].magic != DVD_LOW_CTX_MAGIC) {
        OSReport(
            "(newContext) Something overwrote the context magic - spinning \n");

        while (TRUE) {
            ;
        }
    }

    dvdContexts[freeDvdContext].callback = callback;
    dvdContexts[freeDvdContext].WORD_0x4 = arg2;
    dvdContexts[freeDvdContext].inUse = TRUE;

    id = freeDvdContext;

    if (++freeDvdContext >= DVD_LOW_CTX_MAX) {
        freeDvdContext = 0;
    }

    return &dvdContexts[id];
}

BOOL DVDLowOpenPartition(u32 offset, const ESPTicket* ticket, u32 certsSize,
                         const void* certs, ESPTmd* tmd,
                         DVDLowCallback callback) {
    DVDLowContext* ctx;
    IPCResult result;

    if (ticket != NULL && (u32)ticket % 32 != 0) {
        OSReport("(DVDLowOpenPartition) eTicket memory is unaligned\n");
        return FALSE;
    }

    if (certs != NULL && (u32)certs % 32 != 0) {
        OSReport("(DVDLowOpenPartition) certificates memory is unaligned\n");
        return FALSE;
    }

    if (tmd != NULL && (u32)tmd % 32 != 0) {
        OSReport("(DVDLowOpenPartition) certificates memory is unaligned\n");
        return FALSE;
    }

    requestInProgress = TRUE;
    ctx = newContext(callback, 1);

    nextCommandBuf();
    diCommand[freeCommandBuf].command = DVD_IOCTLV_OPEN_PARTITION;
    diCommand[freeCommandBuf].arg1 = offset;

    // Input vector 1: DI command
    ioVec[0].base = &diCommand[freeCommandBuf];
    ioVec[0].length = sizeof(DVDLowDICommand);

    // Input vector 2: eTicket
    ioVec[1].base = (void*)ticket;
    if (ticket == NULL) {
        ioVec[1].length = 0;
    } else {
        ioVec[1].length = sizeof(ESPTicket);
    }

    // Input vector 3: Shared certs
    ioVec[2].base = (void*)certs;
    if (certs == NULL) {
        ioVec[2].length = 0;
    } else {
        ioVec[2].length = certsSize;
    }

    // Output vector 1: TMD
    ioVec[3].base = tmd;
    ioVec[3].length = sizeof(ESPTmd);

    // Output vector 2: Ticket error
    ioVec[4].base = &lastTicketError;
    ioVec[4].length = sizeof(lastTicketError);

    result = IOS_IoctlvAsync(DiFD, DVD_IOCTLV_OPEN_PARTITION, 3, 2, ioVec,
                             doTransactionCallback, ctx);

    if (result != IPC_RESULT_OK) {
        OSReport(
            "@@@ (DVDLowOpenPartition) IOS_IoctlvAsync returned error: %d\n",
            result);
        ctx->inUse = FALSE;

        return FALSE;
    }

    return TRUE;
}

// Messages of DVDLowOpenPartitionWithTmdAndTicket (not in this program)
char sOpenPartitionWithTmdAndTicketName[] =
    "DVDLowOpenPartitionWithTmdAndTicket";
char sTicketUnalignedMsg[] = "(%s) eTicket memory is unaligned\n";
char sTmdNullMsg[] = "(%s) tmd parameter cannot be NULL\n";
char sTmdUnalignedMsg[] = "(%s) tmd memory is unaligned\n";
char sTicketNullMsg[] = "(%s) eTicket parameter cannot be NULL\n";

BOOL DVDLowOpenPartitionWithTmdAndTicketView(u32 offset,
                                             const ESTicketView* ticketView,
                                             u32 tmdSize, const ESTitleMeta* tmd,
                                             u32 certsSize, const u8* certs,
                                             DVDLowCallback callback) {
    DVDLowContext* ctx;
    IPCResult result;

    if (certs != NULL && (u32)certs % 32 != 0) {
        return FALSE;
    }

    if (tmd == NULL) {
        OSReport(sTmdNullMsg, __FUNCTION__);
        return FALSE;
    } else if ((u32)tmd % 32 != 0) {
        OSReport(sTmdUnalignedMsg, __FUNCTION__);
        return FALSE;
    }

    if (ticketView == NULL) {
        OSReport("(%s) eTicketView parameter cannot be NULL\n", __FUNCTION__);
        return FALSE;
    } else if ((u32)ticketView % 32 != 0) {
        OSReport("(%s) eTicketView memory is unaligned\n", __FUNCTION__);
        return FALSE;
    }

    requestInProgress = TRUE;
    ctx = newContext(callback, 1);

    nextCommandBuf();
    diCommand[freeCommandBuf].command =
        DVD_IOCTLV_OPEN_PARTITION_WITH_TMD_AND_TICKET_VIEW;
    diCommand[freeCommandBuf].arg1 = offset;

    // Input vector 1: DI command
    ioVec[0].base = &diCommand[freeCommandBuf];
    ioVec[0].length = sizeof(DVDLowDICommand);

    // Input vector 2: eTicket view
    ioVec[1].base = (void*)ticketView;
    ioVec[1].length = sizeof(ESTicketView);

    // Input vector 3: TMD
    ioVec[2].base = (void*)tmd;
    ioVec[2].length = tmdSize;

    // Input vector 4: Shared certs
    ioVec[3].base = (void*)certs;
    if (certs == NULL) {
        ioVec[3].length = 0;
    } else {
        ioVec[3].length = certsSize;
    }

    // Output vector 1: Ticket error
    ioVec[4].base = &lastTicketError;
    ioVec[4].length = sizeof(lastTicketError);

    result = IOS_IoctlvAsync(DiFD,
                             DVD_IOCTLV_OPEN_PARTITION_WITH_TMD_AND_TICKET_VIEW,
                             4, 1, ioVec, doTransactionCallback, ctx);

    if (result != IPC_RESULT_OK) {
        OSReport(
            "@@@ (DVDLowOpenPartition) IOS_IoctlvAsync returned error: %d\n",
            result);
        ctx->inUse = FALSE;

        return FALSE;
    }

    return TRUE;
}

BOOL DVDLowGetNoDiscBufferSizes(u32 offset, u32* tmdSize, u32* certsSize,
                                DVDLowCallback callback) {
    DVDLowContext* ctx;
    IPCResult result;

    if (tmdSize == NULL || certsSize == NULL) {
        OSReport("(%s) Error: NULL pointer argument\n", __FUNCTION__);
        return FALSE;
    }

    if ((u32)tmdSize % 32 != 0) {
        OSReport("(%s) numTmdBytes memory is unaligned\n", __FUNCTION__);
        return FALSE;
    }

    if ((u32)certsSize % 32 != 0) {
        OSReport("(%s) certificates memory is unaligned\n", __FUNCTION__);
        return FALSE;
    }

    requestInProgress = TRUE;
    ctx = newContext(callback, 1);

    nextCommandBuf();
    diCommand[freeCommandBuf].command = DVD_IOCTLV_GET_NO_DISC_BUFFER_SIZES;
    diCommand[freeCommandBuf].arg1 = offset;

    // Input vector 1: DI command
    ioVec[0].base = &diCommand[freeCommandBuf];
    ioVec[0].length = sizeof(DVDLowDICommand);

    // Output vector 1: TMD size
    ioVec[1].base = tmdSize;
    ioVec[1].length = sizeof(u32);

    // Output vector 2: Shared certs size
    ioVec[2].base = certsSize;
    ioVec[2].length = sizeof(u32);

    result = IOS_IoctlvAsync(DiFD, DVD_IOCTLV_GET_NO_DISC_BUFFER_SIZES, 1, 2,
                             ioVec, doTransactionCallback, ctx);

    if (result != IPC_RESULT_OK) {
        OSReport("@@@ (%s) IOS_IoctlvAsync returned error: %d\n", __FUNCTION__,
                 result);
        ctx->inUse = FALSE;

        return FALSE;
    }

    return TRUE;
}

#define IS_ALIGNED_32(x) ((u32)(x) % 32 == 0)

BOOL DVDLowGetNoDiscOpenPartitionParams(u32 offset, ESTicket* ticket,
                                        u32* tmdSize, ESTitleMeta* tmd,
                                        u32* certsSize, u8* certs,
                                        u32* dataWordOffset, u8* h3Hash,
                                        DVDLowCallback callback) {
    DVDLowContext* ctx;
    IPCResult result;

    if (ticket == NULL || tmdSize == NULL || tmd == NULL || certsSize == NULL ||
        certs == NULL || dataWordOffset == NULL || h3Hash == NULL) {
        OSReport("(%s) Error: NULL pointer argument\n", __FUNCTION__);
        return FALSE;
    }

    if (!IS_ALIGNED_32(ticket) || !IS_ALIGNED_32(tmdSize) ||
        !IS_ALIGNED_32(tmd) || !IS_ALIGNED_32(certsSize) ||
        !IS_ALIGNED_32(certs) || !IS_ALIGNED_32(dataWordOffset) ||
        !IS_ALIGNED_32(h3Hash)) {
        OSReport("(%s) pointer argument is unaligned\n", __FUNCTION__);
        return FALSE;
    }

    requestInProgress = TRUE;
    ctx = newContext(callback, 1);

    nextCommandBuf();
    diCommand[freeCommandBuf].command =
        DVD_IOCTLV_GET_NO_DISC_OPEN_PARTITION_PARAMS;
    diCommand[freeCommandBuf].arg1 = offset;

    // Input vector 1: DI command
    ioVec[0].base = &diCommand[freeCommandBuf];
    ioVec[0].length = sizeof(DVDLowDICommand);

    // Input vector 2: TMD size
    ioVec[1].base = tmdSize;
    ioVec[1].length = sizeof(u32);

    // Input vector 3: Shared certs size
    ioVec[2].base = certsSize;
    ioVec[2].length = sizeof(u32);

    // Output vector 1: eTicket
    ioVec[3].base = ticket;
    ioVec[3].length = sizeof(ESTicket);

    // Output vector 2: TMD size
    ioVec[4].base = tmdSize;
    ioVec[4].length = sizeof(u32);

    // Output vector 3: TMD
    ioVec[5].base = tmd;
    ioVec[5].length = *tmdSize;

    // Output vector 4: Shared certs size
    ioVec[6].base = certsSize;
    ioVec[6].length = sizeof(u32);

    // Output vector 5: Shared certs
    ioVec[7].base = certs;
    ioVec[7].length = *certsSize;

    // Output vector 6: Data offset
    ioVec[8].base = dataWordOffset;
    ioVec[8].length = sizeof(u32);

    // Output vector 7: H3 hash table
    ioVec[9].base = h3Hash;
    ioVec[9].length = DVD_LOW_H3_HASH_SIZE;

    result = IOS_IoctlvAsync(DiFD, DVD_IOCTLV_GET_NO_DISC_OPEN_PARTITION_PARAMS,
                             3, 7, ioVec, doTransactionCallback, ctx);

    if (result != IPC_RESULT_OK) {
        OSReport("@@@ (%s) IOS_IoctlvAsync returned error: %d\n", __FUNCTION__,
                 result);
        ctx->inUse = FALSE;

        return FALSE;
    }

    return TRUE;
}

// Name of DVDLowNoDiscOpenPartition (not in this program)
char sNoDiscOpenPartitionName[] = "DVDLowNoDiscOpenPartition";

BOOL DVDLowClosePartition(DVDLowCallback callback) {
    DVDLowContext* ctx;
    IPCResult result;

    nextCommandBuf();
    diCommand[freeCommandBuf].command = DVD_IOCTL_CLOSE_PARTITION;

    requestInProgress = TRUE;
    ctx = newContext(callback, 1);

    result = IOS_IoctlAsync(DiFD, DVD_IOCTL_CLOSE_PARTITION,
                            &diCommand[freeCommandBuf], sizeof(DVDLowDICommand),
                            NULL, 0, doTransactionCallback, ctx);

    if (result != IPC_RESULT_OK) {
        OSReport(
            "@@@ (DVDLowClosePartition) IOS_IoctlAsync returned error: %d\n",
            result);
        ctx->inUse = FALSE;

        return FALSE;
    }

    return TRUE;
}

BOOL DVDLowUnencryptedRead(void* dst, u32 size, u32 offset,
                           DVDLowCallback callback) {
    DVDLowContext* ctx;
    IPCResult result;

    requestInProgress = TRUE;
    ctx = newContext(callback, 1);

    readLength = size;

    nextCommandBuf();
    diCommand[freeCommandBuf].command = DVD_IOCTL_UNENCRYPTED_READ;
    diCommand[freeCommandBuf].arg1 = size;
    diCommand[freeCommandBuf].arg2 = offset;

    result = IOS_IoctlAsync(DiFD, DVD_IOCTL_UNENCRYPTED_READ,
                            &diCommand[freeCommandBuf], sizeof(DVDLowDICommand),
                            dst, size, doTransactionCallback, ctx);

    if (result != IPC_RESULT_OK) {
        OSReport(
            "@@@ (DVDLowUnencryptedRead) IOS_IoctlAsync returned error: %d\n",
            result);
        ctx->inUse = FALSE;

        return FALSE;
    }

    return TRUE;
}

BOOL DVDLowStopMotor(BOOL eject, BOOL kill, DVDLowCallback callback) {
    DVDLowContext* ctx;
    IPCResult result;

    requestInProgress = TRUE;
    ctx = newContext(callback, 1);

    nextCommandBuf();
    diCommand[freeCommandBuf].command = DVD_IOCTL_STOP_MOTOR;
    diCommand[freeCommandBuf].arg1 = eject;
    diCommand[freeCommandBuf].arg2 = kill;

    result =
        IOS_IoctlAsync(DiFD, DVD_IOCTL_STOP_MOTOR, &diCommand[freeCommandBuf],
                       sizeof(DVDLowDICommand), &diRegValCache,
                       sizeof(DVDLowDiRegs), doTransactionCallback, ctx);

    if (result != IPC_RESULT_OK) {
        OSReport("@@@ (DVDLowStopMotor) IOS_IoctlAsync returned error: %d\n",
                 result);
        ctx->inUse = FALSE;

        return FALSE;
    }

    return TRUE;
}

// Message of DVDLowWaitForCoverClose (not in this program)
char sWaitForCoverCloseErrorMsg[] =
    "@@@ (DVDLowWaitForCoverClose) IOS_IoctlAsync returned error: %d\n";

BOOL DVDLowInquiry(DVDDriveInfo* out, DVDLowCallback callback) {
    DVDLowContext* ctx;
    IPCResult result;

    requestInProgress = TRUE;
    ctx = newContext(callback, 1);

    nextCommandBuf();
    diCommand[freeCommandBuf].command = DVD_IOCTL_INQUIRY;

    result = IOS_IoctlAsync(DiFD, DVD_IOCTL_INQUIRY, &diCommand[freeCommandBuf],
                            sizeof(DVDLowDICommand), out, sizeof(DVDDriveInfo),
                            doTransactionCallback, ctx);

    if (result != IPC_RESULT_OK) {
        OSReport("@@@ (DVDLowInquiry) IOS_IoctlAsync returned error: %d\n",
                 result);
        ctx->inUse = FALSE;

        return FALSE;
    }

    return TRUE;
}

BOOL DVDLowRequestError(DVDLowCallback callback) {
    DVDLowContext* ctx;
    IPCResult result;

    requestInProgress = TRUE;
    ctx = newContext(callback, 1);

    nextCommandBuf();
    diCommand[freeCommandBuf].command = DVD_IOCTL_REQUEST_ERROR;

    result = IOS_IoctlAsync(DiFD, DVD_IOCTL_REQUEST_ERROR,
                            &diCommand[freeCommandBuf], sizeof(DVDLowDICommand),
                            &diRegValCache, sizeof(DVDLowDiRegs),
                            doTransactionCallback, ctx);

    if (result != IPC_RESULT_OK) {
        OSReport("@@@ (DVDLowRequestError) IOS_IoctlAsync returned error: %d\n",
                 result);
        ctx->inUse = FALSE;

        return FALSE;
    }

    return TRUE;
}

BOOL DVDLowSetSpinupFlag(BOOL enable) {
    spinUpValue = enable;
    return TRUE;
}

// Messages of DVDLowNotifyReset (not in this program)
char sNotifyResetInCallbackMsg[] =
    "(DVDLowSetSpinupFlag): Synch functions can't be called in callbacks\n";
char sNotifyResetErrorMsg[] =
    "@@@ (DVDLowNotifyReset) IOS_IoctlAsync returned error: %d\n";

BOOL DVDLowReset(DVDLowCallback callback) {
    DVDLowContext* ctx;
    IPCResult result;

    requestInProgress = TRUE;
    ctx = newContext(callback, 1);

    nextCommandBuf();
    diCommand[freeCommandBuf].command = DVD_IOCTL_RESET;
    diCommand[freeCommandBuf].arg1 = spinUpValue;

    result = IOS_IoctlAsync(DiFD, DVD_IOCTL_RESET, &diCommand[freeCommandBuf],
                            sizeof(DVDLowDICommand), NULL, 0,
                            doTransactionCallback, ctx);

    if (result != IPC_RESULT_OK) {
        OSReport("@@@ (DVDLowReset) IOS_IoctlAsync returned error: %d\n",
                 result);
        ctx->inUse = FALSE;

        return FALSE;
    }

    return TRUE;
}

BOOL DVDLowAudioBufferConfig(BOOL enable, u32 size, DVDLowCallback callback) {
    DVDLowContext* ctx;
    IPCResult result;

    requestInProgress = TRUE;
    ctx = newContext(callback, 1);

    nextCommandBuf();
    diCommand[freeCommandBuf].command = DVD_IOCTL_AUDIO_BUFFER_CONFIG;
    diCommand[freeCommandBuf].arg1 = enable;
    diCommand[freeCommandBuf].arg2 = size;

    result = IOS_IoctlAsync(DiFD, DVD_IOCTL_AUDIO_BUFFER_CONFIG,
                            &diCommand[freeCommandBuf], sizeof(DVDLowDICommand),
                            &diRegValCache, sizeof(DVDLowDiRegs),
                            doTransactionCallback, ctx);

    if (result != IPC_RESULT_OK) {
        OSReport(
            "@@@ (DVDLowAudioBufferConfig) IOS_IoctlAsync returned error: %d\n",
            result);
        ctx->inUse = FALSE;

        return FALSE;
    }

    return TRUE;
}

// Messages of DVDLowGetCoverStatus through DVDLowRequestRetryNumber
// (not in this program)
char sGetCoverStatusInCallbackMsg[] =
    "(DVDLowGetCoverStatus): Synch functions can't be called in callbacks\n";
char sGetCoverStatusErrorMsg[] =
    "@@@ (DVDLowGetCoverStatus) IOS_Ioctl returned error: %d\n";
char sReadDvdErrorMsg[] =
    "@@@ (DVDLowReadDVD) IOS_IoctlAsync returned error: %d\n";
char sReadDvdConfigErrorMsg[] =
    "@@@ (DVDLowReadDVDConfig) IOS_IoctlAsync returned error: %d\n";
char sReadDvdCopyrightErrorMsg[] =
    "@@@ (DVDLowReadDvdCopyright) IOS_IoctlAsync returned error: %d\n";
char sReadDvdPhysicalErrorMsg[] =
    "@@@ (DVDLowReadDvdPhysical) IOS_IoctlAsync returned error: %d\n";
char sReadDvdDiscKeyErrorMsg[] =
    "@@@ (DVDLowReadDvdDiscKey) IOS_IoctlAsync returned error: %d\n";
char sReportKeyErrorMsg[] =
    "@@@ (DVDLowReportKey) IOS_IoctlAsync returned error: %d\n";
char sOffsetErrorMsg[] =
    "@@@ (DVDLowOffset) IOS_IoctlAsync returned error: %d\n";
char sStopLaserErrorMsg[] =
    "@@@ (DVDLowStopLaser) IOS_IoctlAsync returned error: %d\n";
char sReadDiskBcaErrorMsg[] =
    "@@@ (DVDLowReadDiskBca) IOS_IoctlAsync returned error: %d\n";
char sSerMeasControlErrorMsg[] =
    "@@@ (DVDLowSerMeasControl) IOS_IoctlAsync returned error: %d\n";
char sRequestDiscStatusErrorMsg[] =
    "@@@ (DVDLowRequestDiscStatus) IOS_IoctlAsync returned error: %d\n";
char sRequestRetryNumberErrorMsg[] =
    "@@@ (DVDLowRequestRetryNumber) IOS_IoctlAsync returned error: %d\n";

BOOL DVDLowSetMaximumRotation(u32 speed, DVDLowCallback callback) {
    DVDLowContext* ctx;
    IPCResult result;

    requestInProgress = TRUE;
    ctx = newContext(callback, 1);

    nextCommandBuf();
    diCommand[freeCommandBuf].command = DVD_IOCTL_SET_MAX_ROTATION;
    diCommand[freeCommandBuf].arg1 = speed >> 16 & 3;

    result = IOS_IoctlAsync(DiFD, DVD_IOCTL_SET_MAX_ROTATION,
                            &diCommand[freeCommandBuf], sizeof(DVDLowDICommand),
                            NULL, 0, doTransactionCallback, ctx);

    if (result != IPC_RESULT_OK) {
        OSReport(
            "@@@ (DVDLowSetMaxRotation) IOS_IoctlAsync returned error: %d\n",
            result);
        ctx->inUse = FALSE;

        return FALSE;
    }

    return TRUE;
}

BOOL DVDLowRead(void* dst, u32 size, u32 offset, DVDLowCallback callback) {
    DVDLowContext* ctx;
    IPCResult result;

    if ((u32)dst % 32 != 0) {
        OSReport(
            "(DVDLowRead): ERROR - destAddr buffer is not 32 byte aligned\n");
        return FALSE;
    }

    requestInProgress = TRUE;
    ctx = newContext(callback, 1);

    readLength = size;

    nextCommandBuf();
    diCommand[freeCommandBuf].command = DVD_IOCTL_READ;
    diCommand[freeCommandBuf].arg1 = size;
    diCommand[freeCommandBuf].arg2 = offset;

    result = IOS_IoctlAsync(DiFD, DVD_IOCTL_READ, &diCommand[freeCommandBuf],
                            sizeof(DVDLowDICommand), dst, size,
                            doTransactionCallback, ctx);

    if (result != IPC_RESULT_OK) {
        OSReport("@@@ (DVDLowRead) IOS_IoctlAsync returned error: %d\n",
                 result);
        ctx->inUse = FALSE;

        return FALSE;
    }

    return TRUE;
}

BOOL DVDLowSeek(u32 offset, DVDLowCallback callback) {
    DVDLowContext* ctx;
    IPCResult result;

    requestInProgress = TRUE;
    ctx = newContext(callback, 1);

    nextCommandBuf();
    diCommand[freeCommandBuf].command = DVD_IOCTL_SEEK;
    diCommand[freeCommandBuf].arg1 = offset;

    result = IOS_IoctlAsync(DiFD, DVD_IOCTL_SEEK, &diCommand[freeCommandBuf],
                            sizeof(DVDLowDICommand), NULL, 0,
                            doTransactionCallback, ctx);

    if (result != IPC_RESULT_OK) {
        OSReport("@@@ (DVDLowSeek) IOS_IoctlAsync returned error: %d\n",
                 result);
        ctx->inUse = FALSE;

        return FALSE;
    }

    return TRUE;
}

// Messages of DVDLowGetCoverReg (not in this program)
char sGetCoverRegInCallbackMsg[] =
    "(DVDLowGetCoverReg): Synch functions can't be called in callbacks\n";
char sGetCoverRegErrorMsg[] =
    "@@@ (DVDLowGetCoverReg) IOS_Ioctl returned error: %d\n";

u32 DVDLowGetCoverRegister(void) {
    return diRegValCache.dicvr;
}

u32 DVDLowGetStatusRegister(void) {
    return statusRegister.reg;
}

BOOL DVDLowPrepareCoverRegister(DVDLowCallback callback) {
    DVDLowContext* ctx;
    IPCResult result;

    nextCommandBuf();
    diCommand[freeCommandBuf].command = DVD_IOCTL_PREPARE_COVER_REGISTER;

    requestInProgress = TRUE;
    ctx = newContext(callback, 1);

    result = IOS_IoctlAsync(DiFD, DVD_IOCTL_PREPARE_COVER_REGISTER,
                            &diCommand[freeCommandBuf], sizeof(DVDLowDICommand),
                            &registerBuf, sizeof(DVDLowRegBuffer),
                            doPrepareCoverRegisterCallback, ctx);

    if (result != IPC_RESULT_OK) {
        OSReport("@@@ (DVDLowPrepareCoverRegsiter) IOS_IoctlAsync returned "
                 "error: %d\n",
                 result);
        ctx->inUse = FALSE;

        return FALSE;
    }

    return TRUE;
}

BOOL DVDLowPrepareStatusRegister(DVDLowCallback callback) {
    DVDLowContext* ctx;
    IPCResult result;

    nextCommandBuf();
    diCommand[freeCommandBuf].command = DVD_IOCTL_PREPARE_STATUS_REGISTER;

    requestInProgress = TRUE;
    ctx = newContext(callback, 1);

    result = IOS_IoctlAsync(DiFD, DVD_IOCTL_PREPARE_STATUS_REGISTER,
                            &diCommand[freeCommandBuf], sizeof(DVDLowDICommand),
                            &statusRegister, sizeof(DVDLowRegBuffer),
                            doTransactionCallback, ctx);

    if (result != IPC_RESULT_OK) {
        OSReport("@@@ (DVDLowPrepareStatusRegsiter) IOS_IoctlAsync returned "
                 "error: %d\n",
                 result);
        ctx->inUse = FALSE;

        return FALSE;
    }

    return TRUE;
}

u32 DVDLowGetImmBufferReg(void) {
    return diRegValCache.diimmbuf;
}

BOOL DVDLowUnmaskStatusInterrupts(void) {
    return TRUE;
}

BOOL DVDLowMaskCoverInterrupt(void) {
    return TRUE;
}

BOOL DVDLowClearCoverInterrupt(DVDLowCallback callback) {
    DVDLowContext* ctx;
    IPCResult result;

    nextCommandBuf();
    diCommand[freeCommandBuf].command = DVD_IOCTL_CLEAR_COVER_INTERRUPT;

    requestInProgress = TRUE;
    ctx = newContext(callback, 1);

    result = IOS_IoctlAsync(DiFD, DVD_IOCTL_CLEAR_COVER_INTERRUPT,
                            &diCommand[freeCommandBuf], sizeof(DVDLowDICommand),
                            NULL, 0, doTransactionCallback, ctx);

    if (result != IPC_RESULT_OK) {
        OSReport("@@@ (DVDLowClearCoverInterrupt) IOS_IoctlAsync returned "
                 "error: %d\n",
                 result);
        ctx->inUse = FALSE;

        return FALSE;
    }

    return TRUE;
}

BOOL __DVDLowTestAlarm(const OSAlarm* alarm) {
#pragma unused(alarm)

    return FALSE;
}

// Message of DVDLowEnableDvdVideo (not in this program)
char sEnableDvdVideoErrorMsg[] =
    "@@@ (DVDLowEnableDvdVideo) IOS_IoctlAsync returned error: %d\n";
