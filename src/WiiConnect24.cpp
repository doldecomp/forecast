#include <channel/WiiConnect24.h>
#include <channel/WiiConnect24Internal.h>
#include <channel/System.h>
#include <revolution/MEM.h>
#include <revolution/CX.h>
#include <revolution/NAND.h>
#include <revolution/VF/d_vf.h>
#include <revolution/OS.h>
#include <revolution/NWC24/NWC24Download.h>
#include <revolution/NWC24/NWC24Manage.h>
#include <revolution/NWC24/NWC24Schedule.h>
#include <revolution/NWC24/internal/NWC24iManage.h>
#include <revolution/NWC24/internal/NWC24iSystem.h>
#include <revolution/SC/scapi.h>
#include <cstdio>
#include <types.h>

extern NWC24Work gNWC24Work;
extern OSMessageQueue gWC24Queue;
extern OSMessage gWC24QueueBuffer[16];
extern OSThread gWC24Thread;
extern u8 gWC24ThreadStack[0x8000];
extern u8 gWC24FileBuffer[0x10000];
extern WC24Task gWC24Tasks[8];
extern u8 gWC24Spare[0x10];
extern volatile u8 gNWC24LibOpened;
extern volatile u8 gWC24DownloadComplete;
extern volatile u8 gWC24ShutdownFlag1;
extern volatile u8 gWC24NetInitialized;
extern volatile u8 gWC24ShutdownFlag2;
extern u8 gWC24ShutdownPadding[3];
extern volatile s32 gWC24DownloadIndex;
extern volatile s32 gWC24QueueIndex;
extern volatile s32 gWC24TaskCount;
extern void* gWC24HeapBlock;
extern MEMiHeapHead* gWC24Heap;
extern u8 gWC24HeapPadding[4];

void* DownloaderThread(void* unused);

void InitDownloader(void) {
    void* block = MEM2Alloc(0x10000, 0x20);
    gWC24HeapBlock = block;
    gWC24Heap = MEMCreateExpHeapEx(block, 0x10000, 4);

    OSThread* thread = &gWC24Thread;
    OSInitMessageQueue(&gWC24Queue, gWC24QueueBuffer, 0x10);
    OSCreateThread(thread, (OSThreadFunc)DownloaderThread, NULL, gWC24ThreadStack + 0x8000,
                   0x8000, 8, 1);
    OSResumeThread(thread);

    gWC24DownloadIndex = 0;
    gWC24QueueIndex = 0;
    gWC24TaskCount = 0;
}

void UpdateDownloader(void) {
    if (gWC24TaskCount <= 0)
        return;

    if (gWC24Tasks[gWC24DownloadIndex].mState != 0)
        return;

    gWC24DownloadIndex++;
    gWC24TaskCount--;

    if (gWC24DownloadIndex >= gWC24TaskCount)
        gWC24DownloadIndex = 0;
}

void ShutdownDownloader(s32 event) {
    int i = 0;
    while (i < 10 && (gWC24ShutdownFlag1 || gWC24ShutdownFlag2)) {
        NWC24iRequestShutdown(event);
        OSSleepTicks(OS_TIME_SPEED);
        i++;
    }

    OSJoinThread(&gWC24Thread, NULL);
}

