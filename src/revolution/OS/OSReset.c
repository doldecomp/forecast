#include <revolution/DVD.h>
#include <revolution/OS.h>
#include <revolution/PAD.h>
#include <revolution/SC.h>
#include <revolution/VI.h>

#include <cstring>

static OSShutdownFunctionQueue ShutdownFunctionQueue;
BOOL __OSShutdownToMenu;
static u32 bootThisDol;
static void* SaveEnd;
static void* SaveStart;

static void KillThreads(void);

#pragma dont_inline on
void __OSReboot(u32 resetCode, u32 bootDol) {
    OSDisableInterrupts();
    OSSetArenaLo(OS_EXEC_ARENA_LO);
    OSSetArenaHi(OS_EXEC_ARENA_HI);
    {
        char* argvToPass = NULL;
        __OSNextPartitionType = OS_BOOT_PARTITION_TYPE;
        __OSBootDol(bootDol, resetCode | 0x80000000, (const char**)&argvToPass);
    }
}
#pragma dont_inline reset

void OSGetSaveRegion(void** start, void** end) {
    *start = SaveStart;
    *end = SaveEnd;
}

void OSRegisterShutdownFunction(OSShutdownFunctionInfo* info) {
    OSShutdownFunctionInfo* it;
    OSShutdownFunctionInfo* prev;

    for (it = ShutdownFunctionQueue.head; it != NULL && it->prio <= info->prio;
         it = it->next) {
        ;
    }

    if (it == NULL) {
        prev = ShutdownFunctionQueue.tail;
        if (prev == NULL) {
            ShutdownFunctionQueue.head = info;
        } else {
            ShutdownFunctionQueue.tail->next = info;
        }

        info->prev = prev;
        info->next = NULL;

        it = ShutdownFunctionQueue.head;
        ShutdownFunctionQueue.tail = info;
    } else {
        info->next = it;

        prev = it->prev;
        it->prev = info;
        info->prev = prev;

        if (prev == NULL) {
            ShutdownFunctionQueue.head = info;
        } else {
            prev->next = info;
        }
    }
}

BOOL __OSCallShutdownFunctions(u32 pass, u32 event) {
    OSShutdownFunctionInfo* iter;
    BOOL failure;
    u32 prio;

    prio = 0;
    failure = FALSE;

    for (iter = ShutdownFunctionQueue.head; iter != NULL; iter = iter->next) {
        if (failure && prio != iter->prio) {
            break;
        }

        failure |= !iter->func(pass, event);
        prio = iter->prio;
    }

    failure |= !__OSSyncSram();

    return !failure;
}

void __OSShutdownDevices(u32 event) {
    BOOL padIntr;
    BOOL osIntr;
    BOOL keepEnable;

    switch (event) {
    case OS_SD_EVENT_FATAL:
    case OS_SD_EVENT_RETURN_TO_MENU:
    case OS_SD_EVENT_LAUNCH_APP:
        keepEnable = FALSE;
        break;
    case OS_SD_EVENT_1:
    case OS_SD_EVENT_SHUTDOWN:
    case OS_SD_EVENT_3:
    case OS_SD_EVENT_RESTART:
    default:
        keepEnable = TRUE;
        break;
    }

    __OSStopAudioSystem();

    if (!keepEnable) {
        padIntr = __PADDisableRecalibration(TRUE);
    }

    while (!__OSCallShutdownFunctions(FALSE, event)) {
        ;
    }

    while (!__OSSyncSram()) {
        ;
    }

    osIntr = OSDisableInterrupts();
    __OSCallShutdownFunctions(TRUE, event);
    LCDisable();

    if (!keepEnable) {
        __PADDisableRecalibration(padIntr);
    }

    KillThreads();
}

u8 __OSGetDiscState(u8 last) {
    u32 flags;

    if (__DVDGetCoverStatus() != DVD_COVER_CLOSED) {
        return 3;
    } else if (last == 1) {
        if (!__OSGetRTCFlags(&flags) || flags == 0) {
            goto status_1;
        }
status_2:
        return 2;
    } else {
        goto status_2;
status_1:
        return 1;
    }
}

static void KillThreads(void) {
    OSThread* iter;
    OSThread* next;

    for (iter = OS_THREAD_QUEUE.head; iter != NULL; iter = next) {
        next = iter->nextActive;

        switch (iter->state) {
        case OS_THREAD_STATE_SLEEPING:
        case OS_THREAD_STATE_READY:
            OSCancelThread(iter);
            break;
        default:
            break;
        }
    }
}

