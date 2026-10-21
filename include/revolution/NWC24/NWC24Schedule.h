#ifndef RVL_SDK_NWC24_SCHEDULE_H
#define RVL_SDK_NWC24_SCHEDULE_H
#include <types.h>

#include <revolution/NWC24/NWC24Types.h>

#ifdef __cplusplus
extern "C" {
#endif

s32 NWC24SuspendScheduler(void);
s32 NWC24ResumeScheduler(void);
NWC24Err NWC24ExecDownloadTask(s32 mode, u16 taskId, s32 flags);

#ifdef __cplusplus
}
#endif
#endif
