#include <revolution/DVD.h>
#include <revolution/ESP.h>
#include <revolution/IPC.h>
#include <revolution/NAND.h>
#include <revolution/OS.h>
#include <revolution/SC.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <wchar.h>

#define MENU_TITLE_ID 0x0000000100000002ULL

typedef void (*ApploaderInitFunc)(void (*report)(const char*, ...));
typedef BOOL (*ApploaderMainFunc)(void** dst, s32* size, s32* offset);
typedef void* (*ApploaderCloseFunc)(void);
typedef void (*ApploaderEntryFunc)(ApploaderInitFunc* init,
                                   ApploaderMainFunc* main,
                                   ApploaderCloseFunc* close);

typedef struct ApploaderHeader {
    char date[16];            // at 0x0
    ApploaderEntryFunc entry; // at 0x10
    u32 size;                 // at 0x14
    u32 rebootSize;           // at 0x18
    u32 reserved;             // at 0x1C
} ApploaderHeader;

typedef struct PartTableInfo {
    volatile u32 numGamePartitions;    // at 0x0
    u32 gamePartitionOffset;  // at 0x4
    volatile u32 numOtherPartitions;   // at 0x8
    u32 otherPartitionOffset; // at 0xC
} PartTableInfo;

typedef struct PartInfo {
    u32 offset; // at 0x0
    u32 type;   // at 0x4
} PartInfo;

extern void IPCCltReInit(void);
extern BOOL DVDLowOpenPartitionWithTmdAndTicketView(
    u32 offset, const ESTicketView* ticketView, u32 tmdSize,
    const ESTitleMeta* tmd, u32 certsSize, const void* certs,
    DVDLowCallback callback);
extern BOOL DVDLowFinalize(void);
extern BOOL DVDSetAutoInvalidation(BOOL autoInval);
extern void __DVDResetWithNoSpinup(void);
extern BOOL OSPlayTimeIsLimited(void);
extern s32 __OSGetPlayTime(ESTicketView* ticketView, u32* type,
                           u32* remainingTime);
extern BOOL __OSWriteExpiredFlag(void);
extern BOOL __OSCreateNandbootInfo(void);
extern BOOL __OSWriteNandbootInfo(OSNandbootInfo* info);
extern void __OSHotResetForError(void);

// OS_DOL_EXEC_PARAMS (0x800030F0) as a linker-defined symbol. The final store in
// RunApploader goes through it: the compiler does not share the 0x80000000 base
// of the stores just before it, which a constant address would.
DECL_SECTION(".data") extern OSExecParams* __OSDolExecParams;

static DVDDiskID DiskID;

static volatile BOOL Prepared;
BOOL __OSInReboot;
static volatile u32 intTypeTmp;
// Zero-initialized so it is laid out ahead of the function-local statics.
u32 __OSLaunchPartitionType = 0;

static int PackArgs(void* addr, s32 argc, char* argv[]) {
    int argCount;
    u8* bootInfo2;
    u8* argPtr;

    bootInfo2 = addr;
    memset(bootInfo2, 0, 0x2000);

    if (argc == 0) {
        *(u32*)&bootInfo2[8] = 0;
    } else {
        argCount = argc;
        argPtr = bootInfo2 + 0x2000;

        while (--argc >= 0) {
            argPtr -= (strlen(argv[argc]) + 1);
            strcpy((char*)argPtr, argv[argc]);

            argv[argc] = (char*)(argPtr - bootInfo2);
        }

        argPtr = bootInfo2 + ((argPtr - bootInfo2) & ~3);
        argPtr -= (argCount + 1) * sizeof(argv);

        {
            u32 i;
            char** argList = (char**)argPtr;
            for (i = 0; i < argCount + 1; i++) {
                argList[i] = argv[i];
            }
        }

        argPtr -= sizeof(argCount);
        *(u32*)argPtr = argCount;
        *(u32*)&bootInfo2[8] = argPtr - bootInfo2;
    }

    return 1;
}

static BOOL Utf16ToArg(char* dst, const char* src) {
    char* p;
    int i;
    u8 shift;
    u8 mask;
    int val;

    if (src != NULL) {
        p = dst;
        while (*src != 0 || *(src + 1) != 0) {
            for (i = 0; i < 4; i++) {
                mask = (i & 1) ? 0x0F : 0xF0;
                shift = (i & 1) ? 0 : 4;
                val = (*src & mask) >> shift;
                if (0 <= val && val < 10) {
                    *p = (char)(val + '0');
                } else if (10 <= val && val < 16) {
                    *p = (char)(val + 'a' - 10);
                } else {
                    return FALSE;
                }
                p++;
                if ((i & 1) != 0) {
                    src++;
                }
            }
        }

        *p = 0;
        return TRUE;
    }

    return FALSE;
}

