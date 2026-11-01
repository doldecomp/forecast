#ifndef RVL_SDK_NET_ERROR_CODE_H
#define RVL_SDK_NET_ERROR_CODE_H
#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

int NETiGetConnectionTypeFromConfigList(u32 wired, u32 wireless, u32 wirelessAoss);
s32 NETGetStartupErrorCode(s32 result);

#ifdef __cplusplus
}
#endif
#endif
