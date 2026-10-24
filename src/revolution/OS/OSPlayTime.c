#include <revolution/AI.h>
#include <revolution/ESP.h>
#include <revolution/NAND.h>
#include <revolution/OS.h>
#include <revolution/VI.h>
#include <cstring>

typedef struct PlayTimeWork {
    u8 buffer[2][576];          // at 0x0
    u32 bufferIndex;            // at 0x480
    s16* dmaAddr;               // at 0x484
    u32 state;                  // at 0x488
    s16 left;                   // at 0x48C
    s16 right;                  // at 0x48E
    AIDMACallback prevCallback; // at 0x490
} PlayTimeWork;

char __OSExpiredPath[] = "/shared2/expired";

OSAlarm __OSExpireAlarm;

static OSTime __OSExpireTime;
OSThread* __OSExpireThread;
BOOL __OSExpireSetExpiredFlag;
PlayTimeWork* __OSPlayTimeWork;

BOOL OSPlayTimeIsLimited(void) {
    return __OSExpireTime != 0;
}

void* __OSPlayTimeRebootThread(void* arg);

void __OSPlayTimeRebootCallback(OSAlarm* alarm, OSContext* ctx) {
    #pragma unused(alarm)
    #pragma unused(ctx)
    register OSThread* thread;
    void* stack;

    for (thread = OS_THREAD_QUEUE.head; thread; thread = thread->nextActive) {
        OSSuspendThread(thread);
    }

    stack = (u8*)OS_USABLE_MEM2_END - 4896;
    thread = (OSThread*)stack;
    if (!OSCreateThread(thread, __OSPlayTimeRebootThread, NULL, (void*)((u8*)stack + 4896), 4096, 0, 0)) {
        __OSHotResetForError();
    }
    OSResumeThread(thread);
}

void __OSPlayTimeFadeLastAIDCallback(void) {
    u8* dst;
    u8* p;
    u32 length;
    u32 count;

    if (__OSPlayTimeWork->prevCallback) {
        __OSPlayTimeWork->prevCallback();
    }

    if (__OSPlayTimeWork->state == 0) {
        __OSPlayTimeWork->dmaAddr = (void*)OSPhysicalToCached(AIGetDMAStartAddr());
    }

    if (__OSPlayTimeWork->state == 1) {
        DCInvalidateRange(__OSPlayTimeWork->dmaAddr, 4);
        __OSPlayTimeWork->left = *__OSPlayTimeWork->dmaAddr++;
        __OSPlayTimeWork->right = *__OSPlayTimeWork->dmaAddr;
    }

    if (__OSPlayTimeWork->state >= 1) {
        dst = __OSPlayTimeWork->buffer[__OSPlayTimeWork->bufferIndex];
        p = dst;
        length = AIGetDMALength();
        count = length;
        while (count != 0) {
            f32 rate = 0.995f;
            count -= 4;
            *(s16*)p = __OSPlayTimeWork->left;
            *(s16*)(p + 2) = __OSPlayTimeWork->right;
            p += 4;
            __OSPlayTimeWork->left = __OSPlayTimeWork->left * rate;
            __OSPlayTimeWork->right = __OSPlayTimeWork->right * rate;
        }
        DCFlushRange(dst, length);
        AIInitDMA(dst, length);
        __OSPlayTimeWork->bufferIndex++;
        __OSPlayTimeWork->bufferIndex &= 1;
    }

    __OSPlayTimeWork->state++;
}

BOOL __OSWriteExpiredFlag(void) {
    s32 result = NAND_RESULT_OK;
    BOOL open = FALSE;
    u8 attr = 0;
    u8 titleId[32] ALIGN(32);
    NANDFileInfo file;

    result = NANDPrivateCreate(__OSExpiredPath, NAND_PERM_RWALL, attr);
    if (result == NAND_RESULT_OK || result == NAND_RESULT_EXISTS) {
        result = NANDPrivateOpen(__OSExpiredPath, &file, NAND_ACCESS_WRITE);
        if (result == NAND_RESULT_OK) {
            open = TRUE;
            result = ESP_InitLib();
            if (result == 0) {
                memset(titleId, 0, sizeof(titleId));
                result = ESP_GetTitleId((ESTitleId*)titleId);
                if (result == 0) {
                    result = NANDWrite(&file, titleId, sizeof(titleId));
                    if (result >= 0) {
                        if (result != 32) {
                            result = NAND_RESULT_INVALID;
                        } else {
                            result = NAND_RESULT_OK;
                        }
                    }
                }
            }
        }
    }

    if (open) {
        NANDClose(&file);
    }

    if (result == NAND_RESULT_OK) {
        return TRUE;
    } else {
        return FALSE;
    }
}

