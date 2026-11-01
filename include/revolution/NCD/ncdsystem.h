#ifndef RVL_SDK_NCD_SYSTEM_H
#define RVL_SDK_NCD_SYSTEM_H
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef s32 NCDErr;

NCDErr NCDGetLinkStatus(void);
NCDErr NCDiGetEnabledConfigList(u32* list0, u32* list1, u32* list2);

#ifdef __cplusplus
}
#endif
#endif