s32 CWiiConnect24::getWeather(void* forecastBuf, s64* forecastTime, u32* forecastSize,
                              void* shortBuf, s64* shortTime, u32* shortSize,
                              const char* forecastUrl, const char* shortUrl,
                              u32 forecastBufSize, u32 forecastMaxSize, u32 shortBufSize) {
    WC24Task task;
    task.mType = WC24_TASK_GET_WEATHER;
    task.mTaskId = WC24_TASK_ID_FORECAST;
    task.mSubTaskId = WC24_TASK_ID_SHORT;
    task.mBuffers.forecast = forecastBuf;
    task.mBuffers.shortForecast = shortBuf;
    task.mTimes.forecast = forecastTime;
    task.mTimes.shortForecast = shortTime;
    task.mSizes.forecast = forecastSize;
    task.mSizes.shortForecast = shortSize;
    task.mForecastUrl = forecastUrl;
    task.mShortUrl = shortUrl;
    task.mForecastBufferSize = forecastBufSize;
    task.mBufferSizes[0] = forecastMaxSize;
    task.mBufferSizes[1] = shortBufSize;

    s32 result = -1;
    if (gWC24TaskCount < 8) {
        gWC24Tasks[gWC24QueueIndex] = task;
        gWC24Tasks[gWC24QueueIndex].mState = 1;
        gWC24Tasks[gWC24QueueIndex].mErrorCode = 0;
        gWC24Tasks[gWC24QueueIndex].mMessage[0] = 0;

        if (OSSendMessage(&gWC24Queue, &gWC24Tasks[gWC24QueueIndex], 0)) {
            result = gWC24QueueIndex;
            gWC24QueueIndex++;
            gWC24TaskCount++;

            if (gWC24QueueIndex >= gWC24TaskCount)
                gWC24QueueIndex = 0;
        }
    }

    return result;
}

s32 CWiiConnect24::downloadWeatherForecast(void* buf, s64* time, u32* size, u32 bufSize) {
    WC24Task task;
    task.mType = WC24_TASK_DOWNLOAD_WEATHER;
    task.mTaskId = WC24_TASK_ID_FORECAST;
    task.mSubTaskId = WC24_TASK_ID_NONE;
    task.mBuffers.forecast = buf;
    task.mTimes.forecast = time;
    task.mBufferSizes[0] = bufSize;
    task.mSizes.forecast = size;
    task.mForecastUrl = NULL;

    s32 result = -1;
    if (gWC24TaskCount < 8) {
        gWC24Tasks[gWC24QueueIndex] = task;
        gWC24Tasks[gWC24QueueIndex].mState = 1;
        gWC24Tasks[gWC24QueueIndex].mErrorCode = 0;
        gWC24Tasks[gWC24QueueIndex].mMessage[0] = 0;

        if (OSSendMessage(&gWC24Queue, &gWC24Tasks[gWC24QueueIndex], 0)) {
            result = gWC24QueueIndex;
            gWC24QueueIndex++;
            gWC24TaskCount++;

            if (gWC24QueueIndex >= gWC24TaskCount)
                gWC24QueueIndex = 0;
        }
    }

    return result;
}

s32 CWiiConnect24::downloadWeatherShort(void* buf, s64* time, u32* size, u32 bufSize) {
    WC24Task task;
    task.mType = WC24_TASK_DOWNLOAD_WEATHER;
    task.mTaskId = WC24_TASK_ID_SHORT;
    task.mSubTaskId = WC24_TASK_ID_NONE;
    task.mBuffers.forecast = buf;
    task.mTimes.forecast = time;
    task.mBufferSizes[0] = bufSize;
    task.mSizes.forecast = size;
    task.mForecastUrl = NULL;

    s32 result = -1;
    if (gWC24TaskCount < 8) {
        gWC24Tasks[gWC24QueueIndex] = task;
        gWC24Tasks[gWC24QueueIndex].mState = 1;
        gWC24Tasks[gWC24QueueIndex].mErrorCode = 0;
        gWC24Tasks[gWC24QueueIndex].mMessage[0] = 0;

        if (OSSendMessage(&gWC24Queue, &gWC24Tasks[gWC24QueueIndex], 0)) {
            result = gWC24QueueIndex;
            gWC24QueueIndex++;
            gWC24TaskCount++;

            if (gWC24QueueIndex >= gWC24TaskCount)
                gWC24QueueIndex = 0;
        }
    }

    return result;
}