static int PackInstallerArgs(void* addr, s32 argc, char* argv[]) {
    s32 numArgs;
    char* bootInfo2;
    char* ptr;
    char** list;
    u32 i;
    bootInfo2 = (char*)addr;
    memset(bootInfo2, 0, 0x2000);

    if (argc == 0) {
        *(u32*)&bootInfo2[8] = 0;
    } else {
        numArgs = argc;
        ptr = bootInfo2 + 0x2000;
        while (--argc >= 0) {
            if (argc < 2 || (argc % 2) != 0) {
                ptr -= (strlen(argv[argc]) + 1);
                strcpy(ptr, argv[argc]);
                argv[argc] = (char*)(ptr - bootInfo2);
            } else {
                char* src = argv[argc];
                ptr -= (wcslen((wchar_t*)src) * 4 + 1);
                Utf16ToArg(ptr, src);
                argv[argc] = (char*)(ptr - bootInfo2);
            }
        }

        ptr = bootInfo2 + ((ptr - bootInfo2) & ~0x3);
        ptr -= ((numArgs + 1) * 4);
        list = (char**)ptr;
        for (i = 0; i < numArgs + 1; i++) {
            list[i] = argv[i];
        }
        ptr -= 4;
        *(s32*)ptr = numArgs;
        *(u32*)&bootInfo2[8] = (u32)(ptr - bootInfo2);
    }

    return 1;
}

// Flushes the instruction cache and jumps to the loaded program's entry point.
//
// Kept as asm on purpose: no C source compiles to this. Everything up to the
// isync is plain C (ICFlashInvalidate(); __sync(); __isync();), but the end is
// not:
// - `mtctr r31; bctr` is a jump that never returns. A C call through a
//   function pointer always becomes `mr r12, r31; mtctr r12; bctrl`, a call
//   that saves a return address, and MWCC does not turn it into a tail jump.
// - After the bctr comes an unreachable epilogue (the frfree + blr below) that
//   restores r31 before r0. Compiled C never emits code after a jump, and its
//   epilogue restores r0 first.
// Checked with GC 3.0a3-3.0a5.2 and every Wii compiler, using several C forms
// (plain and typed function pointers, register, dont_inline, a noreturn call
// or endless loop after the jump). The split is also correct: both callers
// branch to the start, and nothing branches into the epilogue.
static asm void Run(void* entryPoint) {
    // clang-format off
    fralloc
    mr r31, r3
    bl ICFlashInvalidate
    sync
    isync
    mtctr r31
    bctr
    frfree
    blr
    // clang-format on
}

static void ReadDisc(void* addr, s32 length, s32 offset) {
    DVDCommandBlock block;
    u8 unused[4] = {0xFF, 0xFF, 0xFF, 0};

    DVDReadAbsAsyncPrio(&block, addr, length, offset, NULL, 0);
    while (DVDGetCommandBlockStatus(&block)) {
        if (DVDGetCommandBlockStatus(&block) > 2 ||
            DVDGetCommandBlockStatus(&block) < 0) {
            __OSReturnToMenuForError();
        }
    }
}

static void Callback(s32 result, DVDCommandBlock* block) {
#pragma unused(result)
#pragma unused(block)
    Prepared = TRUE;
}

static s32 GetApploaderPosition(void) {
    // Function-local: pointer stores elsewhere (e.g. the save-region writes in
    // __OSBootDolSimple) can't alias it, so its load may be scheduled first.
    static s32 apploaderPosition;
    s32 off;

    if (apploaderPosition != 0) {
        return apploaderPosition;
    }

    if (*(s32*)0x800030F4 != 0) {
        s32* header = (s32*)OSAllocFromMEM1ArenaLo(0x40, 0x20);
        ReadDisc(header, 0x40, *(s32*)0x800030F4 >> 2);
        off = header[14];
        apploaderPosition = (*(s32*)0x800030F4 + off) >> 2;
    } else {
        apploaderPosition = 0x910;
    }

    return apploaderPosition;
}

void __OSGetExecParams(OSExecParams* out) {
    if ((void*)OS_DOL_EXEC_PARAMS >= (void*)0x80000000) {
        memcpy(out, OS_DOL_EXEC_PARAMS, sizeof(OSExecParams));
    } else {
        out->WORD_0x0 = 0;
    }
}

static void __OSDVDLowCallback(u32 intType) {
    intTypeTmp = intType;
}

