#include <channel/WiiConnect24.h>
#include <revolution/CX.h>
#include <revolution/NAND.h>
#include <revolution/VF/d_vf.h>
#include <revolution/OS.h>
#include <revolution/NWC24/NWC24Download.h>
#include <revolution/NWC24/NWC24Manage.h>
#include <revolution/NWC24/NWC24Schedule.h>
#include <revolution/NWC24/internal/NWC24iManage.h>
#include <cstdio>
#include <types.h>

extern OSMessageQueue gWC24Queue;
extern u8 gWC24FileBuffer[];
extern volatile u8 gWC24DownloadComplete;
extern volatile u8 gWC24NetInitialized;
extern volatile u8 gWC24ShutdownFlag1;
extern volatile u8 gWC24ShutdownFlag2;
void* WC24Alloc(u32 unused, s32 size);
void WC24Free(void* unused, void* block, s32 size);
extern "C" s32 SOInit(void* init);
extern "C" s32 SOStartup();
extern "C" s32 SOGetLastError();
extern NWC24Work gNWC24Work;
extern volatile u8 gNWC24LibOpened;

char sVfDriveName[] = "@24";
char sVfRootPath[] = "@24:/*";
char sVfReadMode[] = "r";
char sBinNameFormat[] = "%d.bin";
char sForecastFileSuffix[] = "%sB";
char sShortFileSuffix[] = "%sR";


s32 MapDownloadError(s32 error);
s32 DownloadFiles(WC24Task* task, u32 doForecast, u32 doShort);
s32 CreateDownloadTasks(WC24Task* task, u32 doForecast, u32 doShort, u8 redownload,
                                   u16 newInterval, u16 count, u32* idOut);
s32 ExecuteDownloadTask(WC24Task* task, s32 index, u32 unused, u16 unused2);
s32 DeleteTaskFiles(WC24Task* task, u32 doForecast, u32 doShort);
s32 ReadCompressedFile(WC24Task* task, void* fd, void* dest, u32 destSize,
                                  u32* outSize);

struct WC24IdleMode {
    u8 wc24;
    u8 slotLight;
};
// The SDK header declares this as void, but the channel uses the result.
extern "C" BOOL SCGetIdleMode(WC24IdleMode* mode);

static inline void SetTaskMessage(WC24Task* task, const char* msg, s32 code, s32 result) {
    task->mErrorCode = code;
    task->mNwc24Result = result;
    sprintf(task->mMessage, "%s %d %d", msg, code, result);
}

// A macro, not an inline: the message literal must reach SetTaskMessage directly.
// Passing it through another inline parameter keeps it live across
// NWC24GetErrorCode() in a callee-saved register, which the original does not do.
#define SetTaskError(task, msg, result)     (SetTaskMessage(task, msg, NWC24GetErrorCode(), result), MapDownloadError(result))

static inline NWC24Err OpenLib(WC24Task* task, s64 retry) {
    NWC24Err error;

    for (;;) {
        error = NWC24OpenLib(&gNWC24Work);

        if (error != NWC24_ERR_MUTEX && error != NWC24_ERR_BUSY &&
            error != NWC24_ERR_INPROGRESS)
            break;

        OSSleepTicks(OS_MSEC_TO_TICKS(100 * (retry + 1)));
    }

    if (error != NWC24_OK)
        return (NWC24Err)SetTaskError(task, "NWC24OpenLib() failed.", error);

    gNWC24LibOpened = 1;
    return NWC24_OK;
}

static inline NWC24Err CheckLib(WC24Task* task) {
    NWC24Err error = NWC24Check(2);

    if (error != NWC24_OK)
        return (NWC24Err)SetTaskError(task, "NWC24Check() failed.", error);

    WC24IdleMode idle;
    if (SCGetIdleMode(&idle) == 0 || idle.wc24 != 1)
        return NWC24_ERR_INVALID_VALUE;

    return NWC24_OK;
}

static inline NWC24Err CloseLib(WC24Task* task) {
    NWC24Err error = NWC24CloseLib();

    if (error != NWC24_OK)
        return (NWC24Err)SetTaskError(task, "NWC24CloseLib() failed.", error);

    gNWC24LibOpened = 0;
    return NWC24_OK;
}

