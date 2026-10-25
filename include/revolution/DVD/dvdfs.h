#ifndef RVL_SDK_DVD_FS_H
#define RVL_SDK_DVD_FS_H
#include <types.h>

#include <revolution/DVD/dvd.h>

#include <revolution/OS.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct DVDDir {
    u32 entryNum; // at 0x0
    u32 location; // at 0x4
    u32 next;     // at 0x8
} DVDDir;

typedef struct DVDDirEntry {
    u32 entryNum; // at 0x0
    BOOL isDir;   // at 0x4
    char* name;   // at 0x8
} DVDDirEntry;

extern OSThreadQueue __DVDThreadQueue;
extern BOOL __DVDLongFileNameFlag;

void __DVDFSInit(void);
s32 DVDConvertPathToEntrynum(const char* path);
BOOL DVDFastOpen(s32 entrynum, DVDFileInfo* info);
BOOL DVDOpen(const char* path, DVDFileInfo* info);
BOOL DVDClose(DVDFileInfo* info);
BOOL DVDGetCurrentDir(char* buffer, u32 maxlen);
BOOL DVDReadAsyncPrio(DVDFileInfo* info, void* dst, s32 size, s32 offset,
                      DVDAsyncCallback callback, s32 prio);
s32 DVDReadPrio(DVDFileInfo* info, void* dst, s32 size, s32 offset, s32 prio);

#ifdef __cplusplus
}
#endif
#endif