// The original keeps `rv` in one register across every ESP error check, which
// MWCC only does when lifetime splitting and CSE are off for the function.
#pragma push
#pragma opt_lifetimes off
#pragma opt_common_subs off
void __OSLaunchNextFirmware(void) {
    ESTicketView* views;
    s32 rv;
    u32 numViews = 1;
    ESTitleId titleId;
    u32 tmdSize = 0;
    ESTicketView* ticketView;
    u8 i;
    u32 mem2Size;
    u32 mem2End;
    PartInfo* targetPart;
    PartTableInfo* tableInfo;
    PartInfo* p;
    PartInfo* partInfo;
    ESTitleMeta* tmd;

    rv = -1;

    tableInfo = (PartTableInfo*)OSAllocFromMEM1ArenaLo(0x20, 0x20);
    partInfo = (PartInfo*)OSAllocFromMEM1ArenaLo(0x800, 0x20);
    tmd = (ESTitleMeta*)OSAllocFromMEM1ArenaLo(0x4A00, 0x40);
    ticketView = (ESTicketView*)OSAllocFromMEM1ArenaLo(0xE0, 0x20);

    if (__OSLaunchPartitionType == *(u32*)0x80003194 &&
        *(u32*)0x80003198 != 0) {
        rv = ESP_InitLib();
        if (rv == 0) {
            rv = ESP_DiGetTicketView(NULL, ticketView);
        }
        if (rv == 0) {
            rv = ESP_DiGetTmd(NULL, &tmdSize);
        }
        if (rv == 0) {
            rv = ESP_DiGetTmd(tmd, &tmdSize);
        }
        ESP_CloseLib();

        if (OSPlayTimeIsLimited()) {
            u32 type = 0;
            u32 remaining = (u32)-1;
            __OSGetPlayTime(ticketView, &type, &remaining);
            if (remaining == 0) {
                __OSWriteExpiredFlag();
                __OSReturnToMenuForError();
            }
        }
    }

    if (rv == 0) {
        targetPart = partInfo;
        targetPart->type = *(u32*)0x80003194;
        targetPart->offset = *(u32*)0x80003198;
    } else {
        intTypeTmp = 0;
        DVDLowClosePartition(__OSDVDLowCallback);
        while (intTypeTmp == 0) {
            ;
        }
        if (intTypeTmp != 1) {
            OSReport("\nOSExec(): Failed to exec %d in %d\n", intTypeTmp, 888);
            __OSReturnToMenuForError();
        }

        intTypeTmp = 0;
        DVDLowUnencryptedRead(tableInfo, 0x20, 0x10000, __OSDVDLowCallback);
        while (intTypeTmp == 0) {
            ;
        }
        if (intTypeTmp != 1) {
            OSReport("\nOSExec(): Failed to exec %d in %d\n", intTypeTmp, 898);
            __OSReturnToMenuForError();
        }

        intTypeTmp = 0;
        DVDLowUnencryptedRead(partInfo, 0x800, tableInfo->gamePartitionOffset,
                              __OSDVDLowCallback);
        while (intTypeTmp == 0) {
            ;
        }
        if (intTypeTmp != 1) {
            OSReport("\nOSExec(): Failed to exec %d in %d\n", intTypeTmp, 910);
            __OSReturnToMenuForError();
        }

        targetPart = NULL;
        for (p = partInfo, i = 0; i < tableInfo->numGamePartitions; p++, i++) {
            if (p->type == __OSLaunchPartitionType) {
                targetPart = p;
            }
        }

        if (targetPart == NULL) {
            for (p = partInfo + 4, i = 0; i < tableInfo->numOtherPartitions;
                 p++, i++) {
                if (p->type == __OSLaunchPartitionType) {
                    targetPart = p;
                }
            }
        }

        if (targetPart == NULL) {
            OSReport(
                "\nOSExec(): The specified game doesn't exist in the disc\n");
            __OSReturnToMenuForError();
        }

        *(u32*)0x80003194 = targetPart->type;
        *(u32*)0x80003198 = targetPart->offset;

        intTypeTmp = 0;
        if (*(u8*)0x80003187 == 0x80) {
            DVDLowOpenPartitionWithTmdAndTicketView(
                targetPart->offset, ticketView, tmdSize, tmd, 0, NULL,
                __OSDVDLowCallback);
        } else {
            DVDLowOpenPartition(targetPart->offset, NULL, 0, NULL,
                                (ESPTmd*)tmd, __OSDVDLowCallback);
        }
        while (intTypeTmp == 0) {
            ;
        }
        if (intTypeTmp != 1) {
            OSReport("\nOSExec(): Failed to exec %d in %d\n", intTypeTmp, 971);
            __OSReturnToMenuForError();
        }

        intTypeTmp = 0;
        DVDLowClosePartition(__OSDVDLowCallback);
        while (intTypeTmp == 0) {
            ;
        }
        if (intTypeTmp != 1) {
            OSReport("\nOSExec(): Failed to exec %d in %d\n", intTypeTmp, 981);
            __OSReturnToMenuForError();
        }
    }

    titleId = tmd->head.sysVersion;
    rv = ESP_InitLib();
    if (rv != 0) {
        OSReport("\nOSExec(): Failed to exec %d in %d\n", rv, 992);
        __OSHotResetForError();
    }

    rv = ESP_GetTicketViews(titleId, NULL, &numViews);
    if (numViews != 1 || rv != 0) {
        OSReport("\nOSExec(): Failed to exec %d in %d\n", rv, 1001);
        __OSHotResetForError();
    }

    views = (ESTicketView*)OSAllocFromMEM1ArenaLo(
        OSRoundUp32B(numViews * sizeof(ESTicketView)), 0x20);
    rv = ESP_GetTicketViews(titleId, views, &numViews);
    if (rv != 0) {
        OSReport("\nOSExec(): Failed to exec %d in %d\n", rv, 1010);
        __OSHotResetForError();
    }

    DVDLowFinalize();
    mem2Size = *(u32*)0x8000311C;
    mem2End = *(u32*)0x80003120;
    DCStoreRange((void*)0x80003100, 0x100);

    rv = ESP_LaunchTitle(titleId, views);
    if (rv != 0) {
        OSReport("\nOSExec(): Failed to exec %d in %d\n", rv, 1026);
        __OSHotResetForError();
    }

    ESP_CloseLib();
    DCInvalidateRange((void*)0x80003100, 0x100);

    if (mem2Size < *(u32*)0x8000311C) {
        *(u32*)0x80003120 =
            mem2Size - (*(u32*)0x8000311C - *(u32*)0x80003120);
        *(u32*)0x80003128 =
            mem2Size - (*(u32*)0x8000311C - *(u32*)0x80003128);
        *(u32*)0x80003130 =
            mem2Size - (*(u32*)0x8000311C - *(u32*)0x80003130);
        *(u32*)0x80003134 =
            mem2Size - (*(u32*)0x8000311C - *(u32*)0x80003134);
        *(u32*)0x8000311C = mem2Size;
    }

    if (mem2End < *(u32*)0x80003120) {
        __OSInitMemoryProtection();
    }

    __OSInitIPCBuffer();
    IPCReInit();
    IPCCltReInit();
    DVDLowInit();

    intTypeTmp = 0;
    DVDLowReadDiskID(&DiskID, __OSDVDLowCallback);
    while (intTypeTmp == 0) {
        ;
    }
    if (intTypeTmp != 1) {
        OSReport("\nOSExec(): Failed to exec %d in %d\n", intTypeTmp, 1070);
        __OSReturnToMenuForError();
    }

    intTypeTmp = 0;
    if (*(u8*)0x80003187 == 0x80) {
        DVDLowOpenPartitionWithTmdAndTicketView(
            targetPart->offset, ticketView, tmdSize, tmd, 0, NULL, __OSDVDLowCallback);
    } else {
        DVDLowOpenPartition(targetPart->offset, NULL, 0, NULL, (ESPTmd*)tmd,
                            __OSDVDLowCallback);
    }
    while (intTypeTmp == 0) {
        ;
    }
    if (intTypeTmp != 1) {
        OSReport("\nOSExec(): Failed to exec %d in %d\n", intTypeTmp, 1093);
        __OSReturnToMenuForError();
    }
}
#pragma pop

