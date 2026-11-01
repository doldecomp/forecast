#ifndef RVL_SDK_SO_BASIC_H
#define RVL_SDK_SO_BASIC_H
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

// Option level and options of SOGetInterfaceOpt.
// Values from the Mario Kart Wii decompilation (doldecomp/mkw, CC0).
#define SO_SOL_CONFIG 0xFFFE

#define SO_CONFIG_FILTER_INPUT 0x1001
#define SO_CONFIG_FILTER_OUTPUT 0x1002
#define SO_CONFIG_ERROR 0x1003
#define SO_CONFIG_MAC_ADDRESS 0x1004
#define SO_CONFIG_LINK_STATE 0x1005
#define SO_CONFIG_INTERFACE_STATISTICS 0x1006
#define SO_CONFIG_MUTE 0x1007

typedef struct SOInterface {
    s32 type; // at 0x0
    BOOL up;  // at 0x4
    s32 err;  // at 0x8
} SOInterface;

u32 SOGetHostID(void);
s32 SOGetInterfaceOpt(SOInterface* interface, s32 level, s32 optname,
                      void* optval, s32* optlen);

#ifdef __cplusplus
}
#endif
#endif
