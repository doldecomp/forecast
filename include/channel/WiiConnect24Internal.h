#ifndef CHANNEL_WII_CONNECT24_INTERNAL_H
#define CHANNEL_WII_CONNECT24_INTERNAL_H
#include <types.h>

#include <revolution/MEM.h>
#include <revolution/OS.h>

#include <channel/WiiConnect24.h>

extern OSMessageQueue gWC24Queue;
extern OSMessage gWC24QueueBuffer[16];
extern OSThread gWC24Thread;
extern u8 gWC24ThreadStack[];
extern u8 gWC24FileBuffer[];
extern volatile u8 gWC24DownloadComplete;
extern volatile u8 gWC24NetInitialized;
extern volatile u8 gWC24ShutdownFlag1;
extern volatile u8 gWC24ShutdownFlag2;
extern volatile s32 gWC24DownloadIndex;
extern volatile s32 gWC24QueueIndex;
extern volatile s32 gWC24TaskCount;
extern void* gWC24HeapBlock;
extern MEMiHeapHead* gWC24Heap;

// The SO block is not split yet; keep the dependency declarations together.
#ifdef __cplusplus
extern "C" {
#endif
s32 SOInit(void* init);
s32 SOStartup(void);
s32 NETGetStartupErrorCode(s32 soErr);
#ifdef __cplusplus
}
#endif

void* DownloaderThread(void* pArg);
void* WC24Alloc(u32 unused, s32 size);
void WC24Free(void* unused, void* pBlock, s32 size);

#endif