void* DownloaderThread(void* unused) {
    WC24Task* task;
    void* message;
    s64 retry = 0;
    s32 result;
    u8 opened;
    s32 attempts;
    u8 done;

    for (;;) {
        done = FALSE;
        OSReceiveMessage(&gWC24Queue, &message, 1);

        task = (WC24Task*)message;
        if (task == NULL)
            break;

        switch (task->mType) {
        case 0:
            opened = FALSE;
            attempts = 0;

            for (;;) {
                task->mState = 2;
                task->mResult = OpenLib(task, retry);
                if (task->mResult != NWC24_OK)
                    break;

                opened = TRUE;

                task->mResult = CheckLib(task);
                if (task->mResult != NWC24_OK)
                    break;

                task->mState = 3;
                if ((task->mResult = DownloadFiles(task, 1, 1)) == NWC24_OK)
                    break;
                if (task->mResult == NWC24_ERR_FATAL && attempts != 1) {
                    task->mState = 6;
                    task->mResult = CreateDownloadTasks(task, 1, 1, 1, 0x1E, 0x3C, 0);
                    if (task->mResult == NWC24_OK) {
                        task->mState = 2;
                        task->mResult = CloseLib(task);
                        opened = FALSE;
                        if (task->mResult == NWC24_OK) {
                            task->mState = 5;
                            task->mResult = ExecuteDownloadTask(task, 0, 0xFFFFFF, 0xFFFF);
                            if (task->mResult == NWC24_OK) {
                                task->mResult = ExecuteDownloadTask(task, 1, 0xFFFFFF, 0xFFFF);
                                if (task->mResult == NWC24_OK) {
                                    attempts++;
                                    continue;
                                }
                            }
                        }
                    }
                }
                break;
            }

            if (opened) {
                task->mState = 2;
                result = CloseLib(task);
                if (task->mResult == NWC24_OK)
                    task->mResult = result;
            }
            task->mState = 0;
            break;

        case 1:
            task->mState = 5;
            task->mResult = ExecuteDownloadTask(task, 0, 0xFFFFFF, 0xFFFF);
            if (task->mResult == NWC24_OK) {
                task->mState = 2;
                task->mResult = OpenLib(task, retry);
                if (task->mResult == NWC24_OK) {
                    task->mResult = CheckLib(task);
                    if (task->mResult == NWC24_OK) {
                        task->mState = 3;
                        switch (task->mTaskId) {
                        case 3:
                            task->mResult = DownloadFiles(task, 1, 0);
                            break;
                        case 4:
                            task->mResult = DownloadFiles(task, 1, 0);
                            break;
                        }
                    }
                    task->mState = 2;
                    result = CloseLib(task);
                    if (task->mResult == NWC24_OK)
                        task->mResult = result;
                }
            }
            task->mState = 0;
            break;

        case 2:
            task->mState = 2;
            task->mResult = OpenLib(task, retry);
            if (task->mResult == NWC24_OK) {
                task->mResult = CheckLib(task);
                if (task->mResult == NWC24_OK) {
                    task->mState = 6;
                    task->mResult = CreateDownloadTasks(task, 1, 1, task->mRedownload, task->mDownloadInterval,
                                                         task->mDownloadCount, 0);
                }
                task->mState = 2;
                result = CloseLib(task);
                if (task->mResult == NWC24_OK)
                    task->mResult = result;
            }
            task->mState = 0;
            break;

        case 3:
            task->mState = 2;
            task->mResult = OpenLib(task, retry);
            if (task->mResult == NWC24_OK) {
                task->mResult = CheckLib(task);
                if (task->mResult == NWC24_OK) {
                    task->mState = 7;
                    task->mResult = DeleteTaskFiles(task, 1, 1);
                }
                task->mState = 2;
                result = CloseLib(task);
                if (task->mResult == NWC24_OK)
                    task->mResult = result;
            }
            task->mState = 0;
            break;

        case 4:
            done = TRUE;
            break;
        }

        if (!done)
            continue;
        break;
    }

    return NULL;
}

