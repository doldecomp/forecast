#include <revolution/NWC24.h>
#include <revolution/NWC24/NWC24Internal.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

// Task flags checked by this file
#define DL_FLAG_SECURE_ONLY 0x00000004 // URL must not be plain http://
#define DL_FLAG_GROUP_SHARE 0x00000040 // Writable by apps of the same group
#define DL_FLAG_NO_LIMITS 0x40000000   // Skips the interval/count limits
#define DL_FLAG_V2_ONLY 0x80000038     // Not allowed for V1 task types

#define DL_INTERVAL_DEFAULT (2 * 24 * 60) // minutes
#define DL_INTERVAL_MIN (3 * 60)
#define DL_INTERVAL_MAX (7 * 24 * 60)
#define DL_MARGIN_DEFAULT (24 * 60)
#define DL_MARGIN_MAX (14 * 24 * 60)
#define DL_COUNT_MAX 100
#define DL_PRIORITY_DEFAULT 0x7F

#define DL_VF_SIZE_MIN 0x2800
#define DL_VF_FILENAME "wc24dl.vff"

#define DL_ID_INVALID 0xFFFF

static const char* DLFilePath = "/shared2/wc24/nwc24dl.bin";

static BOOL sDlRestrictionDisabled;
static char sDlVfPath[128];

static NWC24Err GetDlUrl(NWC24iDlTask* pTask, char* pUrl, u32 len,
                         BOOL subTask, u8 index) DECOMP_DONT_INLINE;
static NWC24Err WriteDlTask(NWC24iDlTask* pTask) DECOMP_DONT_INLINE;
static NWC24Err AddDlTask(NWC24iDlTask* pTask, u16 min,
                          u16 max) DECOMP_DONT_INLINE;
static NWC24Err DeleteDlTask(NWC24iDlTask* pTask) DECOMP_DONT_INLINE;
static s32 GetDlLastAccess(u16 id);
static s32 GetDlNextTime(u16 id);
static s32 GetDlPriority(u16 id);

static inline NWC24iDlHeader* GetDlHeader(void) {
    if (NWC24WorkP != NULL) {
        return (NWC24iDlHeader*)NWC24WorkP->dlHeader;
    }
    return NULL;
}

// Same as GetDlHeader, written the other way round. The two loops that test the
// header in their condition use this form: MWCC lays out the branches of the
// two spellings differently there.
static inline NWC24iDlHeader* GetLoopDlHeader(void) {
    return NWC24WorkP == NULL ? NULL : (NWC24iDlHeader*)NWC24WorkP->dlHeader;
}

static inline NWC24iDlEntry* GetDlEntry(u16 id) {
    return &GetDlHeader()->entries[id];
}

static inline BOOL IsOwnTask(const NWC24iDlTask* pTask) {
    u32 appId = pTask->appId;
    return (NWC24GetAppId() & 0xFFFFFF00) == (appId & 0xFFFFFF00);
}

static inline BOOL IsGroupTask(const NWC24iDlTask* pTask) {
    u32 groupId = pTask->groupId;
    BOOL shared = FALSE;

    if ((pTask->flags & DL_FLAG_GROUP_SHARE) && groupId == NWC24GetGroupId()) {
        shared = TRUE;
    }

    return shared;
}

static inline BOOL HasFilename(NWC24DlType type) {
    if (type == NWC24_DLTYPE_OCTETSTREAM_V1 ||
        type == NWC24_DLTYPE_OCTETSTREAM_V2) {
        return TRUE;
    }

    return FALSE;
}

static inline NWC24Err CheckDlTask(const NWC24iDlTask* pTask) {
    NWC24iDlHeader* pHeader = GetDlHeader();

    if (pTask == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }

    if (pHeader == NULL) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }

    if (pTask->id != DL_ID_INVALID && pTask->id >= pHeader->maxTasks) {
        return NWC24_ERR_INVALID_VALUE;
    }

    return NWC24_OK;
}

static inline NWC24Err CheckDlTaskWritable(const NWC24iDlTask* pTask) {
    NWC24iDlHeader* pHeader = GetDlHeader();

    if (pTask == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }

    if (pHeader == NULL) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }

    if (!NWC24IsMsgLibOpenedByTool() && !IsOwnTask(pTask)) {
        if (!IsGroupTask(pTask)) {
            return NWC24_ERR_PROTECTED;
        }
    }

    if (pTask->id != DL_ID_INVALID && pTask->id >= pHeader->maxTasks) {
        return NWC24_ERR_INVALID_VALUE;
    }

    return NWC24_OK;
}

