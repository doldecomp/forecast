#include <revolution/OS.h>
#include <types.h>

extern "C" OSThread gWC24Thread;
extern "C" volatile u8 gWC24ShutdownFlag1;
extern "C" volatile u8 gWC24ShutdownFlag2;
extern "C" int NWC24iRequestShutdown(u32 event);

extern "C" void ShutdownDownloader(s32 event) {
    int i = 0;
    while (i < 10 && (gWC24ShutdownFlag1 || gWC24ShutdownFlag2)) {
        NWC24iRequestShutdown(event);
        OSSleepTicks(OS_TIME_SPEED);
        i++;
    }

    OSJoinThread(&gWC24Thread, NULL);
}