s32 DownloadFiles(WC24Task* task, u32 doForecast, u32 doShort) {
    NWC24DlTask dlTasks[2];
    u8 dta[0x448];
    char url[0x100];
    char vfName[0x50];
    char fileName[0x10];
    void* buffers[2];
    s64* times[2];
    u32* sizes[2];
    u16 ids[2];
    u32 outSize;

    s32 error;
    BOOL found = FALSE;

    if (doForecast != 0) {
        ids[0] = task->mTaskId;
        buffers[0] = task->mBuffers.forecast;
        times[0] = task->mTimes.forecast;
        sizes[0] = task->mSizes.forecast;
    } else {
        ids[0] = 0;
        buffers[0] = NULL;
        times[0] = NULL;
        sizes[0] = NULL;
    }

    if (doShort != 0) {
        ids[1] = task->mSubTaskId;
        buffers[1] = task->mBuffers.shortForecast;
        times[1] = task->mTimes.shortForecast;
        sizes[1] = task->mSizes.shortForecast;
    } else {
        ids[1] = 0;
        buffers[1] = NULL;
        times[1] = NULL;
        sizes[1] = NULL;
    }

    // The loops below address the same task through dlTasks and through a
    // second pointer to it; the original keeps one induction pointer for each.
    NWC24DlTask* urlTask = dlTasks;
    for (s32 i = 0; i < 2; i++) {
        if (buffers[i] == NULL)
            continue;

        error = NWC24GetDlTask(&dlTasks[i], ids[i]);
        if (error == -0xD) {
            SetTaskMessage(task, "NWC24GetDlTask() failed.", NWC24GetErrorCode(), error);
            return -1;
        }

        if (error != 0) {
            return SetTaskError(task, "NWC24GetDlTask() failed.", error);
        }

        error = NWC24GetDlUrl(&urlTask[i], url, 0xFF);
        if (error != 0) {
            return SetTaskError(task, "NWC24GetDlUrl() failed.", error);
        }

        if ((&task->mForecastUrl)[i] != NULL) {
            u32 j = 0;
            while (j < 0xFF && url[j] != 0) {
                if (url[j] != (&task->mForecastUrl)[i][j]) {
                    SetTaskMessage(task, "URL is not same.", 0, 0);
                    return -1;
                }

                j++;
            }
        }
    }

    for (s32 i = 0; i < 2; i++) {
        if (buffers[i] == NULL)
            continue;

        error = NWC24GetDlVfName(&dlTasks[i], vfName, 0x50);
        if (error != 0) {
            return SetTaskError(task, "NWC24GetDlVfName() failed.", error);
        }

        error = VFMountDriveNANDFlashEx(sVfDriveName, vfName);
        if (error != 0) {
            SetTaskMessage(task, "VFMountDriveNANDFlash() failed.", 0, error);
            return -1;
        }

        s32 searchResult = VFFileSearchFirst(dta, sVfRootPath, 0x7F);
        while (searchResult == 0)
            searchResult = VFFileSearchNext(dta);

        found = TRUE;
        break;
    }

    if (!found)
        OSPanic("WiiConnect24.cpp", 0x4B1, "VF Load Failed.");

    NWC24DlTask* pTask = dlTasks;
    for (s32 i = 0; i < 2; i++) {
        if (buffers[i] == NULL)
            continue;

        error = NWC24GetDlFilename(&dlTasks[i], fileName, 0x10);
        if (error != 0) {
            return SetTaskError(task, "NWC24GetDlFilename() failed.", error);
        }

        void* file = VFOpenFile(fileName, sVfReadMode, 0);
        if (file == NULL) {
            SetTaskMessage(task, "VFOpenFile() failed.", NWC24GetErrorCode(), 0);

            error = VFUnmountDrive(sVfDriveName);
            if (error != 0) {
                SetTaskMessage(task, "VFUnmountDrive() failed.", 0, error);
                return -7;
            }
            return -1;
        }

        error = ReadCompressedFile(task, file, buffers[i], task->mBufferSizes[i], &outSize);
        if (error != 0)
            return error;

        *sizes[i] = outSize;

        // Pads the size to a multiple of 4, but writes into the task itself
        // rather than into the downloaded buffer.
        char* p;
        u32 size = outSize;
        p = (char*)&task->mBuffers + size;
        while (size < ((outSize + 3) & ~3)) {
            *p++ = 0;
            size++;
        }

        error = VFCloseFile(file);
        if (error != 0) {
            SetTaskMessage(task, "VFCloseFile() failed.", 0, error);
            VFUnmountDrive(sVfDriveName);
            return -7;
        }

        error = NWC24GetDlLastUpdate(&dlTasks[i], times[i]);
        if (error != 0) {
            return SetTaskError(task, "NWC24GetDlLastUpdate() failed.", error);
        }

        error = NWC24GetDlNextTime(&pTask[i], &task->mNextDownloadTimes[i]);
        if (error != 0) {
            return SetTaskError(task, "NWC24GetDlNextTime() failed.", error);
        }
    }

    error = VFUnmountDrive(sVfDriveName);
    if (error != 0) {
        SetTaskMessage(task, "VFUnmoundDrive() failed.", 0, error);
        return -7;
    }

    return 0;
}

