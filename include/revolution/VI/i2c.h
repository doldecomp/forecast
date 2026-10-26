#ifndef RVL_SDK_VI_I2C_H
#define RVL_SDK_VI_I2C_H
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

// I2C slave address of the video encoder
#define VI_I2C_ADDR_VDAC 0xE0

void WaitMicroTime(s32 usec);
s32 __VISendI2CData(u8 slaveAddr, u8* data, s32 size);

#ifdef __cplusplus
}
#endif
#endif