// The same check as CheckDlTaskWritable with the ownership test written out
// (NWC24InitDlTask uses this one). Inlined helpers' locals are created before
// the caller's own, so through IsOwnTask/IsGroupTask appId/groupId would take
// their registers before pHeader; written out, pHeader comes first as in the
// original.
static inline NWC24Err CheckNewDlTask(const NWC24iDlTask* pTask) {
    NWC24iDlHeader* pHeader = GetDlHeader();

    if (pTask == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }

    if (pHeader == NULL) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }

    if (!NWC24IsMsgLibOpenedByTool()) {
        u32 appId = pTask->appId;
        BOOL own = (NWC24GetAppId() & 0xFFFFFF00) == (appId & 0xFFFFFF00);

        if (!own) {
            u32 groupId = pTask->groupId;
            BOOL shared = FALSE;

            if ((pTask->flags & DL_FLAG_GROUP_SHARE) &&
                groupId == NWC24GetGroupId()) {
                shared = TRUE;
            }

            if (!shared) {
                return NWC24_ERR_PROTECTED;
            }
        }
    }

    if (pTask->id != DL_ID_INVALID && pTask->id >= pHeader->maxTasks) {
        return NWC24_ERR_INVALID_VALUE;
    }

    return NWC24_OK;
}

static inline NWC24Err CheckDlEntryAvailable(u16 id) {
    if (id >= GetDlHeader()->maxTasks || id == DL_ID_INVALID) {
        return NWC24_ERR_INVALID_VALUE;
    }

    if (GetDlEntry(id)->app == 0) {
        return NWC24_ERR_NOT_FOUND;
    }

    return NWC24_OK;
}

static inline NWC24Err CheckDlUrl(const char* pUrl) {
    NWC24Err result = NWC24iCheckStringLength(pUrl, 7, 256);

    if (result < 0) {
        return result;
    }

    if (strncmp(pUrl, "http://", 7) != 0 &&
        strncmp(pUrl, "https://", 8) != 0) {
        return NWC24_ERR_FORMAT;
    }

    return NWC24_OK;
}

static inline const char* GetDlTaskUrl(const NWC24iDlTask* pTask) {
    return pTask->url;
}

static inline NWC24Err CheckDlTaskParam(NWC24iDlTask* pTask) {
    const char* pUrl;
    NWC24Err result;

    if (pTask->id >= GetDlHeader()->maxTasks && pTask->id != DL_ID_INVALID) {
        return NWC24_ERR_INVALID_VALUE;
    }

    pUrl = GetDlTaskUrl(pTask);
    result = CheckDlUrl(pUrl);
    if (result < 0) {
        return result;
    }

    return NWC24_OK;
}

static inline NWC24Err CheckLoopDlTask(const NWC24iDlTask* pTask) {
    NWC24iDlHeader* pHeader = GetLoopDlHeader();

    if (pTask == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }

    if (pHeader == NULL) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }

    if (pTask->id != DL_ID_INVALID && pTask->id >= pHeader->maxTasks) {
        return NWC24_ERR_INVALID_VALUE;
    }

    return NWC24_OK;
}

static inline NWC24Err CheckSubTask(const NWC24iDlTask* pTask, u8 index) {
    NWC24Err result = CheckLoopDlTask(pTask);
    if (result != NWC24_OK) {
        return result;
    }

    if (pTask->subTaskType == NWC24_DL_STTYPE_NONE) {
        return NWC24_ERR_INVALID_OPERATION;
    }

    if (pTask->subTaskMask == 0) {
        return NWC24_ERR_FATAL;
    }

    if (index > NWC24i_DL_SUBTASK_MAX - 1) {
        return NWC24_ERR_INVALID_VALUE;
    }

    result = NWC24_OK;
    if (!((1 << index) & pTask->subTaskMask)) {
        result = NWC24_ERR_DISABLED;
    }

    return result;
}

static inline NWC24Err SetEntryNextTime(NWC24iDlTask* pTask, s64 time) {
    NWC24Err result;
    u16 id;

    result = CheckDlTask(pTask);
    if (result != NWC24_OK) {
        return result;
    }

    id = (u16)pTask->id;
    if (id == DL_ID_INVALID) {
        return NWC24_ERR_FAILED;
    }

    GetDlEntry(id)->nextTime = time / 60;
    return NWC24_OK;
}

static inline NWC24Err UpdateNextTime(NWC24iDlTask* pTask) {
    s64 now = 0;
    NWC24Err result = NWC24iGetUniversalTime(&now);
    if (result < 0) {
        return result;
    }

    return SetEntryNextTime(pTask, now + pTask->interval * 60);
}

static inline NWC24Err SetDlLastAccess(NWC24iDlTask* pTask, s64 time) {
    NWC24Err result;
    u16 id;

    result = CheckDlTask(pTask);
    if (result != NWC24_OK) {
        return result;
    }

    id = (u16)pTask->id;
    if (id == DL_ID_INVALID) {
        return NWC24_ERR_FAILED;
    }

    GetDlEntry(id)->lastAccess = time / 60;
    return NWC24_OK;
}

static inline NWC24Err UpdateLastAccess(NWC24iDlTask* pTask) {
    s64 now;
    NWC24Err result = NWC24iGetUniversalTime(&now);
    if (result >= 0) {
        result = SetDlLastAccess(pTask, now);
        if (result < 0) {
            return result;
        }
    }

    return result;
}

