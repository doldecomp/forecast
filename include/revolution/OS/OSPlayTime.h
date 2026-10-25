#ifndef RVL_SDK_OS_PLAY_TIME_H
#define RVL_SDK_OS_PLAY_TIME_H
#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif

void __OSInitPlayTime(void);
BOOL OSPlayTimeIsLimited(void);
s32 __OSGetPlayTime(struct ESTicketView* ticketView, u32* type, u32* remainingTime);
BOOL __OSWriteExpiredFlag(void);

#ifdef __cplusplus
}
#endif
#endif