void OSShutdownSystem(void) {
    SCIdleModeInfo idleMode;
    OSStateFlags stateFlags;
    OSIOSRev iosRev;

    memset(&idleMode, 0, sizeof(SCIdleModeInfo));
    SCInit();
    while (SCCheckStatus() == SC_STATUS_BUSY) {
        ;
    }
    SCGetIdleMode(&idleMode);

    __OSStopPlayRecord();
    __OSUnRegisterStateEvent();
    __DVDPrepareReset();
    __OSReadStateFlags(&stateFlags);

    stateFlags.discState = __OSGetDiscState(stateFlags.discState);
    if (idleMode.wc24 == TRUE) {
        stateFlags.BYTE_0x5 = 5;
    } else {
        stateFlags.BYTE_0x5 = 1;
    }

    __OSClearRTCFlags();
    __OSWriteStateFlags(&stateFlags);
    __OSGetIOSRev(&iosRev);

    if (idleMode.wc24 == TRUE) {
        __OSShutdownToMenu = TRUE;
        OSDisableScheduler();
        __OSShutdownDevices(OS_SD_EVENT_RETURN_TO_MENU);
        OSEnableScheduler();
        __OSLaunchMenu();
    } else {
        OSDisableScheduler();
        __OSShutdownDevices(OS_SD_EVENT_SHUTDOWN);
        __OSShutdownToSBY();
    }
}

void OSRestart(u32 resetCode) {
    u8 appType = OSGetAppType();

    __OSStopPlayRecord();
    __OSUnRegisterStateEvent();

    if (appType == OS_APP_TYPE_CHANNEL) {
        OSDisableScheduler();
        __OSShutdownDevices(OS_SD_EVENT_RESTART);
        OSEnableScheduler();
        __OSRelaunchTitle();
    } else if (appType == OS_APP_TYPE_DVD) {
        OSDisableScheduler();
        __OSShutdownDevices(OS_SD_EVENT_RESTART);
        OSEnableScheduler();
        __OSReboot(resetCode, bootThisDol);
    }

    OSDisableScheduler();
    __OSShutdownDevices(OS_SD_EVENT_1);
    __OSHotResetForError();
}

static void ReturnToMenu(u8 menuMode) {
    OSStateFlags stateFlags;

    __OSStopPlayRecord();
    __OSUnRegisterStateEvent();
    __DVDPrepareReset();

    __OSReadStateFlags(&stateFlags);
    stateFlags.discState = __OSGetDiscState(stateFlags.discState);
    stateFlags.BYTE_0x5 = 3;
    stateFlags.BYTE_0x7 = menuMode;
    __OSClearRTCFlags();
    __OSWriteStateFlags(&stateFlags);

    OSDisableScheduler();
    __OSShutdownDevices(OS_SD_EVENT_RETURN_TO_MENU);
    OSEnableScheduler();

    __OSLaunchMenu();
    OSDisableScheduler();
    __VISetRGBModeImm();
    __OSHotResetForError();
}

void OSReturnToMenu(void) {
    ReturnToMenu(0);
    OSPanic("OSReset.c", 834, "OSReturnToMenu(): Falied to boot system menu.\n");
}

// OSReturnToDataManager and OSSetBootDol are not in this DOL: the game never
// calls them, so the linker dead-stripped their code, but their pooled strings
// stayed in .data. Keep only the strings.
char OSReturnToDataManagerMsg[] =
    "OSReturnToDataManager(): Falied to boot system menu.\n";

void OSReturnToSetting(u8 setting) {
    const char* arg;

    switch (setting) {
    case 1:
        arg = "Calendar/Calendar_index.html";
        break;
    case 2:
        arg = "Display/Display_index.html";
        break;
    case 3:
        arg = "Sound/Sound_index.html";
        break;
    case 4:
        arg = "Parental_Control/Parental_Control_index.html";
        break;
    case 5:
        arg = "Internet/Internet_index.html";
        break;
    case 6:
        arg = "WiiConnect24/Wiiconnect24_index.html";
        break;
    case 7:
        arg = "Update/Update_index.html";
        break;
    case 0:
    default:
        OSReport("OSReturnToSetting(): You can't specify %d.  \n", setting);
        OSPanic("OSReset.c", 910, "");
        break;
    }

    __OSReturnToMenul(1, arg, NULL);
}

void __OSReturnToMenuForError(void) {
    OSStateFlags stateFlags;

    __OSReadStateFlags(&stateFlags);
    stateFlags.discState = 2;
    stateFlags.BYTE_0x5 = 3;
    __OSClearRTCFlags();
    __OSWriteStateFlags(&stateFlags);

    __OSLaunchMenu();
    OSDisableScheduler();
    __VISetRGBModeImm();
    __OSHotResetForError();
    OSPanic("OSReset.c", 949, "__OSReturnToMenu(): Falied to boot system menu.\n");
}

void __OSHotResetForError(void) {
    if (__OSInNandBoot || __OSInReboot) {
        __OSInitSTM();
    }

    __OSHotReset();
    OSPanic("OSReset.c", 973, "__OSHotReset(): Falied to reset system.\n");
}

u32 OSGetResetCode(void) {
    if (__OSRebootParams.WORD_0x0 != 0) {
        return __OSRebootParams.WORD_0x4 | 0x80000000;
    }

    return PI_HW_REGS[PI_RESET] >> 3;
}

void OSResetSystem(int reset, u32 resetCode, BOOL forceMenu) {
#pragma unused(reset)
#pragma unused(resetCode)
#pragma unused(forceMenu)

    OSPanic("OSReset.c", 1124, "OSResetSystem() is obsoleted. It doesn't work any longer.\n");
}

char OSSetBootDolMsg[] =
    "OSSetBootDol() is obsoleted. It doesn't work any longer.\n";