s32 CreateDownloadTasks(WC24Task* task, u32 doForecast, u32 doShort, u8 redownload,
                                   u16 newInterval, u16 count, u32* idOut) {
    NWC24DlTask dlTasks[2];
    u8 search[0x448];
    char url[0x100];
    char vfName[0x50];
    char vfName2[0x50];
    char fileName[0x10];
    char nameBuf[0x10];
    const char* urls[2];
    u16 ids[2];
    u16 interval;

    s32 error;
    BOOL redo = redownload;
    BOOL init = redownload;
    BOOL mounted = FALSE;

    if (doForecast != 0) {
        ids[0] = task->mTaskId;
        urls[0] = task->mForecastUrl;
    } else {
        ids[0] = 0;
        urls[0] = NULL;
    }

    if (doShort != 0) {
        ids[1] = task->mSubTaskId;
        urls[1] = task->mShortUrl;
    } else {
        ids[1] = 0;
        urls[1] = NULL;
    }

    NWC24DlTask* pTask = dlTasks;
    for (s32 i = 0; i < 2; i++) {
        if (ids[i] == 0)
            continue;

        if (redo != 0)
            goto firstDone;

        error = NWC24GetDlTask(&dlTasks[i], ids[i]);
        if (error == -0xD) {
            redo = TRUE;
            init = TRUE;
            goto firstDone;
        }

        if (error != 0)
            return SetTaskError(task, "NWC24GetDlTask() failed.", error);

        error = NWC24GetDlVfName(&pTask[i], vfName, 0x50);
        if (error != 0)
            return SetTaskError(task, "NWC24GetDlVfName() failed.", error);

        if (!mounted) {
            error = VFMountDriveNANDFlashEx(sVfDriveName, vfName);
            if (error == 2 || error == 0xB001) {
                redo = TRUE;
                init = TRUE;
                goto firstDone;
            }

            if (error != 0) {
                SetTaskMessage(task, "VFMountDriveNANDFlash() failed.", 0, error);
                return -7;
            }

            mounted = TRUE;
            s32 searchResult = VFFileSearchFirst(search, sVfRootPath, 0x7F);
            while (searchResult == 0)
                searchResult = VFFileSearchNext(search);
        }

        if (!mounted)
            continue;

        switch (ids[i]) {
        case 3:
        case 4: {
            error = NWC24GetDlFilename(&pTask[i], fileName, 0x10);
            if (error != 0)
                return SetTaskError(task, "NWC24GetDlFilename() failed.", error);

            void* file = VFOpenFile(fileName, sVfReadMode, 0);
            if (file == NULL) {
                redo = TRUE;
                init = TRUE;
                continue;
            }

            error = VFCloseFile(file);
            if (error != 0) {
                SetTaskMessage(task, "VFCloseFile() failed.", 0, error);
                VFUnmountDrive(sVfDriveName);
                return -7;
            }
            break;
        }
        }
    }

firstDone:
    if (mounted) {
        error = VFUnmountDrive(sVfDriveName);
        if (error != 0) {
            SetTaskMessage(task, "VFUnmountDrive() failed.", 0, error);
            return -7;
        }
    }

    if (!redo) {
        for (s32 i = 0; i < 2; i++) {
            if (ids[i] == 0)
                continue;

            error = NWC24GetDlUrl(&dlTasks[i], url, 0xFF);
            if (error != 0)
                return SetTaskError(task, "NWC24GetDlUrl() failed.", error);

            const char* p = url;
            u32 j = 0;
            while (j < 0xFF && *p != 0) {
                if (*p != urls[i][j]) {
                    redo = TRUE;
                    init = TRUE;
                    break;
                }
                p++;
                j++;
            }

            error = NWC24GetDlInterval(&dlTasks[i], &interval);
            if (error != 0)
                return SetTaskError(task, "NWC24GetDlInterval() failed.", error);

            if (newInterval != interval)
                redo = TRUE;
        }
    }

    if (redo) {
        BOOL created = FALSE;
        NWC24DlTask* pTask2 = dlTasks;

        for (s32 i = 0; i < 2; i++) {
            if (ids[i] == 0)
                continue;

            if (init) {
                error = NWC24InitDlTask(&dlTasks[i], 1);
                if (error != 0)
                    return SetTaskError(task, "NWC24InitDlTask() failed.", error);

                error = NWC24SetDlId(&dlTasks[i], ids[i]);
                if (error != 0)
                    return SetTaskError(task, "NWC24SetDlId() failed.", error);

                error = NWC24GetDlVfName(&pTask2[i], vfName2, 0x50);
                if (error != 0)
                    return SetTaskError(task, "NWC24GetDlVfName() failed.", error);

                if (!created) {
                    error = NANDDelete(vfName2);
                    if (error != -0xC && error != 0) {
                        SetTaskMessage(task, "NANDDelete() failed.", 0, error);
                        return -8;
                    }

                    error = NWC24CreateDlVf(&pTask2[i], task->mForecastBufferSize);
                    if (error != 0)
                        return SetTaskError(task, "NWC24CreateDlVf() failed.", error);

                    created = TRUE;
                }
            }

            error = NWC24SetDlUrl(&dlTasks[i], urls[i]);
            if (error != 0)
                return SetTaskError(task, "NWC24SetDlUrl() failed.", error);

            s32 priority;
            s32 margin;
            switch (ids[i]) {
            case 3:
                priority = 100;
                margin = 0x2D0;
                break;
            case 4:
                priority = 100;
                margin = 0x2D0;
                break;
            }

            error = NWC24SetDlPriority(&dlTasks[i], priority);
            if (error != 0)
                return SetTaskError(task, "NWC24SetDlPriority() failed.", error);

            error = NWC24SetDlFlags(&dlTasks[i], 0x40000000);
            if (error != 0)
                return SetTaskError(task, "NWC24SetDlFlags() failed.", error);

            error = NWC24SetDlInterval(&dlTasks[i], newInterval);
            if (error != 0)
                return SetTaskError(task, "NWC24SetDlInterval() failed.", error);

            error = NWC24SetDlRetryMargin(&dlTasks[i], margin);
            if (error != 0)
                return SetTaskError(task, "NWC24SetDlRetryMargin() failed.", error);

            sprintf(nameBuf, sBinNameFormat, ids[i]);
            error = NWC24SetDlFilename(&dlTasks[i], nameBuf);
            if (error != 0)
                return SetTaskError(task, "NWC24SetDlFilename() failed.", error);
        }
    }

    NWC24DlTask* pTask3 = dlTasks;
    for (s32 i = 0; i < 2; i++) {
        if (ids[i] == 0)
            continue;

        error = NWC24SetDlCount(&dlTasks[i], (s16)count);
        if (error != 0)
            return SetTaskError(task, "NWC24SetDlCount() failed.", error);

        if (redo) {
            error = NWC24AddDlTask(&dlTasks[i]);
            if (error != 0)
                return SetTaskError(task, "NWC24AddDlTask() failed.", error);
        } else {
            error = NWC24UpdateDlTask(&dlTasks[i]);
            if (error != 0)
                return SetTaskError(task, "NWC24UpdateDlTask() failed.", error);
        }

        if (idOut != NULL) {
            error = NWC24GetDlId(&pTask3[i], idOut);
            if (error != 0)
                return SetTaskError(task, "NWC24GetDlId() failed.", error);
        }

        NWC24iCheckDlTask(&pTask3[i]);
    }

    gWC24DownloadComplete = 1;
    return 0;
}

