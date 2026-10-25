#include <revolution/DVD.h>
#include <revolution/NWC24.h>
#include <revolution/NWC24/NWC24Internal.h>
#include <revolution/OS.h>

#define CONFIG_MAGIC FOURCC('W', 'c', 'C', 'f')
#define CONFIG_VERSION 8
#define CONFIG_MAX_ID_COUNT 32

#define COMPANY_CODE_NINTENDO 01

static NWC24iConfig* config = NULL;
static BOOL ConfigModified = FALSE;

static const char* MBoxDir = "/shared2/wc24/mbox";
static const char* ConfigFile = "/shared2/wc24/nwc24msg.cfg";
static const char* CfgBakFile = "/shared2/wc24/nwc24msg.cbk";

static u16 VirtualGroupId = COMPANY_CODE_NINTENDO;

static u32 GetConfigCheckSum(void);
static NWC24Err CheckConfig(void) DECOMP_DONT_INLINE;
static NWC24Err GenerateUserId(u64* pId);
static void CacheUserIdToLoMem(u64 id);

// Not in the DOL (dead-stripped): inline, so that no copy is emitted
inline NWC24Err NWC24GetMyUserId(u64* pId) {
    NWC24Err scdErr;
    NWC24Err result;

    result = NWC24_OK;

    if (NWC24IsMsgLibOpened() || NWC24IsMsgLibOpenedByTool()) {
        *pId = config->userId;
    } else {
        scdErr = NWC24SuspendScheduler();
        if (scdErr < 0) {
            return scdErr;
        }

        result = NWC24GenerateNewUserId(pId);
        if (result == NWC24_ERR_ID_GENERATED ||
            result == NWC24_ERR_ID_REGISTERED) {
            result = NWC24_OK;
        }

        scdErr = NWC24ResumeScheduler();
        if (scdErr < 0) {
            result = result != NWC24_OK ? result : scdErr;
        }
    }

    return result;
}

// Not in the DOL (dead-stripped): inline, so that no copy is emitted
inline NWC24Err NWC24GenerateNewUserId(u64* pId) {
    NWC24Err open;
    NWC24Err close;

    open = NWC24BlockOpenMsgLib(TRUE);
    if (open < 0) {
        return open;
    }

    open = GenerateUserId(pId);
    close = NWC24BlockOpenMsgLib(FALSE);

    return open >= 0 ? close : open;
}

NWC24Err NWC24iConfigOpen(void) {
    NWC24Err result;

    ConfigModified = FALSE;
    config = (NWC24iConfig*)NWC24WorkP->config;

    result = NWC24iConfigReload();
    if (result != NWC24_ERR_FILE_NOEXISTS && result == NWC24_OK) {
        CacheUserIdToLoMem(config->userId);
    }

    return result;
}

NWC24Err NWC24iConfigReload(void) {
    NWC24Err result;
    NWC24Err close;
    NWC24File file;

    if (NWC24WorkP == NULL) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }

    result = NWC24FOpen(&file, ConfigFile, NWC24_OPEN_NAND_R);

    if (result == NWC24_OK) {
        result = NWC24FRead(config, sizeof(NWC24iConfig), &file);
        close = NWC24FClose(&file);
        result = result != NWC24_OK ? result : close;
    }

    if (result == NWC24_OK) {
        result = CheckConfig();

        if (result == NWC24_OK) {
            ConfigModified = FALSE;
            return result;
        }
    }

    result = NWC24FOpen(&file, CfgBakFile, NWC24_OPEN_NAND_R);

    if (result == NWC24_OK) {
        result = NWC24FRead(config, sizeof(NWC24iConfig), &file);
        close = NWC24FClose(&file);
        result = result != NWC24_OK ? result : close;
    }

    if (result == NWC24_OK) {
        result = CheckConfig();

        if (result == NWC24_OK) {
            ConfigModified = TRUE;
        }
    }

    return result;
}

