#ifndef RVL_SDK_NCD_SYSTEM_H
#define RVL_SDK_NCD_SYSTEM_H
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef s32 NCDErr;

// Result codes (names unofficial)
enum {
    NCD_RESULT_SUCCESS = 0,
    NCD_RESULT_FATAL = -1,
    NCD_RESULT_IPC_ERROR = -2,
    NCD_RESULT_NO_THREAD = -5,
    NCD_RESULT_INPROGRESS = -8
};

// NCDGetLinkStatus results (names unofficial)
enum {
    NCD_LINKSTATUS_WORKING = 1,
    NCD_LINKSTATUS_NONE = 2,
    NCD_LINKSTATUS_WIRED = 3,
    NCD_LINKSTATUS_WIRELESS_DOWN = 4,
    NCD_LINKSTATUS_WIRELESS_UP = 5
};

NCDErr NCDGetLinkStatus(void);
NCDErr NCDiGetEnabledConfigList(u32* wired, u32* wireless, u32* usbap);

#ifdef __cplusplus
}
#endif
#endif