s32 CWiiConnect24::setWeather(const char* forecastUrl, const char* shortUrl, u32 forecastBufSize,
                              u32 redownload, u32 interval) {
    WC24Task task;
    WC24Task* pTask = &task;
    pTask->mType = WC24_TASK_SET_WEATHER;
    pTask->mTaskId = WC24_TASK_ID_FORECAST;
    pTask->mSubTaskId = WC24_TASK_ID_SHORT;
    pTask->mForecastUrl = forecastUrl;
    pTask->mShortUrl = shortUrl;
    pTask->mForecastBufferSize = forecastBufSize;
    pTask->mRedownload = redownload;
    if (interval == 0)
        pTask->mDownloadInterval = 30;
    else
        pTask->mDownloadInterval = interval;
    pTask->mDownloadCount = 60;

    s32 result = -1;
    if (gWC24TaskCount < 8) {
        gWC24Tasks[gWC24QueueIndex] = *pTask;
        gWC24Tasks[gWC24QueueIndex].mState = 1;
        gWC24Tasks[gWC24QueueIndex].mErrorCode = 0;
        gWC24Tasks[gWC24QueueIndex].mMessage[0] = 0;

        if (OSSendMessage(&gWC24Queue, &gWC24Tasks[gWC24QueueIndex], 0)) {
            result = gWC24QueueIndex;
            gWC24QueueIndex++;
            gWC24TaskCount++;

            if (gWC24QueueIndex >= gWC24TaskCount)
                gWC24QueueIndex = 0;
        }
    }

    return result;
}

s32 CWiiConnect24::deleteWeather() {
    WC24Task task;
    WC24Task* pTask = &task;
    pTask->mType = WC24_TASK_DELETE_WEATHER;
    pTask->mTaskId = WC24_TASK_ID_FORECAST;
    pTask->mSubTaskId = WC24_TASK_ID_SHORT;

    s32 result = -1;
    if (gWC24TaskCount < 8) {
        gWC24Tasks[gWC24QueueIndex] = *pTask;
        gWC24Tasks[gWC24QueueIndex].mState = 1;
        gWC24Tasks[gWC24QueueIndex].mErrorCode = 0;
        gWC24Tasks[gWC24QueueIndex].mMessage[0] = 0;

        if (OSSendMessage(&gWC24Queue, &gWC24Tasks[gWC24QueueIndex], 0)) {
            result = gWC24QueueIndex;
            gWC24QueueIndex++;
            gWC24TaskCount++;

            if (gWC24QueueIndex >= gWC24TaskCount)
                gWC24QueueIndex = 0;
        }
    }

    return result;
}

char sVfDriveName[] = "@24";
char sVfRootPath[] = "@24:/*";
char sVfReadMode[] = "r";
char sBinNameFormat[] = "%d.bin";
char sForecastFileSuffix[] = "%sB";
char sShortFileSuffix[] = "%sR";

NWC24Err MapDownloadError(NWC24Err error);
NWC24Err DownloadFiles(WC24Task* task, u32 doForecast, u32 doShort);
NWC24Err CreateDownloadTasks(WC24Task* task, u32 doForecast, u32 doShort, u8 redownload,
                                   u16 newInterval, u16 count, u32* idOut);
NWC24Err ExecuteDownloadTask(WC24Task* task, s32 index, u32 unused, u16 unused2);
NWC24Err DeleteTaskFiles(WC24Task* task, u32 doForecast, u32 doShort);
NWC24Err ReadCompressedFile(WC24Task* task, void* fd, void* dest, u32 destSize,
                                  u32* outSize);

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
        return SetTaskError(task, "NWC24OpenLib() failed.", error);

    gNWC24LibOpened = 1;
    return NWC24_OK;
}

static inline NWC24Err CheckLib(WC24Task* task) {
    NWC24Err error = NWC24Check(2);

    if (error != NWC24_OK)
        return SetTaskError(task, "NWC24Check() failed.", error);

    SCIdleModeInfo idle;
    if (SCGetIdleMode(&idle) == 0 || idle.wc24 != 1)
        return NWC24_ERR_INVALID_VALUE;

    return NWC24_OK;
}

