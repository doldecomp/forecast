#include <revolution/OS.h>
#include <revolution/NWC24/NWC24Schedule.h>
#include <revolution/NWC24/internal/NWC24iSystem.h>
#include <revolution/NWC24/internal/NWC24iTime.h>

void __OSInitNet(void) {
    s32 error;
    OSIOSRev rev;

    __OSGetIOSRev(&rev);
    if (rev.idLo <= 4 || rev.idLo == 9) {
        return;
    }

    error = NWC24iPrepareShutdown();
    if (error != 0) {
        if (error < 0) {
            OSReport("Failed to register network shutdown function. %d\n",
                     error);
        }

        error = NWC24SuspendScheduler();
        if (error < 0) {
            OSReport("Failed to suspend the WiiConnect24 scheduler. %d\n",
                     error);
        }
    }

    if (!__OSInIPL) {
        error = NWC24iSynchronizeRtcCounter(FALSE);
        if (error != 0) {
            OSReport(
                "Failed to synchronize time with network resource managers. %d\n",
                error);
        }
    }
}

char sNWC24iPrepareShutdown[] = "NWC24iPrepareShutdown";
char sDevNetKdRequest[] = "/dev/net/kd/request";
char sNWC24SuspendScheduler[] = "NWC24SuspendScheduler";
char sNWC24ResumeScheduler[] = "NWC24ResumeScheduler";
char sNWC24iRequestShutdown[] = "NWC24iRequestShutdown";
char sShutdownGiveUp[] = "NWC24Shutdown_: Give up!\n";
char sNWC24iSetRtcCounter[] = "NWC24iSetRtcCounter_";
char sDevNetKdTime[] = "/dev/net/kd/time";
