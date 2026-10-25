#ifndef RVL_SDK_NWC24_INTERNAL_SCHEDULE_H
#define RVL_SDK_NWC24_INTERNAL_SCHEDULE_H
#include <types.h>

#include <revolution/NWC24/NWC24Types.h>
#ifdef __cplusplus
extern "C" {
#endif

#define NWC24i_SCHEDULER_DEVICE "/dev/net/kd/request"

#define NWC24_SCD_ERROR_LOG_MAX 32

typedef enum NWC24ScdTaskStage {
    NWC24_SCD_TASK_WAITING = 0,
    NWC24_SCD_TASK_ACCOUNT = 1,
    NWC24_SCD_TASK_CHECK = 2,
    NWC24_SCD_TASK_RECEIVE = 3,
    NWC24_SCD_TASK_SEND = 5,
    NWC24_SCD_TASK_SAVE = 6,
    NWC24_SCD_TASK_DOWNLOAD = 7,
    NWC24_SCD_TASK_PROCESS = 8,
} NWC24ScdTaskStage;

typedef struct NWC24ScdStat {
    NWC24Err result;                           // at 0x0
    u32 permission;                            // at 0x4
    s32 lastCriticalError;                     // at 0x8
    u32 newMsgFlag;                            // at 0xC
    NWC24ScdTaskStage taskStage;               // at 0x10
    u32 numErrors;                             // at 0x14
    u32 numMsgSent;                            // at 0x18
    u32 numMsgReceived;                        // at 0x1C
    u32 numMsgSaved;                           // at 0x20
    u32 numMsgRejected;                        // at 0x24
    u32 numMsgFiltered;                        // at 0x28
    u32 countMailChk;                          // at 0x2C
    u32 countMailRcv;                          // at 0x30
    u32 countMailSav;                          // at 0x34
    u32 countMailSnd;                          // at 0x38
    u32 countDL;                               // at 0x3C
    u32 countEstablished;                      // at 0x40
    u32 mailTaskTrace;                         // at 0x44
    u32 dlTaskTrace;                           // at 0x48
    u32 countMailPrc;                          // at 0x4C
    u32 countForceRecv;                        // at 0x50
    u32 countMailIdle;                         // at 0x54
    u32 countScriptExec;                       // at 0x58
    u32 reserved[9];                           // at 0x5C
    s32 errorLog[NWC24_SCD_ERROR_LOG_MAX];     // at 0x80
} NWC24ScdStat;

NWC24Err NWC24iGetSchedulerStat(NWC24ScdStat* pStat, u32 size);
NWC24Err NWC24iRequestGenerateUserId(u64* pId, u32* arg1);
NWC24Err NWC24iTrySuspendForOpenLib(void);
NWC24Err NWC24iResumeForCloseLib(void);
NWC24Err NWC24iSetScriptMode(s32 mode);

#ifdef __cplusplus
}
#endif
#endif