void __OSLaunchMenu(void) {
    s32 rv;
    ESTicketView* views;
    u32 numViews = 1;

    OSSetArenaLo((void*)0x81280000);
    OSSetArenaHi((void*)0x812F0000);

    if (ESP_InitLib() != 0) {
        return;
    }

    rv = ESP_GetTicketViews(MENU_TITLE_ID, NULL, &numViews);
    if (numViews != 1 || rv != 0) {
        return;
    }

    views = (ESTicketView*)OSAllocFromMEM1ArenaLo(
        OSRoundUp32B(numViews * sizeof(ESTicketView)), 0x20);
    if (ESP_GetTicketViews(MENU_TITLE_ID, views, &numViews) != 0) {
        return;
    }

    if (ESP_LaunchTitle(MENU_TITLE_ID, views) != 0) {
        return;
    }

    while (TRUE) {
        ;
    }
}

BOOL __OSCheckCompanyCode(ESTitleId titleId, BOOL isDisc) {
    NANDStatus status;
    char homeDir[68];
    char dataDir[68];
    u16 curGroupId;
    u16 dstGroupId;

    if (NANDGetHomeDir(homeDir) != NAND_RESULT_OK) {
        return FALSE;
    }

    if (NANDGetStatus(homeDir, &status) != NAND_RESULT_OK) {
        return FALSE;
    }

    curGroupId = status.groupId;

    if (curGroupId == 2 && OSGetAppType() == OS_APP_TYPE_DVD &&
        (OSGetConsoleType() & OS_CONSOLE_MASK) == OS_CONSOLE_MASK_EMU) {
        curGroupId = *(u16*)DVDGetCurrentDiskID()->company;
    }

    if (isDisc) {
        dstGroupId = *(u16*)DVDGetCurrentDiskID()->company;
    } else {
        sprintf(dataDir, "/title/%08x/%08x/data",
                (u32)((titleId >> 32) & 0xFFFFFFFFULL),
                (u32)(titleId & 0xFFFFFFFFULL));
        if (NANDGetStatus(dataDir, &status) != NAND_RESULT_OK) {
            return FALSE;
        }
        dstGroupId = status.groupId;
    }

    return curGroupId == dstGroupId;
}

