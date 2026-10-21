#include <channel/WiiConnect24.h>
#include <revolution/OS.h>
#include <types.h>

extern OSMessageQueue gWC24Queue;
extern volatile s32 gWC24QueueIndex;
extern volatile s32 gWC24TaskCount;

s32 CWiiConnect24::getWeather(void* forecastBuf, s64* forecastTime, u32* forecastSize,
                              void* shortBuf, s64* shortTime, u32* shortSize,
                              const char* forecastUrl, const char* shortUrl,
                              u32 forecastBufSize, u32 forecastMaxSize, u32 shortBufSize) {
    WC24Task task;
    task.mType = 0;
    task.mTaskId = 3;
    task.mSubTaskId = 4;
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
    task.mType = 1;
    task.mTaskId = 3;
    task.mSubTaskId = 0;
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
    task.mType = 1;
    task.mTaskId = 4;
    task.mSubTaskId = 0;
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
    pTask->mType = 2;
    pTask->mTaskId = 3;
    pTask->mSubTaskId = 4;
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
    pTask->mType = 3;
    pTask->mTaskId = 3;
    pTask->mSubTaskId = 4;

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