struct WC24SOConfig {
    void* alloc;
    void* free;
};

s32 ExecuteDownloadTask(WC24Task* task, s32 index, u32 unused, u16 unused2) {
    s32 initResult;
    s32 startupResult;
    s32 error;
    s32 taskId;
    WC24SOConfig config;

    taskId = (&task->mTaskId)[index];

    if (!gWC24NetInitialized) {
        gWC24ShutdownFlag2 = 1;
        config.alloc = (void*)WC24Alloc;
        config.free = (void*)WC24Free;

        initResult = SOInit(&config);
        if (initResult < 0) {
            SetTaskMessage(task, "SOInit() failed.", 0, initResult);
            gWC24ShutdownFlag2 = 0;
            return -9;
        }

        startupResult = SOStartup();
        if (startupResult < 0) {
            SetTaskMessage(task, "SOStartup() failed.", SOGetLastError(), startupResult);
            gWC24ShutdownFlag2 = 0;
            return -9;
        }

        gWC24NetInitialized = 1;
        gWC24ShutdownFlag2 = 0;
    }

    switch (taskId) {
    case 3:
    case 4:
        gWC24ShutdownFlag1 = 1;
        error = NWC24ExecDownloadTask(2, taskId, 0);
        gWC24ShutdownFlag1 = 0;

        if (error != 0) {
            SetTaskMessage(task, "NWC24ExecDownloadTask() failed.", NWC24GetErrorCode(), error);
            return MapDownloadError(error);
        }
        break;
    }
    return 0;
}