s32 __OSGetValidTicketIndex(ESTicketView* ticketViewList, u32 numTicketViews) {
    u32 i;
    s32 index = 0;
    BOOL foundUnexpired = FALSE;
    u32 maxAccessBits = 0;
    u32 maxRemaining = 0;
    int j;

    if (ticketViewList == NULL) {
        OSReport("NULL pointer detected: line %d in %s\n", 1339, "OSExec.c");
        return -1017;
    }

    for (i = 0; i < numTicketViews; i++) {
        u32 remaining;
        u32 limitType;
        s32 rv = __OSGetPlayTime(&ticketViewList[i], &limitType, &remaining);
        if (rv > 0) {
            continue;
        }
        if (rv == 0) {
            goto valid;
        }
        return rv;

valid:
        switch (limitType) {
            case 0: {
                u32 bits = 0;
                u16 mask;
                if (!foundUnexpired) {
                    index = i;
                    foundUnexpired = TRUE;
                }
                mask = (ticketViewList[i].sysAccessMask[1] << 8) |
                       ticketViewList[i].sysAccessMask[0];
                for (j = 0; j < 16; j++) {
                    if (mask & (1 << j)) {
                        bits++;
                    }
                }
                if (bits > maxAccessBits) {
                    index = i;
                    maxAccessBits = bits;
                }
                break;
            }
            case 1:
                if (!foundUnexpired && remaining > maxRemaining) {
                    index = i;
                    maxRemaining = remaining;
                }
                break;
            case 9:
                break;
            default:
                break;
            }
    }

    if (!foundUnexpired && maxRemaining == 0) {
        return -1;
    }

    return index;
}

void __OSRelaunchTitle(void) {
    ESTitleId titleId ALIGN(32);
    ESTicketView* ticketView;
    ESTicketView* views;
    u32 numViews = 1;
    s32 rv;

    OSSetArenaLo((void*)0x81280000);
    OSSetArenaHi((void*)0x812F0000);

    if (ESP_InitLib() != 0) {
        __OSReturnToMenuForError();
    }

    if (ESP_GetTitleId(&titleId) != 0) {
        __OSReturnToMenuForError();
    }

    ticketView = (ESTicketView*)OSAllocFromMEM1ArenaLo(0xE0, 0x20);
    if (ticketView == (void*)NULL) {
        __OSReturnToMenuForError();
    }

    memset(ticketView, 0, 0xE0);

    rv = ESP_DiGetTicketView(NULL, ticketView);
    if (rv == -1017) {
        if (ESP_GetTicketViews(titleId, NULL, &numViews) != 0) {
            __OSReturnToMenuForError();
        }
        views = (ESTicketView*)OSAllocFromMEM1ArenaLo(
            OSRoundUp32B(numViews * sizeof(ESTicketView)), 0x20);
        if (views == (void*)NULL) {
            __OSReturnToMenuForError();
        }
        if (ESP_GetTicketViews(titleId, views, &numViews) != 0) {
            __OSReturnToMenuForError();
        }
        memcpy(ticketView, views, sizeof(ESTicketView));
    } else if (rv != 0) {
        __OSReturnToMenuForError();
    } else {
        if (OSPlayTimeIsLimited()) {
            u32 type = 0;
            u32 remaining = (u32)-1;
            __OSGetPlayTime(ticketView, &type, &remaining);
            if (remaining == 0) {
                __OSWriteExpiredFlag();
                __OSReturnToMenuForError();
            }
        }
    }

    if (ESP_LaunchTitle(titleId, ticketView) != 0) {
        __OSReturnToMenuForError();
    }

    while (TRUE) {
        ;
    }
}

