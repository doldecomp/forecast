#include <channel/WiiConnect24.h>
#include <types.h>

extern "C" volatile s32 gWC24DownloadIndex;
extern "C" volatile s32 gWC24TaskCount;

extern "C" void UpdateDownloader(void) {
    if (gWC24TaskCount <= 0)
        return;

    if (gWC24Tasks[gWC24DownloadIndex].mState != 0)
        return;

    gWC24DownloadIndex++;
    gWC24TaskCount--;

    if (gWC24DownloadIndex >= gWC24TaskCount)
        gWC24DownloadIndex = 0;
}
