#ifndef CHANNEL_WII_CONNECT24_H
#define CHANNEL_WII_CONNECT24_H
#include <types.h>

// A pair of buffers/timestamps/sizes used by a WiiConnect24 download task.
// Each pair occupies 0x60 bytes; the remaining words are reserved and are
// only ever copied as part of the whole task.
struct WC24BufferSet {
    void* forecast;      // at 0x000
    void* shortForecast; // at 0x004
    u32 unk08[22];       // at 0x008
};

struct WC24TimeSet {
    s64* forecast;      // at 0x000
    s64* shortForecast; // at 0x004
    u32 unk08[22];      // at 0x008
};

struct WC24SizeSet {
    u32* forecast;      // at 0x000
    u32* shortForecast; // at 0x004
    u32 unk08[22];      // at 0x008
};

// A request queued for the WiiConnect24 worker thread (size 0x1B8)
struct WC24Task {
    u32 mType;               // at 0x000
    u16 mTaskId;             // at 0x004
    u16 mSubTaskId;          // at 0x006
    u32 unk008;              // at 0x008
    u32 unk00C;              // at 0x00C
    u32 unk010;              // at 0x010
    WC24BufferSet mBuffers;  // at 0x014
    WC24TimeSet mTimes;      // at 0x074
    WC24SizeSet mSizes;      // at 0x0D4
    s64 mNextDownloadTimes[2];         // at 0x138
    const char* mForecastUrl; // at 0x148
    const char* mShortUrl;    // at 0x14C
    u32 mForecastBufferSize;              // at 0x150
    u32 mBufferSizes[2];     // at 0x154
    u16 mDownloadInterval;              // at 0x15C
    u16 mDownloadCount;              // at 0x15E
    u8 mRedownload;               // at 0x160
    s32 mState;              // at 0x164
    s32 mResult;             // at 0x168
    s32 mErrorCode;          // at 0x16C
    s32 mNwc24Result;        // at 0x170
    char mMessage[0x40];     // at 0x174
};

extern WC24Task gWC24Tasks[];

// WiiConnect24.cpp: weather download tasks
class CWiiConnect24 {
public:
    static s32 getWeather(void* forecastBuf, s64* forecastTime, u32* forecastSize,
                          void* shortBuf, s64* shortTime, u32* shortSize,
                          const char* forecastUrl, const char* shortUrl,
                          u32 forecastBufSize, u32 forecastMaxSize, u32 shortBufSize);
    static s32 downloadWeatherForecast(void* buf, s64* time, u32* size, u32 bufSize);
    static s32 downloadWeatherShort(void* buf, s64* time, u32* size, u32 bufSize);
    static s32 setWeather(const char* forecastUrl, const char* shortUrl, u32 forecastBufSize,
                          u32 redownload, u32 interval);
    static s32 deleteWeather();
};

#endif