void __OSLaunchTitle(ESTitleId titleId) {
    ESTicketView* ticketView;
    ESTicketView* views;
    s32 index;
    u32 numViews = 1;

    if (ESP_InitLib() != 0) {
        return;
    }

    ticketView = (ESTicketView*)OSAllocFromMEM1ArenaLo(0xE0, 0x20);
    if (ticketView == NULL) {
        return;
    }

    memset(ticketView, 0, 0xE0);

    if (ESP_GetTicketViews(titleId, NULL, &numViews) != 0) {
        return;
    }

    views = (ESTicketView*)OSAllocFromMEM1ArenaLo(
        OSRoundUp32B(numViews * sizeof(ESTicketView)), 0x20);
    if (views == NULL) {
        return;
    }

    if (ESP_GetTicketViews(titleId, views, &numViews) != 0) {
        return;
    }

    index = __OSGetValidTicketIndex(views, numViews);
    if (index < 0) {
        return;
    }

    memcpy(ticketView, &views[index], sizeof(ESTicketView));

    if (ESP_LaunchTitle(titleId, ticketView) != 0) {
        return;
    }

    while (TRUE) {
        ;
    }
}

static inline BOOL CheckFirmware(ESTmdView* tmdView) {
    u32 numViews = 0;
    u32 numContents = 0;
    ESTitleId sysVersion = tmdView->head.sysVersion;

    if (ESP_GetTicketViews(sysVersion, NULL, &numViews) != 0 || numViews != 1) {
        return FALSE;
    }
    if (ESP_ListTitleContentsOnCard(sysVersion, NULL, &numContents) != 0 ||
        numContents == 0) {
        return FALSE;
    }
    return TRUE;
}

static inline BOOL CheckRegion(ESTmdView* tmdView) {
    u32 region = tmdView->head.customData.region;
    if (region == 3) {
        return TRUE;
    }
    if (region == SCGetProductGameRegion()) {
        return TRUE;
    }
    return FALSE;
}

static inline BOOL IsDriveSpinEnabled(ESTmdView* tmdView) {
    return (tmdView->head.customData.driveSpin & 1) ? TRUE : FALSE;
}

void LaunchCommon(ESTitleId titleId, u32 launchCode, char* const argv[],
                 BOOL check) {
    u32 tmdSize;
    ESTmdView* tmdView;
    ESTitleId launcher ALIGN(32);
    OSStateFlags stateFlags;
    char titleIdStr[20];

    tmdSize = (u32)(tmdView = NULL);

    if (ESP_InitLib() != 0) {
        __OSReturnToMenuForError();
    }

    if (ESP_GetTmdView(titleId, NULL, &tmdSize) != 0) {
        __OSReturnToMenuForError();
    }

    tmdView = (ESTmdView*)OSAllocFromMEM1ArenaLo(OSRoundUp32B(tmdSize), 0x40);
    if (tmdView == (void*)NULL) {
        __OSReturnToMenuForError();
    }

    if (ESP_GetTmdView(titleId, tmdView, &tmdSize) != 0) {
        __OSReturnToMenuForError();
    }

    if (!CheckFirmware(tmdView)) {
        OSReport("OSLaunchTitle(): Firmware is not installed\n");
        __OSReturnToMenuForError();
    }

    if (check) {
        if (!__OSCheckCompanyCode(titleId, FALSE)) {
            OSReport("OSLaunchTitle(): Company code is not correct\n");
            __OSReturnToMenuForError();
        }

        if (!CheckRegion(tmdView)) {
            OSReport("OSLaunchTitle(): Country code is not correct\n");
            __OSReturnToMenuForError();
        }
    }

    if (!IsDriveSpinEnabled(tmdView)) {
        if (__DVDGetCoverStatus() == DVD_COVER_CLOSED) {
            __DVDResetWithNoSpinup();
        }
    }

    if (argv != NULL || launchCode != 0) {
        OSNandbootInfo* info;
        void* argBuf;
        int argc;
        char** newArgv;
        int i;

        argBuf = OSAllocFromMEM1ArenaLo(0x2000, 0x40);
        info = (OSNandbootInfo*)((u8*)argBuf + 0xFE0);

        sprintf(titleIdStr, "%016llx", titleId);
        argc = 0;
        if (argv != NULL) {
            while (argv[argc] != NULL) {
                argc++;
            }
        }
        newArgv =
            (char**)OSAllocFromMEM1ArenaLo((++argc + 1) * sizeof(char*), 1);
        newArgv[0] = titleIdStr;
        for (i = 1; i < argc; i++) {
            newArgv[i] = argv[i - 1];
        }
        PackArgs(argBuf, argc, newArgv);

        if (__OSInIPL) {
            launcher = MENU_TITLE_ID;
        } else {
            s32 err = ESP_InitLib();
            if (err != 0) {
                OSReport("\nOSExec(): Failed to exec %d in %d\n", err, 1712);
                __OSHotResetForError();
            }
            err = ESP_GetTitleId(&launcher);
            if (err != 0) {
                OSReport("\nOSExec(): Failed to exec %d in %d\n", err, 1720);
                __OSHotResetForError();
            }
        }

        info->launcher = launcher;
        info->argsoff = *(u32*)((u8*)argBuf + 8);
        info->apptype = OSGetAppType();
        if (titleId == MENU_TITLE_ID) {
            info->titletype = 4;
        } else {
            info->titletype = 2;
        }
        info->launchcode = launchCode;

        __OSCreateNandbootInfo();
        __OSWriteNandbootInfo(info);

        __OSReadStateFlags(&stateFlags);
        stateFlags.BYTE_0x5 = 3;
        if (titleId == MENU_TITLE_ID) {
            stateFlags.discState = __OSGetDiscState(stateFlags.discState);
            stateFlags.BYTE_0x7 = 2;
        }
        __OSWriteStateFlags(&stateFlags);
    }

    OSDisableScheduler();
    __OSShutdownDevices(OS_SD_EVENT_LAUNCH_APP);
    OSEnableScheduler();
    __OSLaunchTitle(titleId);
    __OSLaunchMenu();
}