void* __OSPlayTimeRebootThread(void* arg) {
    #pragma unused(arg)
    PlayTimeWork work ALIGN(32);
    BOOL enabled;
    u32 i;
    u32 dim;

    __OSPlayTimeWork = &work;
    memset(&work, 0, sizeof(work));
    __OSPlayTimeWork->prevCallback = AIRegisterDMACallback(__OSPlayTimeFadeLastAIDCallback);

    for (i = 0; i < 20; i++) {
        dim = i / 5 + 1;
        if (dim > 7) {
            dim = 7;
        }
        VIWaitForRetrace();
        __OSSetVIForceDimming(TRUE, dim, dim);
    }

    AIRegisterDMACallback(NULL);
    VISetBlack(TRUE);
    VIFlush();

    enabled = OSDisableInterrupts();
    if (__OSExpireSetExpiredFlag) {
        __OSWriteExpiredFlag();
    }
    OSRestoreInterrupts(enabled);

    OSReturnToMenu();
    return NULL;
}

void __OSPlayTimeAlarmExpired(OSAlarm* alarm, OSContext* ctx) {
    #pragma unused(alarm)
    #pragma unused(ctx)
    OSThread* thread;
    void* stack;

    if (__OSExpireThread) {
        for (thread = OS_THREAD_QUEUE.head; thread; thread = thread->nextActive) {
            if (thread != __OSExpireThread) {
                OSSuspendThread(thread);
            }
        }

        OSCreateAlarm(&__OSExpireAlarm);
        OSSetAlarm(&__OSExpireAlarm, OS_SEC_TO_TICKS(240LL), __OSPlayTimeRebootCallback);

        while (__OSExpireThread->suspend > 0) {
            OSResumeThread(__OSExpireThread);
        }
    } else {
        for (thread = OS_THREAD_QUEUE.head; thread; thread = thread->nextActive) {
            OSSuspendThread(thread);
        }

        stack = (u8*)OS_USABLE_MEM2_END - 4896;
        thread = (OSThread*)stack;
        if (!OSCreateThread(thread, __OSPlayTimeRebootThread, NULL, (u8*)stack + 4896, 4096, 0, 0)) {
            __OSHotResetForError();
        }
        OSResumeThread(thread);
    }
}

s32 __OSGetPlayTime(ESTicketView* ticketView, u32* type, register u32* remainingTime) {
    ESTicketView alignedTicketView ALIGN(32);
    ESLpEntry entries[ES_LIMIT_MAX] ALIGN(32);
    register int foundNonZero;
    u32 numEntries;
    s32 result;
    int i;
    u32 code;

    numEntries = 0;
    foundNonZero = 0;

    if ((u32)ticketView & 0x1F) {
        memcpy(&alignedTicketView, ticketView, sizeof(ESTicketView));
        ticketView = &alignedTicketView;
    }

    result = ESP_GetConsumption(ticketView->ticketId, NULL, &numEntries);
    if (result <= 0) {
        switch (result) {
        case 0:
            if (numEntries != 0) {
                switch (result = ESP_GetConsumption(ticketView->ticketId, entries, &numEntries)) {
                case 0:
                    break;
                }
            }
            break;
        }
    }

    if (result == 0) {
        for (i = 0; i < ES_LIMIT_MAX; i++) {
            code = ticketView->limits.data[i].code;
            if (code == 1) {
                *type = 1;
                if (numEntries == 0) {
                    *remainingTime = ticketView->limits.data[i].limit;
                } else if (entries[i].limit >= ticketView->limits.data[i].limit) {
                    *remainingTime = 0;
                } else {
                    *remainingTime = ticketView->limits.data[i].limit - entries[i].limit;
                }
                goto done;
            }

            if (code != 0) {
                foundNonZero = i + 1;
            }
        }

        if (foundNonZero == 0) {
            *type = 0;
            *remainingTime = (u32)-1;
        } else {
            foundNonZero--;
            if (ticketView->limits.data[foundNonZero].code == 4) {
                *type = 4;
                *remainingTime = ticketView->limits.data[foundNonZero].limit;
                if (numEntries != 0) {
                    *remainingTime -= entries[foundNonZero].limit;
                }
            } else {
                *type = 9;
            }
        }
    }

done:
    return result;
}

void __OSInitPlayTime(void) {
    ESTicketView ticketView ALIGN(32);
    u32 remainingTime;
    s32 type;
    s32 result;

    __OSExpireTime = 0;
    __OSExpireThread = NULL;
    __OSExpireSetExpiredFlag = TRUE;

    switch (ESP_InitLib()) {
    case 0:
        result = ESP_DiGetTicketView(NULL, &ticketView);
        if (result == -1017) {
        } else {
            switch (result) {
            case 0:
                result = __OSGetPlayTime(&ticketView, (u32*)&type, &remainingTime);
                break;
            }
        }

        if (result != 0) {
            switch (result) {
            case -1017:
                break;
            }
        } else {
            if (type == 0) {
            } else if (type == 1) {
                if (remainingTime == 0) {
                    OSPanic("OSPlayTime.c", 775, "Expired");
                }
                OSCreateAlarm(&__OSExpireAlarm);
                OSSetAlarm(&__OSExpireAlarm, OS_SEC_TO_TICKS((OSTime)remainingTime + 1), __OSPlayTimeAlarmExpired);
                __OSExpireTime = __OSExpireAlarm.end;
                OSReport("PlayTime: %d seconds left\n\0", remainingTime);
            }
        }
        break;
    }

    ESP_CloseLib();
}
