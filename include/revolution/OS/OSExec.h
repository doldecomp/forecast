#ifndef RVL_SDK_OS_EXEC_H
#define RVL_SDK_OS_EXEC_H
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSExecParams {
    UNKWORD WORD_0x0;    // at 0x0
    UNKWORD WORD_0x4;    // at 0x4
    u32 bootDol;         // at 0x8
    void* regionStart;   // at 0xC
    void* regionEnd;     // at 0x10
    BOOL argsUseDefault; // at 0x14
    void* argsAddr;      // at 0x18
} OSExecParams;

extern BOOL __OSInReboot;
extern u32 __OSLaunchPartitionType;

void __OSGetExecParams(OSExecParams* out);
void __OSLaunchMenu(void);
void __OSRelaunchTitle(void);
void __OSReturnToMenul(u32 launchCode, const char* arg0, ...);
void __OSBootDolSimple(u32 dolOffset, u32 restartCode, void* regionStart,
                       void* regionEnd, BOOL argsUseDefault, s32 argc,
                       char** argv);
void __OSBootDol(u32 dolOffset, u32 restartCode, const char** argv);

#ifdef __cplusplus
}
#endif
#endif
