#ifndef RVL_SDK_NET_ERROR_CODE_H
#define RVL_SDK_NET_ERROR_CODE_H
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

s32 NETiGetConnectionTypeFromConfigList(u32 wired, u32 wireless, u32 usbap);
s32 NETGetStartupErrorCode(s32 soErr);

#ifdef __cplusplus
}
#endif
#endif