static inline void ClearDlError(NWC24iDlTask* pTask) {
    if (CheckDlTaskWritable(pTask) == NWC24_OK) {
        pTask->lastError = 0;
        pTask->errorCount = 0;
    }
}

static inline NWC24Err SeekDlTaskEntry(u16 id, NWC24File* pFile) {
    NWC24iDlHeader* pHeader = GetDlHeader();

    if (pHeader->maxTasks > NWC24i_DL_TASK_MAX || id >= pHeader->maxTasks) {
        return NWC24_ERR_INVALID_VALUE;
    }

    return NWC24FSeek(pFile, sizeof(NWC24iDlHeader) + id * sizeof(NWC24iDlTask),
                      NWC24_SEEK_BEG);
}

static inline NWC24Err ReadDlTaskEntry(NWC24iDlTask* pTask, u16 id,
                                NWC24File* pFile) {
    NWC24Err result = SeekDlTaskEntry(id, pFile);
    if (result < 0) {
        return result;
    }

    result = NWC24FRead(pTask, sizeof(NWC24iDlTask), pFile);
    if (result < 0) {
        return result;
    }

    return NWC24_OK;
}

static inline NWC24Err WriteDlTaskEntry(NWC24iDlTask* pTask, NWC24File* pFile) {
    NWC24Err result = SeekDlTaskEntry(pTask->id, pFile);
    if (result < 0) {
        return result;
    }

    memcpy(NWC24WorkP->dlTask, pTask, sizeof(NWC24iDlTask));
    result = NWC24FWrite(NWC24WorkP->dlTask, sizeof(NWC24iDlTask), pFile);
    if (result < 0) {
        return result;
    }

    return NWC24_OK;
}

static inline NWC24Err WriteDlHeader(NWC24File* pFile) {
    NWC24Err result = NWC24FSeek(pFile, 0, NWC24_SEEK_BEG);
    if (result < 0) {
        return result;
    }

    result = NWC24FWrite(GetDlHeader(), sizeof(NWC24iDlHeader), pFile);
    if (result < 0) {
        return result;
    }

    return NWC24_OK;
}

static inline NWC24Err ReadDlHeader(NWC24File* pFile) {
    NWC24Err result = NWC24FSeek(pFile, 0, NWC24_SEEK_BEG);
    if (result < 0) {
        return result;
    }

    result = NWC24FRead(NWC24WorkP->dlHeader, sizeof(NWC24iDlHeader), pFile);
    if (result < 0) {
        return result;
    }

    return NWC24_OK;
}

static inline NWC24Err UpdateDlEntry(NWC24iDlTask* pTask) {
    NWC24iDlEntry* pEntry = GetDlEntry(pTask->id);

    pEntry->app = pTask->appId;
    pEntry->priority = pTask->priority;
    return NWC24_OK;
}

static inline void InitDlEntry(u16 id) {
    memset(&((NWC24iDlHeader*)NWC24WorkP->dlHeader)->entries[id], 0,
           sizeof(NWC24iDlEntry));
}

static inline NWC24Err ClearDlTaskEntry(u16 id, NWC24File* pFile) {
    NWC24iDlTask* pTask = (NWC24iDlTask*)NWC24WorkP->dlTask;

    memset(pTask, 0, sizeof(NWC24iDlTask));
    pTask->type = 0xFF;
    pTask->id = id;

    InitDlEntry(id);
    return WriteDlTaskEntry(pTask, pFile);
}

static inline NWC24Err LoadDlTask(NWC24iDlTask* pTask, u16 id) {
    NWC24File file;
    NWC24Err result;
    NWC24Err close;

    result = NWC24FOpen(&file, DLFilePath, NWC24_OPEN_NAND_RBUFF);
    if (result < 0) {
        return result;
    }

    result = ReadDlTaskEntry(pTask, id, &file);
    close = NWC24FClose(&file);

    return result != NWC24_OK ? result : close;
}

static inline NWC24Err CheckDlHeader(NWC24iDlHeader* pHeader) {
    if (pHeader->maxTasks == 0 && pHeader->maxSubTasks != 0) {
        NWC24File file;

        // Convert a header from an older library version
        pHeader->maxTasks = pHeader->maxSubTasks;
        if (pHeader->maxSubTasks > NWC24i_DL_SUBTASK_MAX) {
            pHeader->maxSubTasks = NWC24i_DL_SUBTASK_MAX;
        }

        if (NWC24FOpen(&file, DLFilePath, NWC24_OPEN_NAND_RW) >= 0) {
            WriteDlHeader(&file);
            NWC24FClose(&file);
        }
    }

    if (pHeader->maxTasks < 1 || pHeader->privateTasks < 1 ||
        pHeader->maxTasks < pHeader->privateTasks) {
        return NWC24_ERR_BROKEN;
    }

    return NWC24_OK;
}

static inline NWC24Err DeleteDlTaskForced(NWC24DlTask* pTask) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result = CheckDlTask(pTaskImpl);
    if (result != NWC24_OK) {
        return result;
    }

    result = DeleteDlTask(pTaskImpl);
    if (result < 0) {
        return result;
    }

    pTaskImpl->id = DL_ID_INVALID;
    return result;
}