void __OSReturnToMenul(u32 launchCode, const char* arg0, ...) {
    va_list args;
    char** argv;
    u32 i;

    __OSUnRegisterStateEvent();
    OSSetArenaLo((void*)0x81280000);
    OSSetArenaHi((void*)0x812F0000);

    argv = (char**)OSAllocFromMEM1ArenaLo(4, 0x1000);
    i = 0;

    va_start(args, arg0);
    while ((argv[i++] = (char*)arg0) != NULL) {
        arg0 = va_arg(args, const char*);
    }

    LaunchCommon(MENU_TITLE_ID, launchCode, argv, FALSE);
    __OSReturnToMenuForError();
}

// __OSLaunchTitlevForSystem, __OSLaunchTitlelForSystem and OSLaunchDisk only
// print these messages. The game never calls them, so the linker dead-stripped
// their code, but their pooled strings stayed in .data. Keep only the strings.
char __OSLaunchTitlevForSystemMsg[] =
    "__OSLaunchTitlevForSystem(): only system can call this function\n";
char __OSLaunchTitlelForSystemMsg[] =
    "__OSLaunchTitlelForSystem(): only system can call this function\n";
char OSLaunchDiskMsgDVDApp[] =
    "OSLaunchDisk(): You can't call this API from DVD application.  \n";
char OSLaunchDiskMsgPrepare[] =
    "OSLaunchDisk(): You must call DVDPrepareDisk before launching disc.\n";
char OSLaunchDiskMsgDiffId[] =
    "OSLaunchDisk(): Specified id is different from one specified to "
    "DVDPrepareDisk.\n";
char OSLaunchDiskZeroId[] = "0000000000000000";

static inline void ReadApploader(ApploaderHeader* header) {
    ReadDisc(header, 0x20, GetApploaderPosition());
    ReadDisc((void*)0x81200000, OSRoundUp32B(header->size),
             GetApploaderPosition() + 8);
    ICInvalidateRange((void*)0x81200000, OSRoundUp32B(header->size));
}

static inline BOOL CheckApploaderDate(ApploaderHeader* header) {
    return (strncmp(header->date, "2004/02/01", 10) > 0) ? TRUE : FALSE;
}

static inline void RunApploader(ApploaderHeader* header, OSExecParams* params) {
    void* entryPoint;
    OSExecParams* paramsCopy;
    ApploaderInitFunc appInit;
    ApploaderMainFunc appMain;
    ApploaderCloseFunc appClose;
    void* dst;
    s32 size;
    s32 offset;

    header->entry(&appInit, &appMain, &appClose);

    paramsCopy =
        (OSExecParams*)OSAllocFromMEM1ArenaLo(sizeof(OSExecParams), 1);
    memcpy(paramsCopy, params, sizeof(OSExecParams));
    OS_DOL_EXEC_PARAMS = paramsCopy;

    appInit((void (*)(const char*, ...))OSReport);
    OSSetArenaLo(paramsCopy);

    while (appMain(&dst, &size, &offset)) {
        ReadDisc(dst, size, (u32)offset >> __DVDLayoutFormat);
    }

    entryPoint = appClose();

    OS_CURRENT_APP_NAME = *(u32*)0x80000000;
    OS_CURRENT_APP_TYPE = OS_APP_TYPE_DVD;

    paramsCopy =
        (OSExecParams*)OSAllocFromMEM1ArenaLo(sizeof(OSExecParams), 1);
    memcpy(paramsCopy, params, sizeof(OSExecParams));
    __OSDolExecParams = paramsCopy;

    PI_HW_REGS[PI_RESET] = 7;
    OSDisableInterrupts();
    Run(entryPoint);
}