NWC24Err NWC24iConfigFlush(void) {
    NWC24Err result;
    NWC24Err close;
    NWC24File file;

    if (NWC24WorkP == NULL) {
        return NWC24_ERR_LIB_NOT_OPENED;
    }

    if (!ConfigModified) {
        return NWC24_OK;
    }

    config->checksum = GetConfigCheckSum();

    result = NWC24FOpen(&file, ConfigFile, NWC24_OPEN_NAND_W);

    if (result == NWC24_OK) {
        result = NWC24FWrite(config, sizeof(NWC24iConfig), &file);
        close = NWC24FClose(&file);
        result = result != NWC24_OK ? result : close;

        ConfigModified = FALSE;
    }

    if (result != NWC24_OK) {
        return result;
    }

    result = NWC24FOpen(&file, CfgBakFile, NWC24_OPEN_NAND_W);

    if (result == NWC24_OK) {
        result = NWC24FWrite(config, sizeof(NWC24iConfig), &file);
        close = NWC24FClose(&file);
        result = result != NWC24_OK ? result : close;

        ConfigModified = FALSE;
    }

    return result;
}

// Not in the DOL (dead-stripped): inline, so that no copy is emitted
inline const char* NWC24GetAccountDomain(void) {
    return config->acctDomain;
}

const char* NWC24GetMBoxDir(void) {
    OSIOSRev rev;
    __OSGetIOSRev(&rev);

    // Debug: "Firmware older than 0.7.6 is not supported"
    if (rev.idLo == 0 &&
        (rev.verMajor == 7 && rev.verMinor == 0 || rev.verMajor < 7)) {
        // clang-format off
#line 695
    OS_ERROR("stopped.");
        // clang-format on
    }

    return MBoxDir;
}

u32 NWC24GetAppId(void) {
    u32 appId = *(u32*)OSGetAppGamename();

    if (appId == 0 && OSGetAppType() == 0) {
        appId = *(u32*)DVDGetCurrentDiskID()->game;
    }

    return appId;
}

u16 NWC24GetGroupId(void) {
    NANDStatus stat;
    u16 groupId;
    char* pHomeDir;

    groupId = VirtualGroupId;

    switch (OSGetAppType()) {
    case OS_APP_TYPE_IPL: {
        groupId = COMPANY_CODE_NINTENDO;
        break;
    }

    case OS_APP_TYPE_DVD: {
        groupId = *(u16*)DVDGetCurrentDiskID()->company;
        break;
    }

    case OS_APP_TYPE_CHANNEL: {
        pHomeDir = NWC24WorkP->pathWork;

        if (NANDGetHomeDir(pHomeDir) == NAND_RESULT_OK) {
            if (NANDGetStatus(pHomeDir, &stat) == NAND_RESULT_OK) {
                groupId = stat.groupId;
            }
        }
        break;
    }
    }

    return groupId;
}

NWC24Err NWC24GetIdCreationStage(NWC24IDCreationStage* pStage) {
    *pStage = (NWC24IDCreationStage)config->createStage;
    return NWC24_OK;
}

static u32 GetConfigCheckSum(void) {
    u32 i;
    u32 csum;
    u32* pData;
    u32 dataWords;

    csum = 0;
    pData = (u32*)config;
    dataWords = (sizeof(NWC24iConfig) - sizeof(config->checksum)) / sizeof(u32);

    for (i = 0; i < dataWords; i++) {
        csum += *pData++;
    }

    return csum;
}

static NWC24Err CheckConfig(void) {
    if (config->magic != CONFIG_MAGIC) {
        return NWC24_ERR_BROKEN;
    }

    if (config->checksum != GetConfigCheckSum()) {
        return NWC24_ERR_BROKEN;
    }

    if (config->createCount >= CONFIG_MAX_ID_COUNT) {
        return NWC24_ERR_BROKEN;
    }

    if (config->version != CONFIG_VERSION) {
        return NWC24_ERR_VER_MISMATCH;
    }

    return NWC24_OK;
}

static NWC24Err GenerateUserId(u64* pId) {
    NWC24Err result;
    u32 dummy = 0;

    if (pId == NULL) {
        return NWC24_ERR_INVALID_VALUE;
    }

    *pId = 9999999999999999;
    result = NWC24iRequestGenerateUserId(pId, &dummy);
    CacheUserIdToLoMem(*pId);
    return result;
}

static void CacheUserIdToLoMem(u64 id) {
    OS_NWC24_USER_ID = id;
    DCStoreRange(OS_NWC24_USER_ID_BUFFER, sizeof(OS_NWC24_USER_ID_BUFFER));
}
