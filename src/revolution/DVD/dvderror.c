#include <revolution/DVD.h>
#include <revolution/NAND.h>
#include <revolution/OS.h>

// The error log is a ring of DVD_ERROR_LOG_MAX records behind a header record
#define DVD_ERROR_LOG_MAX 7

static BOOL ExistFlag = FALSE;
static NANDCommandBlock NandCb;
static NANDFileInfo NandInfo;
static DVDErrorCallback Callback;
static u32 NextOffset;

DVDErrorInfo __ErrorInfo ALIGN(32);
DVDErrorInfo __FirstErrorInfo ALIGN(32);

void cbForNandClose(s32 result, NANDCommandBlock* block) {
#pragma unused(block)

    if (Callback != NULL) {
        Callback(result == NAND_RESULT_OK ? 1 : 2, 0);
    }
}

void cbForNandWrite(s32 result, NANDCommandBlock* block) {
#pragma unused(result)
#pragma unused(block)

    if (NANDCloseAsync(&NandInfo, cbForNandClose, &NandCb) != NAND_RESULT_OK) {
        if (Callback != NULL) {
            Callback(2, 0);
        }
    }
}

void cbForNandSeek(s32 result, NANDCommandBlock* block) {
#pragma unused(block)

    if (result == sizeof(DVDErrorInfo) * (NextOffset + 1)) {
        DCFlushRange(&__ErrorInfo, sizeof(DVDErrorInfo));

        if (NANDWriteAsync(&NandInfo, &__ErrorInfo, sizeof(DVDErrorInfo),
                           cbForNandWrite, &NandCb) != NAND_RESULT_OK) {
            // Must call callback function manually
            cbForNandWrite(-1, NULL);
        }
    } else if (Callback != NULL) {
        Callback(2, 0);
    }
}

void cbForNandWrite0(s32 result, NANDCommandBlock* block) {
#pragma unused(block)

    if (result == sizeof(DVDErrorInfo)) {
        if (NANDSeekAsync(&NandInfo, sizeof(DVDErrorInfo) * (NextOffset + 1),
                          NAND_SEEK_BEG, cbForNandSeek,
                          &NandCb) != NAND_RESULT_OK) {
            // Must call callback function manually
            cbForNandSeek(-1, NULL);
        }
    } else if (Callback != NULL) {
        Callback(2, 0);
    }
}

void cbForNandSeek2(s32 result, NANDCommandBlock* block) {
#pragma unused(block)

    if (result == sizeof(DVDErrorInfo)) {
        __FirstErrorInfo.next = (__FirstErrorInfo.next + 1) % DVD_ERROR_LOG_MAX;

        if (NANDWriteAsync(&NandInfo, &__FirstErrorInfo, sizeof(DVDErrorInfo),
                           cbForNandWrite0, &NandCb) != NAND_RESULT_OK) {
            // Must call callback function manually
            cbForNandWrite0(-1, NULL);
        }
    } else if (Callback != NULL) {
        Callback(2, 0);
    }
}

void cbForNandRead(s32 result, NANDCommandBlock* block) {
#pragma unused(block)

    if (result == sizeof(DVDErrorInfo)) {
        NextOffset = __FirstErrorInfo.next;

        if (NANDSeekAsync(&NandInfo, sizeof(DVDErrorInfo), NAND_SEEK_BEG,
                          cbForNandSeek2, &NandCb) != NAND_RESULT_OK) {
            // Must call callback function manually
            cbForNandSeek2(-1, NULL);
        }
    } else {
        __ErrorInfo.next = 1;

        if (NANDWriteAsync(&NandInfo, &__ErrorInfo, sizeof(DVDErrorInfo),
                           cbForNandWrite, &NandCb) != NAND_RESULT_OK) {
            // Must call callback function manually
            cbForNandWrite(-1, NULL);
        }
    }
}

