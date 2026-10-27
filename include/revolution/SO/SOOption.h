#ifndef RVL_SDK_SO_OPTION_H
#define RVL_SDK_SO_OPTION_H
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

#define SO_SOL_CONFIG 0xFFFE

#define SO_CONFIG_FILTER_INPUT 0x1001
#define SO_CONFIG_FILTER_OUTPUT 0x1002
#define SO_CONFIG_ERROR 0x1003

typedef struct SOInterface SOInterface;

int SOGetInterfaceOpt(SOInterface* interface, int level, int optname,
                      void* optval, int* optlen);

#ifdef __cplusplus
}
#endif
#endif