static inline BOOL IsBefore(BOOL descending, s32 a, s32 b) {
    return descending ? b < a : a < b;
}

static inline NWC24Err InitIterateWork(NWC24DlIterateWork* pWork, u32 mode) {
    memset(pWork, 0, sizeof(NWC24DlIterateWork));

    if (!(mode & NWC24_DL_ITER_DESCENDING)) {
        pWork->value = 0x7FFFFFFF;
        pWork->lastValue = 0x80000001;
    } else {
        pWork->value = 0x80000001;
        pWork->lastValue = 0x7FFFFFFF;
    }

    pWork->mode = mode;
    pWork->lastId = -1;
    pWork->begin = TRUE;
    pWork->active = TRUE;
    return NWC24_OK;
}

NWC24Err NWC24InitDlTask(NWC24DlTask* pTask, NWC24DlType type) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    char homeDir[64] = {0};
    u32 titleIdHi;
    u32 titleIdLo;
    u32 value;

    // "/title/XXXXXXXX/YYYYYYYY/data"; both halves are parsed through one
    // temporary, as in the original (it decides titleIdHi's register).
    NANDGetHomeDir(homeDir);
    homeDir[15] = '\0';
    value = strtoul(&homeDir[7], NULL, 16);
    titleIdHi = value;
    homeDir[24] = '\0';
    value = strtoul(&homeDir[16], NULL, 16);
    titleIdLo = value;

    if (GetDlHeader() == NULL) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }

    if (pTask == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }

    if (type >= 4) {
        return NWC24_ERR_INVALID_VALUE;
    }

    memset(pTaskImpl, 0, sizeof(NWC24iDlTask));
    pTaskImpl->type = type;
    pTaskImpl->priority = DL_PRIORITY_DEFAULT;
    pTaskImpl->appId = NWC24GetAppId();
    pTaskImpl->groupId = NWC24GetGroupId();
    pTaskImpl->titleIdHi = titleIdHi;
    pTaskImpl->titleIdLo = titleIdLo;
    pTaskImpl->id = DL_ID_INVALID;
    pTaskImpl->count = 1;
    pTaskImpl->interval = DL_INTERVAL_DEFAULT;
    pTaskImpl->margin = DL_MARGIN_DEFAULT;

    if (HasFilename(type)) {
        strcpy(pTaskImpl->fileName, "content.bin");
    }

    if (CheckNewDlTask(pTaskImpl) < 0) {
        return NWC24_ERR_FATAL;
    }

    return NWC24_OK;
}

NWC24Err NWC24SetDlId(NWC24DlTask* pTask, NWC24DlId id) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTask(pTaskImpl);
    if (result != NWC24_OK) {
        return result;
    }

    if (id >= GetDlHeader()->maxTasks) {
        return NWC24_ERR_INVALID_VALUE;
    }

    pTaskImpl->id = id;
    return NWC24_OK;
}

NWC24Err NWC24GetDlId(NWC24DlTask* pTask, NWC24DlId* pId) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTask(pTaskImpl);
    if (result != NWC24_OK) {
        return result;
    }

    if (pId == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }

    if (pTaskImpl->id == DL_ID_INVALID) {
        return NWC24_ERR_INVALID_OPERATION;
    }

    *pId = pTaskImpl->id;
    return NWC24_OK;
}

NWC24Err NWC24SetDlPriority(NWC24DlTask* pTask, u8 priority) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTaskWritable(pTaskImpl);
    if (result != NWC24_OK) {
        return result;
    }

    pTaskImpl->priority = priority;
    return NWC24_OK;
}

NWC24Err NWC24SetDlInterval(NWC24DlTask* pTask, u16 interval) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTaskWritable(pTaskImpl);
    if (result != NWC24_OK) {
        return result;
    }

    if (!sDlRestrictionDisabled) {
        if (pTaskImpl->id >= GetDlHeader()->privateTasks &&
            !(pTaskImpl->flags & DL_FLAG_NO_LIMITS) &&
            (interval < DL_INTERVAL_MIN || interval > DL_INTERVAL_MAX)) {
            return NWC24_ERR_INVALID_VALUE;
        }
    }

    if (pTaskImpl->interval != interval) {
        pTaskImpl->interval = interval;

        if (pTaskImpl->id != DL_ID_INVALID) {
            UpdateNextTime(pTaskImpl);
        }
    }

    return NWC24_OK;
}

NWC24Err NWC24GetDlInterval(NWC24DlTask* pTask, u16* pInterval) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTask(pTaskImpl);
    if (result != NWC24_OK) {
        return result;
    }

    if (pInterval == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }

    *pInterval = pTaskImpl->interval;
    return NWC24_OK;
}

