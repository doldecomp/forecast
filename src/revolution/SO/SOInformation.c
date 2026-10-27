#include <revolution/IPC.h>
#include <revolution/SO.h>

s32 SOGetHostID(void) {
    s32 rmId;
    int result;
    s32 hostId = 0;

    result = SOiPrepare(NULL, &rmId);
    if (result == SO_SUCCESS) {
        hostId = IOS_Ioctl(rmId, SO_IOCTL_GETHOSTID, NULL, 0, NULL, 0);
        SOiConclude(NULL, result);
    }

    return hostId;
}
