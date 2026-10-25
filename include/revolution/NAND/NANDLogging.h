#ifndef RVL_SDK_NAND_LOGGING_H
#define RVL_SDK_NAND_LOGGING_H
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef void (*NANDLoggingCallback)(BOOL success);

BOOL NANDLoggingAddMessageAsync(NANDLoggingCallback callback, const char* fmt,
                                ...);

#ifdef __cplusplus
}
#endif
#endif
