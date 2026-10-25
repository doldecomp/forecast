#ifndef RVL_SDK_NWC24_DOWNLOAD_H
#define RVL_SDK_NWC24_DOWNLOAD_H
#include <types.h>

#include <revolution/FS.h>
#include <revolution/NWC24/NWC24Types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NWC24i_DL_TASK_MAX 120
#define NWC24i_DL_SUBTASK_MAX 32

typedef u16 NWC24DlId;

typedef enum {
    NWC24_DLTYPE_MULTIPART_V1,
    NWC24_DLTYPE_OCTETSTREAM_V1,
    NWC24_DLTYPE_MULTIPART_V2,
    NWC24_DLTYPE_OCTETSTREAM_V2
} NWC24DlType;

typedef enum {
    NWC24_DL_STTYPE_NONE,
    NWC24_DL_STTYPE_INCREMENT,
    NWC24_DL_STTYPE_TIME_HOUR,
    NWC24_DL_STTYPE_TIME_DAYOFWEEK,
    NWC24_DL_STTYPE_TIME_DAY
} NWC24DlSubTaskType;

typedef enum {
    NWC24_DL_STFLAG_TRAILING_FILENAME = (1 << 0),
    NWC24_DL_STFLAG_TRAILING_URL = (1 << 1),
    NWC24_DL_STFLAG_INTELLIGENT_UPDATE = (1 << 8),
    NWC24_DL_STFLAG_RETICENT_UPDATE = (1 << 9)
} NWC24DlSubTaskFlags;

#define NWC24_DL_TASK_PUBLIC_SIZE 0x200
typedef struct NWC24DlTask {
    u8 data[NWC24_DL_TASK_PUBLIC_SIZE];
} NWC24DlTask;

// Sort key for NWC24IterateDlTaskEx (low halfword of mode)
typedef enum {
    NWC24_DL_ITER_LAST_ACCESS,
    NWC24_DL_ITER_NEXT_TIME,
    NWC24_DL_ITER_PRIORITY
} NWC24DlIterateKey;

// Set in mode to iterate in descending order
#define NWC24_DL_ITER_DESCENDING 0x80000000

typedef struct NWC24DlIterateWork {
    u32 mode;      // at 0x0
    s32 value;     // at 0x4
    s32 lastValue; // at 0x8
    s32 lastId;    // at 0xC
    BOOL begin;    // at 0x10
    BOOL active;   // at 0x14
} NWC24DlIterateWork;

NWC24Err NWC24GetDlTask(NWC24DlTask* pTask, NWC24DlId id);
NWC24Err NWC24InitDlTask(NWC24DlTask* pTask, NWC24DlType type);
NWC24Err NWC24SetDlId(NWC24DlTask* pTask, NWC24DlId id);
NWC24Err NWC24GetDlId(NWC24DlTask* pTask, NWC24DlId* pId);
NWC24Err NWC24SetDlPriority(NWC24DlTask* pTask, u8 priority);
NWC24Err NWC24SetDlInterval(NWC24DlTask* pTask, u16 interval);
NWC24Err NWC24GetDlInterval(NWC24DlTask* pTask, u16* pInterval);
NWC24Err NWC24SetDlRetryMargin(NWC24DlTask* pTask, u16 margin);
NWC24Err NWC24SetDlUrl(NWC24DlTask* pTask, const char* url);
NWC24Err NWC24GetDlUrl(NWC24DlTask* pTask, char* url, u32 len);
NWC24Err NWC24SetDlFlags(NWC24DlTask* pTask, u32 flags);
NWC24Err NWC24SetDlFilename(NWC24DlTask* pTask, const char* name);
NWC24Err NWC24GetDlFilename(NWC24DlTask* pTask, char* name, u32 len);
NWC24Err NWC24SetDlCount(NWC24DlTask* pTask, s16 count);
NWC24Err NWC24GetDlLastUpdate(NWC24DlTask* pTask, s64* time);
NWC24Err NWC24UpdateDlTask(NWC24DlTask* pTask);
NWC24Err NWC24DeleteDlTask(NWC24DlTask* pTask);
NWC24Err NWC24AddDlTask(NWC24DlTask* pTask);
NWC24Err NWC24GetDlNextTime(NWC24DlTask* pTask, s64* time);
NWC24Err NWC24GetDlVfName(NWC24DlTask* pTask, char* name, u32 len);
NWC24Err NWC24CreateDlVf(NWC24DlTask* pTask, u32 size);
NWC24Err NWC24IterateDlTask(NWC24DlId* pId, BOOL begin);
NWC24Err NWC24IterateDlTaskEx(NWC24DlIterateWork* pWork, NWC24DlId* pId);
NWC24Err NWC24PurgeOldestDlTask(void);

#ifdef __cplusplus
}
#endif
#endif