static inline NWC24Err CloseLib(WC24Task* task) {
    NWC24Err error = NWC24CloseLib();

    if (error != NWC24_OK)
        return SetTaskError(task, "NWC24CloseLib() failed.", error);

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

NWC24Err DownloadFiles(WC24Task* task, u32 doForecast, u32 doShort) {
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

    NWC24Err error;
    s32 vfError;
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
        if (error == NWC24_ERR_NOT_FOUND) {
            SetTaskMessage(task, "NWC24GetDlTask() failed.", NWC24GetErrorCode(), error);
            return NWC24_ERR_FATAL;
        }

        if (error != NWC24_OK) {
            return SetTaskError(task, "NWC24GetDlTask() failed.", error);
        }

        error = NWC24GetDlUrl(&urlTask[i], url, 0xFF);
        if (error != NWC24_OK) {
            return SetTaskError(task, "NWC24GetDlUrl() failed.", error);
        }

        if ((&task->mForecastUrl)[i] != NULL) {
            u32 j = 0;
            while (j < 0xFF && url[j] != 0) {
                if (url[j] != (&task->mForecastUrl)[i][j]) {
                    SetTaskMessage(task, "URL is not same.", 0, 0);
                    return NWC24_ERR_FATAL;
                }

                j++;
            }
        }
    }

    for (s32 i = 0; i < 2; i++) {
        if (buffers[i] == NULL)
            continue;

        error = NWC24GetDlVfName(&dlTasks[i], vfName, 0x50);
        if (error != NWC24_OK) {
            return SetTaskError(task, "NWC24GetDlVfName() failed.", error);
        }

        vfError = VFMountDriveNANDFlashEx(sVfDriveName, vfName);
        if (vfError != 0) {
            SetTaskMessage(task, "VFMountDriveNANDFlash() failed.", 0, vfError);
            return NWC24_ERR_FATAL;
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
        if (error != NWC24_OK) {
            return SetTaskError(task, "NWC24GetDlFilename() failed.", error);
        }

        void* file = VFOpenFile(fileName, sVfReadMode, 0);
        if (file == NULL) {
            SetTaskMessage(task, "VFOpenFile() failed.", NWC24GetErrorCode(), 0);

            vfError = VFUnmountDrive(sVfDriveName);
            if (vfError != 0) {
                SetTaskMessage(task, "VFUnmountDrive() failed.", 0, vfError);
                return NWC24_ERR_PROTECTED;
            }
            return NWC24_ERR_FATAL;
        }

        NWC24Err result = ReadCompressedFile(task, file, buffers[i], task->mBufferSizes[i], &outSize);
        if (result != NWC24_OK)
            return result;

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

        vfError = VFCloseFile(file);
        if (vfError != 0) {
            SetTaskMessage(task, "VFCloseFile() failed.", 0, vfError);
            VFUnmountDrive(sVfDriveName);
            return NWC24_ERR_PROTECTED;
        }

        error = NWC24GetDlLastUpdate(&dlTasks[i], times[i]);
        if (error != NWC24_OK) {
            return SetTaskError(task, "NWC24GetDlLastUpdate() failed.", error);
        }

        error = NWC24GetDlNextTime(&pTask[i], &task->mNextDownloadTimes[i]);
        if (error != NWC24_OK) {
            return SetTaskError(task, "NWC24GetDlNextTime() failed.", error);
        }
    }

    vfError = VFUnmountDrive(sVfDriveName);
    if (vfError != 0) {
        SetTaskMessage(task, "VFUnmoundDrive() failed.", 0, vfError);
        return NWC24_ERR_PROTECTED;
    }

    return NWC24_OK;
}

NWC24Err CreateDownloadTasks(WC24Task* task, u32 doForecast, u32 doShort, u8 redownload,
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

    NWC24Err error;
    s32 vfError;
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
        if (error == NWC24_ERR_NOT_FOUND) {
            redo = TRUE;
            init = TRUE;
            goto firstDone;
        }

        if (error != NWC24_OK)
            return SetTaskError(task, "NWC24GetDlTask() failed.", error);

        error = NWC24GetDlVfName(&pTask[i], vfName, 0x50);
        if (error != NWC24_OK)
            return SetTaskError(task, "NWC24GetDlVfName() failed.", error);

        if (!mounted) {
            vfError = VFMountDriveNANDFlashEx(sVfDriveName, vfName);
            if (vfError == 2 || vfError == 0xB001) {
                redo = TRUE;
                init = TRUE;
                goto firstDone;
            }

            if (vfError != 0) {
                SetTaskMessage(task, "VFMountDriveNANDFlash() failed.", 0, vfError);
                return NWC24_ERR_PROTECTED;
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
            if (error != NWC24_OK)
                return SetTaskError(task, "NWC24GetDlFilename() failed.", error);

            void* file = VFOpenFile(fileName, sVfReadMode, 0);
            if (file == NULL) {
                redo = TRUE;
                init = TRUE;
                continue;
            }

            vfError = VFCloseFile(file);
            if (vfError != 0) {
                SetTaskMessage(task, "VFCloseFile() failed.", 0, vfError);
                VFUnmountDrive(sVfDriveName);
                return NWC24_ERR_PROTECTED;
            }
            break;
        }
        }
    }

firstDone:
    if (mounted) {
        vfError = VFUnmountDrive(sVfDriveName);
        if (vfError != 0) {
            SetTaskMessage(task, "VFUnmountDrive() failed.", 0, vfError);
            return NWC24_ERR_PROTECTED;
        }
    }

    if (!redo) {
        for (s32 i = 0; i < 2; i++) {
            if (ids[i] == 0)
                continue;

            error = NWC24GetDlUrl(&dlTasks[i], url, 0xFF);
            if (error != NWC24_OK)
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
            if (error != NWC24_OK)
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
                if (error != NWC24_OK)
                    return SetTaskError(task, "NWC24InitDlTask() failed.", error);

                error = NWC24SetDlId(&dlTasks[i], ids[i]);
                if (error != NWC24_OK)
                    return SetTaskError(task, "NWC24SetDlId() failed.", error);

                error = NWC24GetDlVfName(&pTask2[i], vfName2, 0x50);
                if (error != NWC24_OK)
                    return SetTaskError(task, "NWC24GetDlVfName() failed.", error);

                if (!created) {
                    s32 nandResult = NANDDelete(vfName2);
                    if (nandResult != NAND_RESULT_NOEXISTS && nandResult != NAND_RESULT_OK) {
                        SetTaskMessage(task, "NANDDelete() failed.", 0, nandResult);
                        return NWC24_ERR_OVERFLOW;
                    }

                    error = NWC24CreateDlVf(&pTask2[i], task->mForecastBufferSize);
                    if (error != NWC24_OK)
                        return SetTaskError(task, "NWC24CreateDlVf() failed.", error);

                    created = TRUE;
                }
            }

            error = NWC24SetDlUrl(&dlTasks[i], urls[i]);
            if (error != NWC24_OK)
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
            if (error != NWC24_OK)
                return SetTaskError(task, "NWC24SetDlPriority() failed.", error);

            error = NWC24SetDlFlags(&dlTasks[i], 0x40000000);
            if (error != NWC24_OK)
                return SetTaskError(task, "NWC24SetDlFlags() failed.", error);

            error = NWC24SetDlInterval(&dlTasks[i], newInterval);
            if (error != NWC24_OK)
                return SetTaskError(task, "NWC24SetDlInterval() failed.", error);

            error = NWC24SetDlRetryMargin(&dlTasks[i], margin);
            if (error != NWC24_OK)
                return SetTaskError(task, "NWC24SetDlRetryMargin() failed.", error);

            sprintf(nameBuf, sBinNameFormat, ids[i]);
            error = NWC24SetDlFilename(&dlTasks[i], nameBuf);
            if (error != NWC24_OK)
                return SetTaskError(task, "NWC24SetDlFilename() failed.", error);
        }
    }

    NWC24DlTask* pTask3 = dlTasks;
    for (s32 i = 0; i < 2; i++) {
        if (ids[i] == 0)
            continue;

        error = NWC24SetDlCount(&dlTasks[i], (s16)count);
        if (error != NWC24_OK)
            return SetTaskError(task, "NWC24SetDlCount() failed.", error);

        if (redo) {
            error = NWC24AddDlTask(&dlTasks[i]);
            if (error != NWC24_OK)
                return SetTaskError(task, "NWC24AddDlTask() failed.", error);
        } else {
            error = NWC24UpdateDlTask(&dlTasks[i]);
            if (error != NWC24_OK)
                return SetTaskError(task, "NWC24UpdateDlTask() failed.", error);
        }

        if (idOut != NULL) {
            error = NWC24GetDlId(&pTask3[i], idOut);
            if (error != NWC24_OK)
                return SetTaskError(task, "NWC24GetDlId() failed.", error);
        }

        NWC24iCheckDlTask(&pTask3[i]);
    }

    gWC24DownloadComplete = 1;
    return NWC24_OK;
}

