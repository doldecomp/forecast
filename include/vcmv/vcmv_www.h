#ifndef CHANNEL_VCMV_WWW_H
#define CHANNEL_VCMV_WWW_H

#include <types.h>
#include <revolution/OS/OSTime.h>
#include <revolution/MEM.h>

BOOL GetCurrentCalendarTime(OSCalendarTime* time);

void wwwUnlinkedFunction(void);

struct WWWNandFile;
struct WWWLZState;

struct WWWSettings {
    u32 magic;          // at 0x0
    u8 pad4;            // at 0x4
    u8 flag5;           // at 0x5
    u8 stdAspect;       // at 0x6
    u8 dtvAspect;       // at 0x7
    u32 magic2;         // at 0x8
    u8 version;         // at 0xC
    u8 padD[0x13];      // to 0x20
};

extern WWWSettings wwwSettingsBuf;
extern u8 wwwAspectRatio;
extern u8* wwwAspectPtr;
extern bool wwwStarted;
extern bool wwwLibLoaded;
extern MEMAllocator* wwwMem1Allocator;
extern MEMAllocator* wwwMem2Allocator;

void wwwAddFonts(void);
s32 wwwCreateParentDirs(WWWNandFile* file);
s32 wwwWriteNandFile(WWWNandFile* file);
void wwwSaveSettings(void);
void wwwLoadSettings(void);
void wwwLZRead(u8* dst, WWWLZState* state, u32 n);
s32 wwwLoadLibrary(void);
s32 wwwLoadFonts(void);
s32 wwwInitialize(void);
void wwwShutdown(void);

#endif
