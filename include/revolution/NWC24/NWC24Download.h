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

NWC24Err NWC24CheckDlTask(NWC24DlTask* pTask);
NWC24Err NWC24DeleteDlTaskForced(NWC24DlTask* pTask);
NWC24Err NWC24GetDlTask(NWC24DlTask* pTask, u16 id);
NWC24Err NWC24InitDlTask(NWC24DlTask* pTask, u32 type);
NWC24Err NWC24SetDlId(NWC24DlTask* pTask, u16 id);
NWC24Err NWC24GetDlId(NWC24DlTask* pTask, u32* pId);
NWC24Err NWC24SetDlPriority(NWC24DlTask* pTask, s32 priority);
NWC24Err NWC24SetDlInterval(NWC24DlTask* pTask, u16 interval);
NWC24Err NWC24GetDlInterval(NWC24DlTask* pTask, u16* pInterval);
NWC24Err NWC24SetDlRetryMargin(NWC24DlTask* pTask, s32 margin);
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

#ifdef __cplusplus
}
#endif
#endif