struct WC24SOConfig {
    void* alloc;
    void* free;
};

NWC24Err ExecuteDownloadTask(WC24Task* task, s32 index, u32 unused, u16 unused2) {
    s32 initResult;
    s32 startupResult;
    NWC24Err error;
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
            return NWC24_ERR_LIB_NOT_OPENED;
        }

        startupResult = SOStartup();
        if (startupResult < 0) {
            SetTaskMessage(task, "SOStartup() failed.", SOGetLastError(), startupResult);
            gWC24ShutdownFlag2 = 0;
            return NWC24_ERR_LIB_NOT_OPENED;
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

        if (error != NWC24_OK) {
            SetTaskMessage(task, "NWC24ExecDownloadTask() failed.", NWC24GetErrorCode(), error);
            return MapDownloadError(error);
        }
        break;
    }
    return NWC24_OK;
}

NWC24Err DeleteTaskFiles(WC24Task* task, u32 doForecast, u32 doShort) {
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

        NWC24Err error = NWC24GetDlTask(&dlTask, ids[i]);
        if (error == NWC24_ERR_NOT_FOUND)
            continue;

        if (error != NWC24_OK) {
            return SetTaskError(task, "NWC24GetDl*() failed.", error);
        }

        if (!found) {
            found = TRUE;
            error = NWC24GetDlVfName(&dlTask, vfName, 0x50);
            if (error != NWC24_OK) {
                return SetTaskError(task, "NWC24GetDlVfName() failed.", error);
            }
        }

        error = NWC24DeleteDlTask(&dlTask);
        if (error != NWC24_OK) {
            return SetTaskError(task, "NWC24DeleteDlTask() failed.", error);
        }
    }

    if (found) {
        s32 nandResult = NANDDelete(vfName);
        if (nandResult != NAND_RESULT_NOEXISTS && nandResult != NAND_RESULT_OK) {
            SetTaskMessage(task, "NANDDelete() failed.", 0, nandResult);
            return NWC24_ERR_OVERFLOW;
        }
    }

    return NWC24_OK;
}