void cbForNandSeek0(s32 result, NANDCommandBlock* block) {
#pragma unused(block)

    if (result == 0) {
        NextOffset = 0;
        __ErrorInfo.next = 1;

        if (NANDWriteAsync(&NandInfo, &__FirstErrorInfo, sizeof(DVDErrorInfo),
                           cbForNandWrite0, &NandCb) != NAND_RESULT_OK) {
            // Must call callback function manually
            cbForNandWrite0(-1, NULL);
        }
    } else if (Callback != NULL) {
        Callback(2, 0);
    }
}

void cbForNandSeek1(s32 result, NANDCommandBlock* block) {
#pragma unused(block)

    if (result == sizeof(DVDErrorInfo)) {
        if (NANDReadAsync(&NandInfo, &__FirstErrorInfo, sizeof(DVDErrorInfo),
                          cbForNandRead, &NandCb) != NAND_RESULT_OK) {
            // Must call callback function manually
            cbForNandRead(-1, NULL);
        }
    } else {
        if (NANDSeekAsync(&NandInfo, 0, NAND_SEEK_BEG, cbForNandSeek0,
                          &NandCb) != NAND_RESULT_OK) {
            // Must call callback function manually
            cbForNandSeek0(-1, NULL);
        }
    }
}

void cbForNandOpen(s32 result, NANDCommandBlock* block) {
#pragma unused(block)

    if (result == NAND_RESULT_OK) {
        if (ExistFlag) {
            if (NANDSeekAsync(&NandInfo, sizeof(DVDErrorInfo), NAND_SEEK_BEG,
                              cbForNandSeek1, &NandCb) != NAND_RESULT_OK) {
                // Must call callback function manually
                cbForNandSeek1(-1, NULL);
            }
        } else {
            NextOffset = 0;
            __ErrorInfo.next = 1;

            if (NANDWriteAsync(&NandInfo, &__FirstErrorInfo,
                               sizeof(DVDErrorInfo), cbForNandWrite0,
                               &NandCb) != NAND_RESULT_OK) {
                // Must call callback function manually
                cbForNandWrite0(-1, NULL);
            }
        }
    } else if (Callback != NULL) {
        Callback(2, 0);
    }
}

void cbForNandCreate(s32 result, NANDCommandBlock* block) {
#pragma unused(block)

    if (result == NAND_RESULT_OK || result == NAND_RESULT_EXISTS) {
        if (result == NAND_RESULT_EXISTS) {
            ExistFlag = TRUE;
        }

        if (NANDPrivateOpenAsync("/shared2/test2/dvderror.dat", &NandInfo,
                                 NAND_ACCESS_RW, cbForNandOpen,
                                 &NandCb) != NAND_RESULT_OK) {
            if (Callback != NULL) {
                Callback(2, 0);
            }
        }
    } else if (Callback != NULL) {
        Callback(2, 0);
    }
}

void cbForNandCreateDir(s32 result, NANDCommandBlock* block) {
#pragma unused(block)

    if (result == NAND_RESULT_OK || result == NAND_RESULT_EXISTS) {
        if (NANDPrivateCreateAsync("/shared2/test2/dvderror.dat",
                                   NAND_PERM_RWALL, 0, cbForNandCreate,
                                   &NandCb) != NAND_RESULT_OK) {
            if (Callback != NULL) {
                Callback(2, 0);
            }
        }
    } else if (Callback != NULL) {
        Callback(2, 0);
    }
}

void cbForPrepareStatusRegister(u32 intType) {
    if (intType == DVD_INTTYPE_TC) {
        __ErrorInfo.disr = DVDLowGetStatusRegister();
    } else {
        __ErrorInfo.disr = 0xFFFFFFFF;
    }

    if (NANDPrivateCreateDirAsync("/shared2/test2", NAND_PERM_RWALL, 0,
                                  cbForNandCreateDir,
                                  &NandCb) != NAND_RESULT_OK) {
        if (Callback != NULL) {
            Callback(2, 0);
        }
    }
}

void __DVDStoreErrorCode(u32 error, DVDErrorCallback callback) {
    __ErrorInfo.error = error;
    __ErrorInfo.sec = OS_TICKS_TO_SEC(OSGetTime());
    Callback = callback;

    DVDLowPrepareStatusRegister(cbForPrepareStatusRegister);
}