NWC24Err NWC24SetDlRetryMargin(NWC24DlTask* pTask, u16 margin) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTaskWritable(pTaskImpl);
    if (result != NWC24_OK) {
        return result;
    }

    if (!sDlRestrictionDisabled) {
        if (pTaskImpl->id >= GetDlHeader()->privateTasks &&
            (margin < 1 || margin > DL_MARGIN_MAX)) {
            return NWC24_ERR_INVALID_VALUE;
        }
    }

    pTaskImpl->margin = margin;
    return NWC24_OK;
}

NWC24Err NWC24SetDlUrl(NWC24DlTask* pTask, const char* pUrl) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTaskWritable(pTaskImpl);
    if (result != NWC24_OK) {
        return result;
    }

    result = CheckDlUrl(pUrl);
    if (result < 0) {
        return result;
    }

    if (!sDlRestrictionDisabled && (pTaskImpl->flags & DL_FLAG_SECURE_ONLY) &&
        strncmp(pUrl, "http://", 7) == 0) {
        return NWC24_ERR_INVALID_VALUE;
    }

    NWC24iStrLCpy(pTaskImpl->url, pUrl, sizeof(pTaskImpl->url));
    return NWC24_OK;
}

NWC24Err NWC24GetDlUrl(NWC24DlTask* pTask, char* pUrl, u32 len) {
    return GetDlUrl((NWC24iDlTask*)pTask, pUrl, len, FALSE, 0xFF);
}

NWC24Err NWC24SetDlFlags(NWC24DlTask* pTask, u32 flags) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTaskWritable(pTaskImpl);
    if (result != NWC24_OK) {
        return result;
    }

    if (!sDlRestrictionDisabled && (flags & DL_FLAG_SECURE_ONLY) &&
        strncmp(pTaskImpl->url, "http://", 7) == 0) {
        return NWC24_ERR_INVALID_VALUE;
    }

    switch (pTaskImpl->type) {
    case NWC24_DLTYPE_MULTIPART_V1:
    case NWC24_DLTYPE_OCTETSTREAM_V1:
        if (flags & DL_FLAG_V2_ONLY) {
            return NWC24_ERR_INVALID_OPERATION;
        }
        break;
    case NWC24_DLTYPE_MULTIPART_V2:
    case NWC24_DLTYPE_OCTETSTREAM_V2:
        break;
    }

    pTaskImpl->flags = flags;
    return NWC24_OK;
}

NWC24Err NWC24SetDlFilename(NWC24DlTask* pTask, const char* pFilename) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTaskWritable(pTaskImpl);
    if (result != NWC24_OK) {
        return result;
    }

    result = NWC24iCheckStringLength(pFilename, 1, NAND_MAX_PATH);
    if (result < 0) {
        return result;
    }

    if (!HasFilename(pTaskImpl->type)) {
        return NWC24_ERR_INVALID_OPERATION;
    }

    NWC24iStrLCpy(pTaskImpl->fileName, pFilename, NAND_MAX_PATH);
    return NWC24_OK;
}

static inline NWC24Err GetDlFilename(NWC24DlTask* pTask, char* pFilename,
                                     u32 len) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;
    u32 nameLen;

    result = CheckDlTask(pTaskImpl);
    if (result != NWC24_OK) {
        return result;
    }

    nameLen = strlen(pTaskImpl->fileName);
    if (nameLen == 0) {
        return NWC24_ERR_NOT_FOUND;
    }

    if (nameLen + 1 > len) {
        return NWC24_ERR_NOMEM;
    }

    strncpy(pFilename, pTaskImpl->fileName, nameLen);
    pFilename[nameLen] = '\0';
    return NWC24_OK;
}

NWC24Err NWC24GetDlFilename(NWC24DlTask* pTask, char* pFilename, u32 len) {
    return GetDlFilename(pTask, pFilename, len);
}

NWC24Err NWC24SetDlCount(NWC24DlTask* pTask, s16 count) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTaskWritable(pTaskImpl);
    if (result != NWC24_OK) {
        return result;
    }

    if (count < 1 || (pTaskImpl->id >= GetDlHeader()->privateTasks &&
                      !(pTaskImpl->flags & DL_FLAG_NO_LIMITS) &&
                      count > DL_COUNT_MAX)) {
        return NWC24_ERR_INVALID_VALUE;
    }

    pTaskImpl->count = count;
    return NWC24_OK;
}

NWC24Err NWC24GetDlLastUpdate(NWC24DlTask* pTask, s64* pTime) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTask(pTaskImpl);
    if (result != NWC24_OK) {
        return result;
    }

    if (pTime == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }

    *pTime = pTaskImpl->lastUpdate * 60;
    return NWC24_OK;
}

NWC24Err NWC24iCheckDlTask(NWC24DlTask* pTask) {
    NWC24Err result = CheckDlTask((NWC24iDlTask*)pTask);
    if (result != NWC24_OK) {
        return result;
    }

    return NWC24_OK;
}