NWC24Err MapDownloadError(NWC24Err error) {
    NWC24Err mapped = NWC24_ERR_FULL;

    if ((error <= NWC24_ERR_FILE_OPEN && error >= NWC24_ERR_FILE_OTHER) ||
        error == NWC24_ERR_NAND_CORRUPT || error == NWC24_ERR_FILE_EXISTS ||
        error == NWC24_ERR_INTERNAL_VF || error == NWC24_ERR_FILE_BROKEN) {
        mapped = NWC24_ERR_NOMEM;
    } else if (error == NWC24_ERR_OLD_SYSTEM) {
        mapped = NWC24_ERR_CONFIG;
    } else if (error == NWC24_ERR_FATAL || error == NWC24_ERR_NOMEM ||
               error == NWC24_ERR_INTERNAL_IPC) {
        mapped = NWC24_ERR_FAILED;
    } else if ((u32)(NWC24GetErrorCode() + 0x20000) == 0x55CE) {
        mapped = NWC24_ERR_NULL;
    } else {
        switch (error) {
        case NWC24_ERR_DISABLED:
            mapped = NWC24_ERR_INVALID_VALUE;
            break;

        case NWC24_ERR_NETWORK:
            mapped = NWC24_ERR_NOT_SUPPORTED;
            break;

        case NWC24_ERR_SERVER:
            mapped = NWC24_ERR_NULL;
            break;
        }
    }

    return mapped;
}