s32 DeleteTaskFiles(WC24Task* task, u32 doForecast, u32 doShort) {
    NWC24DlTask dlTask;
    char vfName[0x50];
    u16 ids[2];
    u8 found;

    found = FALSE;

    if (doForecast != 0)
        ids[0] = task->mTaskId;
    else
        ids[0] = 0;

    if (doShort != 0)
        ids[1] = task->mSubTaskId;
    else
        ids[1] = 0;

    for (s32 i = 0; i < 2; i++) {
        if (ids[i] == 0)
            continue;

        s32 error = NWC24GetDlTask(&dlTask, ids[i]);
        if (error == -0xD)
            continue;

        if (error != 0) {
            return SetTaskError(task, "NWC24GetDl*() failed.", error);
        }

        if (!found) {
            found = TRUE;
            error = NWC24GetDlVfName(&dlTask, vfName, 0x50);
            if (error != 0) {
                return SetTaskError(task, "NWC24GetDlVfName() failed.", error);
            }
        }

        error = NWC24DeleteDlTask(&dlTask);
        if (error != 0) {
            return SetTaskError(task, "NWC24DeleteDlTask() failed.", error);
        }
    }

    if (found) {
        s32 error = NANDDelete(vfName);
        if (error != -0xC && error != 0) {
            SetTaskMessage(task, "NANDDelete() failed.", 0, error);
            return -8;
        }
    }

    return 0;
}

s32 MapDownloadError(s32 error) {
    s32 mapped = -6;

    if ((error <= -0x10 && error >= -0x15) || error == -0x26 || error == -0x29 || error == -0x2B ||
        error == -0x2E) {
        mapped = -0xB;
    } else if (error == -0x31) {
        mapped = -0xC;
    } else if (error == -1 || error == -0xB || error == -0x2A) {
        mapped = -2;
    } else if ((u32)(NWC24GetErrorCode() + 0x20000) == 0x55CE) {
        mapped = -5;
    } else {
        switch (error) {
        case -0x27:
            mapped = -3;
            break;

        case -0x1F:
            mapped = -4;
            break;

        case -0x20:
            mapped = -5;
            break;
        }
    }

    return mapped;
}

s32 ReadCompressedFile(WC24Task* task, void* fd, void* dest, u32 destSize,
                                  u32* outSize) {
    u32 fileSize = VFGetFileSizeByFd(fd);
    u32 uncompSize = 0;
    CXUncompContextLZ ctx;

    CXInitUncompContextLZ(&ctx, dest);

    u32 offset = 0;
    while (offset < fileSize) {
        u32 chunk = fileSize - offset;
        if (chunk > 0x10000)
            chunk = 0x10000;

        s32 error = VFReadFile(fd, gWC24FileBuffer, chunk, NULL);
        if (error != 0) {
            SetTaskMessage(task, "VFReadFile() failed.", 0, error);
            return -7;
        }

        if (offset == 0) {
            if ((gWC24FileBuffer[0] & 0xF0) != 0x10) {
                SetTaskMessage(task, "CXGetCompressionType() data is not LZ.", 0, 0);
                return -5;
            }

            uncompSize = CXGetUncompressedSize(gWC24FileBuffer);
            if (uncompSize > destSize) {
                SetTaskMessage(task, "CXGetUncompressedSize() overflow.", 0, 0);
                return -5;
            }
        }

        CXReadUncompLZ(&ctx, gWC24FileBuffer, chunk);
        offset += 0x10000;
    }

    // Equivalent to !CXIsFinishedUncompLZ(&ctx).
    BOOL notFinished = ctx.destCount > 0 || ctx.headerSize > 0;
    if (notFinished) {
        SetTaskMessage(task, "CXIsFinisiedUncompLZ() is false.", 0, 0);
        return -0xA;
    }

    *outSize = uncompSize;
    return 0;
}
