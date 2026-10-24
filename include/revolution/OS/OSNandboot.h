#ifndef RVL_SDK_OS_NANDBOOT_H
#define RVL_SDK_OS_NANDBOOT_H
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct OSNandbootInfo {
    u32 checksum;
    u32 argsoff;
    u8 unk1;
    u8 unk2;
    u8 apptype;
    u8 titletype;
    u32 launchcode;
    u32 unknown[2];
    u64 launcher;
    u8 argbuf[0x1000];
} OSNandbootInfo;

extern char __OSNandbootInfoPath[];

BOOL __OSCreateNandbootInfo(void);
BOOL __OSWriteNandbootInfo(OSNandbootInfo* info);

#ifdef __cplusplus
}
#endif
#endif