NWC24Err NWC24IterateDlTask(NWC24DlId* pId, BOOL begin) {
    u16 id;

    if (GetDlHeader() == NULL) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }

    if (pId == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }

    if (begin) {
        *pId = 0;
    } else {
        (*pId)++;
    }

    if (*pId >= GetDlHeader()->maxTasks) {
        return NWC24_ERR_DONE;
    }

    id = *pId;
    while (id < GetLoopDlHeader()->maxTasks) {
        if (CheckDlEntryAvailable(id) >= 0) {
            *pId = id;
            return NWC24_OK;
        }

        id++;
    }

    return NWC24_ERR_DONE;
}

NWC24Err NWC24IterateDlTaskEx(NWC24DlIterateWork* pWork, NWC24DlId* pId) {
    u32 descending;
    BOOL found = FALSE;
    s32 (*getKey)(u16);
    NWC24DlId id;
    NWC24Err result;
    s32 value;

    if (!pWork->active) {
        return NWC24_ERR_INVALID_VALUE;
    }

    descending = pWork->mode >> 31;

    switch ((u16)pWork->mode) {
    case NWC24_DL_ITER_LAST_ACCESS:
        getKey = GetDlLastAccess;
        break;
    case NWC24_DL_ITER_NEXT_TIME:
        getKey = GetDlNextTime;
        break;
    case NWC24_DL_ITER_PRIORITY:
        getKey = GetDlPriority;
        break;
    default:
        return NWC24_ERR_INVALID_VALUE;
    }

    if (!pWork->begin) {
        // Tasks that share the current value, after the last returned ID
        for (result = NWC24IterateDlTask(&id, TRUE); result >= 0;
             result = NWC24IterateDlTask(&id, FALSE)) {
            value = getKey(id);

            if (value == pWork->value && pWork->lastId < id) {
                *pId = id;
                pWork->value = value;
                pWork->lastId = id;
                pWork->lastValue = pWork->value;
                return NWC24_OK;
            }
        }
    } else {
        pWork->begin = FALSE;
    }

    if (!(pWork->mode & NWC24_DL_ITER_DESCENDING)) {
        pWork->value = 0x7FFFFFFF;
    } else {
        pWork->value = 0x80000001;
    }

    for (result = NWC24IterateDlTask(&id, TRUE); result >= 0;
         result = NWC24IterateDlTask(&id, FALSE)) {
        value = getKey(id);

        if (IsBefore(descending, pWork->lastValue, value) &&
            IsBefore(descending, value, pWork->value)) {
            found = TRUE;
            *pId = id;
            pWork->value = value;
            pWork->lastId = id;
        }
    }

    if (found) {
        pWork->lastValue = pWork->value;
        return NWC24_OK;
    }

    pWork->active = FALSE;
    return NWC24_ERR_DONE;
}

static inline NWC24Err AdvanceSubTask(NWC24iDlTask* pTask) {
    NWC24Err result;

    while ((result = CheckSubTask(pTask, pTask->subTaskCounter)) ==
           NWC24_ERR_DISABLED) {
        pTask->subTaskCounter =
            (pTask->subTaskCounter + 1) & (NWC24i_DL_SUBTASK_MAX - 1);
    }

    return result;
}

// Moves an increment task on to its next enabled subtask, then saves it. Both
// results leave through this function's own return value, so where
// NWC24UpdateDlTask is inlined (AddDlTask) the error path and the WriteDlTask
// result share r3, as in the original.
static inline NWC24Err CommitDlTask(NWC24iDlTask* pTask) {
    NWC24Err result;

    if (pTask->subTaskType == NWC24_DL_STTYPE_INCREMENT &&
        (result = AdvanceSubTask(pTask)) < 0) {
        return result;
    }

    return WriteDlTask(pTask);
}

NWC24Err NWC24UpdateDlTask(NWC24DlTask* pTask) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    result = CheckDlTaskWritable(pTaskImpl);
    if (result != NWC24_OK) {
        return result;
    }

    if (pTaskImpl->id == DL_ID_INVALID ||
        pTaskImpl->id >= GetDlHeader()->maxTasks) {
        return NWC24_ERR_INVALID_VALUE;
    }

    result = UpdateLastAccess(pTaskImpl);
    if (result < 0) {
        return result;
    }

    ClearDlError(pTaskImpl);
    return CommitDlTask(pTaskImpl);
}

NWC24Err NWC24DeleteDlTask(NWC24DlTask* pTask) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    if (NWC24GetAppId() != NWC24i_APP_ID_IPL) {
        result = CheckDlTaskWritable(pTaskImpl);
        if (result != NWC24_OK) {
            return result;
        }
    }

    return DeleteDlTaskForced(pTask);
}

NWC24Err NWC24AddDlTask(NWC24DlTask* pTask) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;
    u16 privateTasks;
    u16 maxTasks;

    result = CheckDlHeader(GetDlHeader());
    if (result < 0) {
        return result;
    }

    privateTasks = GetDlHeader()->privateTasks;
    maxTasks = GetDlHeader()->maxTasks;
    result = AddDlTask(pTaskImpl, privateTasks, maxTasks);
    if (result >= 0) {
        result = UpdateNextTime(pTaskImpl);
    }

    return result;
}