#pragma push
#pragma optimization_level 3
void __OSBootDolSimple(u32 dolOffset, u32 restartCode, void* regionStart,
                       void* regionEnd, BOOL argsUseDefault, s32 argc,
                       char** argv) {
    ApploaderHeader* header;
    OSExecParams* params;
    ESTitleId titleId ALIGN(32);

    OSDisableInterrupts();
    if (__OSInReboot) {
        __OSLaunchPartitionType = *(u32*)0x80003194;
    }

    params = (OSExecParams*)OSAllocFromMEM1ArenaLo(sizeof(OSExecParams), 1);
    params->WORD_0x0 = TRUE;
    params->WORD_0x4 = restartCode;
    params->regionStart = regionStart;
    params->regionEnd = regionEnd;
    params->argsUseDefault = argsUseDefault;

    if (!argsUseDefault) {
        params->argsAddr = OSAllocFromMEM1ArenaLo(0x2000, 1);
        if (__OSLaunchPartitionType == 2 && !__OSInReboot) {
            PackInstallerArgs(params->argsAddr, argc, argv);
        } else {
            PackArgs(params->argsAddr, argc, argv);
        }
    }

    DVDInit();
    DVDSetAutoInvalidation(TRUE);
    DVDResume();

    Prepared = FALSE;
    __DVDPrepareResetAsync(Callback);
    __OSMaskInterrupts(0xFFFFFFF0);
    __OSUnmaskInterrupts(0x10);
    OSEnableInterrupts();

    while (Prepared != TRUE) {
        ;
    }

    __OSLaunchNextFirmware();

    if (restartCode == 0xA0000000 && !__OSInReboot) {
        if (ESP_InitLib() != 0) {
            goto exit;
        }
        if (ESP_GetTitleId(&titleId) != 0) {
            goto exit;
        }
        if (ESP_CloseLib() != 0) {
            goto exit;
        }
        snprintf((char*)params->argsAddr + (u32)argv[1], 17, "%016llx",
                 titleId);
    }

    header = (ApploaderHeader*)OSAllocFromMEM1ArenaLo(0x20, 0x20);
    ReadApploader(header);

    if (CheckApploaderDate(header)) {
        if (dolOffset == 0xFFFFFFFF) {
            dolOffset = ((header->size + 0x20) >> 2) + GetApploaderPosition();
        }
        params->bootDol = dolOffset;
        RunApploader(header, params);
    } else {
        *(void**)0x812FDFF0 = regionStart;
        *(void**)0x812FDFEC = regionEnd;
        *(u8*)0x800030E2 = 1;

        ReadDisc((void*)0x81330000, OSRoundUp32B(header->rebootSize),
                 GetApploaderPosition() + ((header->size + 0x20) >> 2));
        ICInvalidateRange((void*)0x81330000, OSRoundUp32B(header->rebootSize));
        OSDisableInterrupts();
        ICFlashInvalidate();
        Run((void*)0x81330000);
    }

exit:;
}
#pragma pop

void __OSBootDol(u32 dolOffset, u32 restartCode, const char** argv) {
    void* saveStart;
    void* saveEnd;
    char dolOffsetInString[20];
    s32 argvlen;
    char** argvToPass;
    s32 i;

    OSGetSaveRegion(&saveStart, &saveEnd);
    sprintf(dolOffsetInString, "%d", dolOffset);

    argvlen = 0;
    if (argv != NULL) {
        while (argv[argvlen] != NULL) {
            argvlen++;
        }
    }

    argvlen++;
    argvToPass =
        (char**)OSAllocFromMEM1ArenaLo((argvlen + 1) * sizeof(char*), 1);
    argvToPass[0] = dolOffsetInString;

    for (i = 1; i < argvlen; i++) {
        argvToPass[i] = (char*)argv[i - 1];
    }

    __OSBootDolSimple(0xFFFFFFFF, restartCode, saveStart, saveEnd, FALSE,
                      argvlen, argvToPass);
}

// OSLaunchPartition was dead-stripped the same way; only its strings remain.
char OSLaunchPartitionMsgNandApp[] =
    "OSLaunchPartition(): You can't call this API from NAND application.  \n";
char OSLaunchPartitionMsgNotDisc[] =
    "OSLaunchPartition(): Specified title ID is not DISC application.  \n";