NWC24Err ReadCompressedFile(WC24Task* task, void* fd, void* dest, u32 destSize,
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

        s32 vfError = VFReadFile(fd, gWC24FileBuffer, chunk, NULL);
        if (vfError != 0) {
            SetTaskMessage(task, "VFReadFile() failed.", 0, vfError);
            return NWC24_ERR_PROTECTED;
        }

        if (offset == 0) {
            if ((gWC24FileBuffer[0] & 0xF0) != 0x10) {
                SetTaskMessage(task, "CXGetCompressionType() data is not LZ.", 0, 0);
                return NWC24_ERR_NULL;
            }

            uncompSize = CXGetUncompressedSize(gWC24FileBuffer);
            if (uncompSize > destSize) {
                SetTaskMessage(task, "CXGetUncompressedSize() overflow.", 0, 0);
                return NWC24_ERR_NULL;
            }
        }

        CXReadUncompLZ(&ctx, gWC24FileBuffer, chunk);
        offset += 0x10000;
    }

    // Equivalent to !CXIsFinishedUncompLZ(&ctx).
    BOOL notFinished = ctx.destCount > 0 || ctx.headerSize > 0;
    if (notFinished) {
        SetTaskMessage(task, "CXIsFinisiedUncompLZ() is false.", 0, 0);
        return NWC24_ERR_LIB_OPENED;
    }

    *outSize = uncompSize;
    return NWC24_OK;
}

void* WC24Alloc(u32 unused, s32 size) {
    void* block = NULL;

    if (size > 0)
        block = MEMAllocFromExpHeapEx(gWC24Heap, size, 0x20);

    return block;
}

void WC24Free(void* unused, void* block, s32 size) {
    if (block == NULL)
        return;

    if (size <= 0)
        return;

    MEMFreeToExpHeap(gWC24Heap, block);
}

NWC24Work gNWC24Work;
OSMessageQueue gWC24Queue;
OSMessage gWC24QueueBuffer[16];
OSThread gWC24Thread;
u8 gWC24ThreadStack[0x8000];
u8 gWC24FileBuffer[0x10000];
WC24Task gWC24Tasks[8];
u8 gWC24Spare[0x10];

volatile u8 gNWC24LibOpened;
volatile u8 gWC24DownloadComplete;
volatile u8 gWC24ShutdownFlag1;
volatile u8 gWC24NetInitialized;
volatile u8 gWC24ShutdownFlag2;
u8 gWC24ShutdownPadding[3];
volatile s32 gWC24DownloadIndex;
volatile s32 gWC24QueueIndex;
volatile s32 gWC24TaskCount;
void* gWC24HeapBlock;
MEMiHeapHead* gWC24Heap;
u8 gWC24HeapPadding[4];