NWC24Err NWC24GetDlTask(NWC24DlTask* pTask, NWC24DlId id) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result;

    if (GetDlHeader() == NULL) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }

    result = CheckDlEntryAvailable(id);
    if (result < 0) {
        return result;
    }

    return LoadDlTask(pTaskImpl, id);
}

NWC24Err NWC24GetDlNextTime(NWC24DlTask* pTask, s64* pTime) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24iDlEntry* pEntry;
    NWC24Err result;
    s64 now;

    result = CheckDlTask(pTaskImpl);
    if (result != NWC24_OK) {
        return result;
    }

    pEntry = GetDlEntry(pTaskImpl->id);
    result = NWC24iGetUniversalTime(&now);

    if (pEntry == NULL) {
        return NWC24_ERR_FATAL;
    }

    // Pull back next times that are too far in the future
    if (result >= 0 && pEntry->nextTime > now + 7 * 24 * 60 * 60) {
        pEntry->nextTime = now / 60 + DL_INTERVAL_DEFAULT;
    }

    if (pTime == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }

    *pTime = pEntry->nextTime * 60;
    return NWC24_OK;
}

NWC24Err NWC24PurgeOldestDlTask(void) {
    NWC24DlIterateWork work;
    NWC24DlTask task;
    NWC24DlId id;
    NWC24Err result;

    result = InitIterateWork(&work, NWC24_DL_ITER_LAST_ACCESS);
    if (result < 0) {
        return result;
    }

    // Find the least recently used public task
    while ((result = NWC24IterateDlTaskEx(&work, &id)) == NWC24_OK) {
        if (id >= GetDlHeader()->privateTasks) {
            break;
        }
    }

    if (result >= 0) {
        result = NWC24GetDlTask(&task, id);
        if (result < 0) {
            return result;
        }

        result = DeleteDlTaskForced(&task);
        if (result < 0) {
            return result;
        }
    } else if (result == NWC24_ERR_DONE) {
        result = NWC24_ERR_FAILED;
    }

    return result;
}

// Defined here rather than as literals: these come before "wc24dl.vff" and
// "/%s" in the original data (literals are laid out in code order instead)
static char sDlDataDirFormat[] = "/title/%08x/%08x/data";
static char sDlSubTaskSuffix[] = ".%02d";

static inline NWC24Err GetDlDataDir(NWC24DlTask* pTask, char* pDir, u32 len) {
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)pTask;
    NWC24Err result = CheckDlTask(pTaskImpl);
    if (result != NWC24_OK) {
        return result;
    }

    if (pTaskImpl->appId == 0) {
        return NWC24_ERR_INVALID_VALUE;
    }

    // "/title/XXXXXXXX/YYYYYYYY/data"
    if (len < 30) {
        return NWC24_ERR_NOMEM;
    }

    snprintf(pDir, len, sDlDataDirFormat, pTaskImpl->titleIdHi,
             pTaskImpl->titleIdLo);
    return NWC24_OK;
}

static inline NWC24Err GetDlDataPath(NWC24iDlTask* pTask, char* pPath, u32 len,
                                     const char* pName) {
    NWC24Err result;
    u32 dirLen;

    result = CheckDlTask(pTask);
    if (result != NWC24_OK) {
        return result;
    }

    if (pTask->appId == 0) {
        return NWC24_ERR_INVALID_VALUE;
    }

    result = GetDlDataDir((NWC24DlTask*)pTask, pPath, len);
    if (result < 0) {
        return result;
    }

    dirLen = strlen(pPath);
    snprintf(pPath + dirLen, len - dirLen, "/%s", pName);
    return NWC24_OK;
}

NWC24Err NWC24GetDlVfName(NWC24DlTask* pTask, char* pName, u32 len) {
    return GetDlDataPath((NWC24iDlTask*)pTask, pName, len, DL_VF_FILENAME);
}

NWC24Err NWC24CreateDlVf(NWC24DlTask* pTask, u32 size) {
    const char* pName;
    NWC24Err result;

    result = CheckDlTaskWritable((NWC24iDlTask*)pTask);
    if (result != NWC24_OK) {
        return result;
    }

    if (size < DL_VF_SIZE_MIN) {
        return NWC24_ERR_INVALID_VALUE;
    }

    pName = DL_VF_FILENAME;
    GetDlDataPath((NWC24iDlTask*)pTask, sDlVfPath, sizeof(sDlVfPath), pName);
    return NWC24CreateVF(sDlVfPath, size);
}

NWC24Err NWC24iOpenDlTaskList(void) {
    NWC24Err result;

    result = NWC24iLoadDlHeader();
    if (result >= 0) {
        NWC24iSynchronizeRtcCounter(FALSE);

        result = NWC24iCheckHeaderConsistency(GetDlHeader(), FALSE);
        if (result >= 0) {
            return NWC24_OK;
        }
    }

    return result;
}

NWC24Err NWC24iCloseDlTaskList(void) {
    return NWC24_OK;
}

