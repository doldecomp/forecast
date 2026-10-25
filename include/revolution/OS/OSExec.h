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

// MEM1 while the OS boots another program: the apploader is loaded at
// OS_APPLOADER_ADDR and the reboot code at OS_REBOOT_ADDR, with the arena
// between them.
#define OS_APPLOADER_ADDR ((void*)0x81200000)
#define OS_EXEC_ARENA_LO ((void*)0x81280000)
#define OS_EXEC_ARENA_HI ((void*)0x812F0000)
#define OS_REBOOT_ADDR ((void*)0x81330000)

extern BOOL __OSInReboot;
extern u32 __OSNextPartitionType;

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