NWC24Err NWC24iCheckHeaderConsistency(NWC24iDlHeader* pHeader, BOOL clear) {
    NWC24DlTask task;
    NWC24iDlTask* pTaskImpl = (NWC24iDlTask*)&task;
    u16 i;

    for (i = 0; i < pHeader->maxTasks; i++) {
        if (CheckDlEntryAvailable(i) == NWC24_OK && clear) {
            if (NWC24GetDlTask(&task, i) < 0) {
                DeleteDlTaskForced(&task);
            } else if (i >= GetDlHeader()->privateTasks &&
                       pTaskImpl->count == 0) {
                DeleteDlTaskForced(&task);
            }
        }
    }

    return NWC24_OK;
}

NWC24Err NWC24iLoadDlHeader(void) {
    NWC24File file;
    NWC24Err result;
    NWC24Err close;
    u32 length = 0;

    result = NWC24FOpen(&file, DLFilePath, NWC24_OPEN_NAND_R);
    if (result < 0) {
        return result;
    }

    result = ReadDlHeader(&file);
    if (result < 0) {
        return result;
    }

    result = NWC24FGetLength(&file, &length);
    if (result >= 0) {
        result = CheckDlHeader(GetDlHeader());
    }

    close = NWC24FClose(&file);
    return result != NWC24_OK ? result : close;
}

static NWC24Err GetDlUrl(NWC24iDlTask* pTask, char* pUrl, u32 len,
                         BOOL subTask, u8 index) {
    NWC24Err result;
    u32 urlLen;

    result = CheckDlTask(pTask);
    if (result != NWC24_OK) {
        return result;
    }

    urlLen = strlen(pTask->url);
    if (urlLen == 0) {
        return NWC24_ERR_NOT_FOUND;
    }

    if (urlLen < 8) {
        return NWC24_ERR_FAILED;
    }

    // Sub-task URLs get a ".NN" suffix
    if (urlLen + (subTask ? 3 : 0) + 1 > len) {
        return NWC24_ERR_NOMEM;
    }

    strncpy(pUrl, pTask->url, urlLen);
    pUrl[urlLen] = '\0';

    if (subTask) {
        snprintf(pUrl + urlLen, len - urlLen, sDlSubTaskSuffix, index);
    }

    return NWC24_OK;
}

static NWC24Err WriteDlTask(NWC24iDlTask* pTask) {
    NWC24File file;
    NWC24Err result;
    NWC24Err close;

    result = NWC24FOpen(&file, DLFilePath, NWC24_OPEN_NAND_RW);
    if (result < 0) {
        return result;
    }

    result = WriteDlTaskEntry(pTask, &file);
    if (result >= 0) {
        result = UpdateDlEntry(pTask);
        if (result >= 0) {
            result = WriteDlHeader(&file);
        }
    }

    close = NWC24FClose(&file);
    return result != NWC24_OK ? result : close;
}

static inline NWC24Err FindFreeDlId(NWC24iDlTask* pTask, u32 min, u32 max) {
    NWC24iDlHeader* pHeader = GetDlHeader();
    u16 id;

    if (pTask == NULL || min > max || min >= pHeader->maxTasks ||
        max > pHeader->maxTasks) {
        return NWC24_ERR_INVALID_VALUE;
    }

    for (id = min; id < max; id++) {
        if (GetDlEntry(id)->app == 0) {
            pTask->id = id;
            return NWC24_OK;
        }
    }

    return NWC24_ERR_FULL;
}

static NWC24Err AddDlTask(NWC24iDlTask* pTask, u16 min, u16 max) {
    NWC24Err result;

    result = CheckDlTaskWritable(pTask);
    if (result != NWC24_OK) {
        return result;
    }

    result = CheckDlTaskParam(pTask);
    if (result < 0) {
        return result;
    }

    while (TRUE) {
        if (pTask->id != DL_ID_INVALID) {
            return NWC24UpdateDlTask((NWC24DlTask*)pTask);
        }

        result = FindFreeDlId(pTask, min, max);
        if (result == NWC24_ERR_FULL) {
            result = NWC24PurgeOldestDlTask();
            if (result < 0) {
                return result;
            }
        } else if (result < 0) {
            return result;
        }
    }
}

static NWC24Err DeleteDlTask(NWC24iDlTask* pTask) {
    NWC24File file;
    NWC24Err result;
    NWC24Err close;

    result = NWC24FOpen(&file, DLFilePath, NWC24_OPEN_NAND_RW);
    if (result < 0) {
        return result;
    }

    result = ClearDlTaskEntry(pTask->id, &file);
    if (result >= 0) {
        InitDlEntry(pTask->id);
        result = WriteDlHeader(&file);
    }

    close = NWC24FClose(&file);
    return result != NWC24_OK ? result : close;
}

static s32 GetDlLastAccess(u16 id) {
    return GetDlEntry(id)->lastAccess;
}

static s32 GetDlNextTime(u16 id) {
    return GetDlEntry(id)->nextTime;
}

static s32 GetDlPriority(u16 id) {
    return GetDlEntry(id)->priority;
}
